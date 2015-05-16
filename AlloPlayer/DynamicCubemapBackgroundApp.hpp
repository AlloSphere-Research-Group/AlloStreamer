#include <alloutil/al_OmniApp.hpp>
#include <SOIL.h>

#include "H264RawPixelsSink.h"

#define QUOTE(x) #x
#define STR(x) QUOTE(x)

struct DynamicCubemapBackgroundApp : al::OmniApp {
    al::Mesh cube, sphere;
    al::Light light;
    al_sec now;
    std::vector<H264RawPixelsSink*> sinks;
    boost::mutex sinkMutex;
    SwsContext* resizeCtx;

  DynamicCubemapBackgroundApp() {
      nav().smooth(0.8);

      // set up cube
      cube.color(1,1,1,1);
      cube.primitive(al::Graphics::TRIANGLES);
      addCube(cube);
      for (int i = 0; i < cube.vertices().size(); ++i) {
          float f = (float)i / cube.vertices().size();
          cube.color(al::Color(al::HSV(f, 1 - f, 1), 1));
      }
      cube.generateNormals();
      
      // set up sphere
      sphere.primitive(al::Graphics::TRIANGLES);
      addSphere(sphere, 1.0, 32, 32);
      for (int i = 0; i < sphere.vertices().size(); ++i) {
          float f = (float)i / sphere.vertices().size();
          sphere.color(al::Color(al::HSV(f, 1 - f, 1), 1));
      }
      sphere.generateNormals();
      
      // set up light
      light.ambient(al::Color(0.4, 0.4, 0.4, 1.0));
      light.pos(5, 5, 5);
      
      resizeCtx = nullptr;
  }

  virtual ~DynamicCubemapBackgroundApp() {}

  bool onCreate() {
      
      std::cout << "OpenGL version: " << glGetString(GL_VERSION) << ", GLSL version " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
      return OmniApp::onCreate();
  }
    
    bool onFrame() {
        now = al::MainLoop::now();
        return OmniApp::onFrame();
    }

  void onDraw(al::Graphics& gl){
      
      int face = mOmni.face();
      int resolution = mOmni.resolution();
      
      {
          // get next frame
          boost::mutex::scoped_lock lock(sinkMutex);
          
          if (sinks.size() > face)
          {
              AVFrame* frame = sinks[face]->getCurrentFrame();
              
              if (frame)
              {
                  if (!resizeCtx)
                  {
                      // setup resizer for received frames
                      resizeCtx = sws_getContext(
                                     frame->width, frame->height, (AVPixelFormat)frame->format,
                                     resolution, resolution, AV_PIX_FMT_RGB24,
                                     SWS_BICUBIC, NULL, NULL, NULL);
                  }
                  
                  AVFrame* resizedFrame = av_frame_alloc();
                  if (!resizedFrame)
                  {
                      fprintf(stderr, "Could not allocate video frame\n");
                      exit(1);
                  }
                  resizedFrame->format = AV_PIX_FMT_RGB24;
                  resizedFrame->width = resolution;
                  resizedFrame->height = resolution;
                  
                  if (av_image_alloc(resizedFrame->data, resizedFrame->linesize, resizedFrame->width, resizedFrame->height,
                                     (AVPixelFormat)resizedFrame->format, 32) < 0)
                  {
                      fprintf(stderr, "Could not allocate raw picture buffer\n");
                      exit(1);
                  }
                  
                  // resize frame
                  sws_scale(resizeCtx, frame->data, frame->linesize, 0, frame->height,
                      resizedFrame->data, resizedFrame->linesize);
                  
                  unsigned char* pixels = new unsigned char[resizedFrame->width * resizedFrame->height * 3];
                  
                  // read pixels from frame
                  if (avpicture_layout((AVPicture*)resizedFrame, (AVPixelFormat)resizedFrame->format,
                       resizedFrame->width, resizedFrame->height,
                       pixels, resizedFrame->width * resizedFrame->height * 3) < 0)
                  {
                      fprintf(stderr, "Could not resize frame\n");
                      exit(1);
                  }
                  
                  glUseProgram(0);
                  glDepthMask(GL_FALSE);
                  
                  // draw the background
                  glDrawPixels(resizedFrame->width,
                               resizedFrame->height,
                               GL_RGB,
                               GL_UNSIGNED_BYTE,
                               (GLvoid*)pixels);
                  
                  glDepthMask(GL_TRUE);
                  
                  delete[] pixels;
              }
          }
      }

      

      light();
      mShader.begin();
      mOmni.uniforms(mShader);

      gl.pushMatrix();

      gl.draw(cube);
      
      gl.pushMatrix();
      // rotate over time:
      gl.rotate(now*30., 0.707, 0.707, 0.);
      gl.translate(5., 5., 5.);
      gl.draw(sphere);
      gl.popMatrix();
      
      gl.popMatrix();

      mShader.end();
  }


  virtual void onAnimate(al_sec dt) {
    pose = nav();
  }

  virtual void onMessage(al::osc::Message& m) {
    OmniApp::onMessage(m);
  }

  virtual bool onKeyDown(const al::Keyboard& k) { return true; }
    
    void addSink(H264RawPixelsSink* sink)
    {
        boost::mutex::scoped_lock lock(sinkMutex);
        sinks.push_back(sink);
    }
};

int mainDynamicCubemapBackgroundApp(int argc, char* argv[]) {
  DynamicCubemapBackgroundApp().start();
  return 0;
}
