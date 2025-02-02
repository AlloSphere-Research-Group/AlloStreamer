// Example low level rendering Unity plugin

#include <math.h>
#include <stdio.h>
#include <thread>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/offset_ptr.hpp>
#include <vector>
#include <algorithm>

#include "CubemapExtractionPlugin.h"
#include "FrameD3D9.hpp"
#include "FrameD3D11.hpp"
#include "FrameOpenGL.hpp"
#include "AlloShared/config.h"
#include "AlloShared/Process.h"
#include "AlloServer/AlloServer.h"
#include "AlloShared/Binoculars.hpp"

Frame* getFrameFromTexture(void* texturePtr);

// --------------------------------------------------------------------------
// Helper utilities

static ShmAllocator* shmAllocator = nullptr;
static Process* thisProcess = nullptr;
static Process alloServerProcess(ALLOSERVER_ID, false);
static std::chrono::system_clock::time_point presentationTime;
static std::mutex d3D11DeviceContextMutex;
static boost::interprocess::managed_shared_memory shm;
static Binoculars* binoculars = nullptr;
static StereoCubemap::Ptr cubemap;
static std::mutex mutex;
static bool closeResetIPCThread;
static std::thread resetIPCThread;

struct CubemapConfig
{
    void** texturePtrs;
    size_t facesCount;
    int width;
	int height;
};
static CubemapConfig* cubemapConfig = nullptr;

struct BinocularsConfig
{
    void* texturePtr;
    int width;
    int height;
};
static BinocularsConfig* binocularsConfig = nullptr;

GLuint tex;

// Prints a string
static void DebugLog (const char* str)
{
	#if UNITY_WIN
	OutputDebugStringA (str);
	#else
	printf ("%s", str);
	#endif
}

void resetIPCLoop()
{
	while (true)
	{
		while (!alloServerProcess.timedWaitForBirth(std::chrono::milliseconds(100)))
		{
			if (closeResetIPCThread) return;
		}
		while (!alloServerProcess.timedJoin(std::chrono::milliseconds(100)))
		{
			if (closeResetIPCThread) return;
		}
        std::unique_lock<std::mutex> lock(mutex);
		for (int j = 0; j < cubemap->getEyesCount(); j++)
		{
			Cubemap* eye = cubemap->getEye(j);
			for (int i = 0; i < eye->getFacesCount(); i++)
			{
				eye->getFace(i)->getContent()->getBarrier().reset();
			}
		}
	}
}

void allocateCubemap(CubemapConfig* cubemapConfig)
{
    shm.destroy<StereoCubemap::Ptr>("Cubemap");
    
    if (cubemapConfig)
    {
        std::vector<CubemapFace*> leftFaces;
        for (int i = 0; i < (std::min)((size_t)Cubemap::MAX_FACES_COUNT, cubemapConfig->facesCount); i++)
        {
            CubemapFace* face = CubemapFace::create(getFrameFromTexture(cubemapConfig->texturePtrs[i]),
                                                    i,
                                                    *shmAllocator);
			leftFaces.push_back(face);
        }

		std::vector<CubemapFace*> rightFaces;
		for (int i = Cubemap::MAX_FACES_COUNT; i < cubemapConfig->facesCount; i++)
		{
			CubemapFace* face = CubemapFace::create(getFrameFromTexture(cubemapConfig->texturePtrs[i]),
				i,
				*shmAllocator);
			rightFaces.push_back(face);
		}
        
		std::vector<Cubemap*> eyes;
		eyes.push_back(Cubemap::create(leftFaces, *shmAllocator));
		eyes.push_back(Cubemap::create(rightFaces, *shmAllocator));

		cubemap = StereoCubemap::create(eyes, *shmAllocator);
        StereoCubemap::Ptr cubemapPtr = *shm.construct<StereoCubemap::Ptr>("Cubemap")(cubemap.get());
    }
    else
    {
        cubemap = nullptr;
    }
}

void allocateBinoculars(BinocularsConfig* binocularsConfig)
{
    shm.destroy<Binoculars::Ptr>("Binoculars");
    
    if (binocularsConfig)
    {
        binoculars = Binoculars::create(getFrameFromTexture(binocularsConfig->texturePtr),
                                        *shmAllocator);
        Binoculars::Ptr binocarlsPtr = *shm.construct<Binoculars::Ptr>("Binoculars")(binoculars);
    }
    else
    {
        binoculars = nullptr;
    }
}

void allocateSHM(CubemapConfig* cubemapConfig, BinocularsConfig* binocularsConfig)
{
    unsigned long shmSize = 65536;
    if (cubemapConfig)
    {
        shmSize += cubemapConfig->width * cubemapConfig->height * 4 * cubemapConfig->facesCount +
                   sizeof(Cubemap) + cubemapConfig->facesCount * sizeof(CubemapFace);
    }
    if (binocularsConfig)
    {
        shmSize += binocularsConfig->width * binocularsConfig->height * 4 + sizeof(Frame);
    }
    
    boost::interprocess::shared_memory_object::remove(SHM_NAME);
    
    shm = boost::interprocess::managed_shared_memory(boost::interprocess::open_or_create,
                                                     SHM_NAME,
                                                     shmSize);
    
    shmAllocator = new ShmAllocator(*(new ShmAllocator::BoostShmAllocator(shm.get_segment_manager())));
    
    allocateCubemap(cubemapConfig);
    allocateBinoculars(binocularsConfig);
    
    thisProcess = new Process(CUBEMAPEXTRACTIONPLUGIN_ID, true);

	closeResetIPCThread = false;
	resetIPCThread = std::thread(&resetIPCLoop);
}



void releaseSHM()
{
    std::unique_lock<std::mutex> lock(mutex);
	closeResetIPCThread = true;
    shm.destroy<Cubemap::Ptr>("Cubemap");
    cubemap = nullptr;
    cubemapConfig = nullptr;
    shm.destroy<Cubemap::Ptr>("Binoculars");
    binoculars = nullptr;
    binocularsConfig = nullptr;
    boost::interprocess::shared_memory_object::remove(SHM_NAME);
    delete thisProcess;
    thisProcess = nullptr;
}

// --------------------------------------------------------------------------
// UnitySetGraphicsDevice

static int g_DeviceType = -1;


// Actual setup/teardown functions defined below
#if SUPPORT_D3D9
IDirect3DDevice9* g_D3D9Device;
#endif
#if SUPPORT_D3D11
ID3D11Device* g_D3D11Device;
#endif


extern "C" void EXPORT_API UnitySetGraphicsDevice (void* device, int deviceType, int eventType)
{
	// Set device type to -1, i.e. "not recognized by our plugin"
	g_DeviceType = -1;
	
	#if SUPPORT_D3D9
	// D3D9 device, remember device pointer and device type.
	// The pointer we get is IDirect3DDevice9.
	if (deviceType == kGfxRendererD3D9)
	{
		DebugLog ("Set D3D9 graphics device\n");
		g_DeviceType = deviceType;
		g_D3D9Device = (IDirect3DDevice9*)device;
	}
	#endif

	#if SUPPORT_D3D11
	// D3D11 device, remember device pointer and device type.
	// The pointer we get is ID3D11Device.
	if (deviceType == kGfxRendererD3D11)
	{
		DebugLog ("Set D3D11 graphics device\n");
		g_DeviceType = deviceType;
		g_D3D11Device = (ID3D11Device*)device;
	}
	#endif

	#if SUPPORT_OPENGL
	// If we've got an OpenGL device, remember device type. There's no OpenGL
	// "device pointer" to remember since OpenGL always operates on a currently set
	// global context.
	if (deviceType == kGfxRendererOpenGL)
	{
		DebugLog ("Set OpenGL graphics device\n");
		g_DeviceType = deviceType;
	}
	#endif
}

Frame* getFrameFromTexture(void* texturePtr)
{
	// A script calls this at initialization time; just remember the texture pointer here.
	// Will update texture pixels each frame from the plugin rendering event (texture update
	// needs to happen on the rendering thread).
    
    Frame* frame = nullptr;

#if SUPPORT_D3D9
	// D3D9 case
	if (g_DeviceType == kGfxRendererD3D9)
	{
        /*frame = FrameD3D9::create(
			(IDirect3DTexture9*)texturePtr,
			index,
			*shmAllocator);*/
	}
#endif


#if SUPPORT_D3D11
	// D3D11 case
	if (g_DeviceType == kGfxRendererD3D11)
	{
		frame = FrameD3D11::create((ID3D11Texture2D*)texturePtr,
			                       *shmAllocator);
	}
#endif


#if SUPPORT_OPENGL
	// OpenGL case
	if (g_DeviceType == kGfxRendererOpenGL)
	{
        frame = FrameOpenGL::create((GLuint)(size_t)texturePtr,
                                    *shmAllocator);
	}
#endif
    
    return frame;
}

void copyFromGPUToCPU(Frame* frame)
{
    frame->setPresentationTime(presentationTime);
    
    // PREPARE COPYING
    
#if SUPPORT_D3D9
    // D3D9 case
    if (g_DeviceType == kGfxRendererD3D9)
    {
        //CubemapFaceD3D9* faceD3D9 = (CubemapFaceD3D9*)face;
        // copy data from GPU to CPU
        //HRESULT hr = g_D3D9Device->GetRenderTargetData(faceD3D9->gpuSurfacePtr, faceD3D9->cpuSurfacePtr);
    }
#endif
    
    
#if SUPPORT_D3D11
    // D3D11 case
    if (g_DeviceType == kGfxRendererD3D11)
    {
		FrameD3D11* frameD3D11 = (FrameD3D11*)frame;
        
        // DirectX 11 is not thread-safe
        std::mutex::scoped_lock lock(d3D11DeviceContextMutex);
        
        ID3D11DeviceContext* g_D3D11DeviceContext = NULL;
        g_D3D11Device->GetImmediateContext(&g_D3D11DeviceContext);
        
        // Copy data from GPU to CPU
		// Since the texture is YUV420p encoded we only need 3/8 the amount of data
		D3D11_BOX region =
		{
			0, 0, 0,
			frameD3D11->getWidth(), frameD3D11->getHeight() * 3 / 8, 1
		};
		g_D3D11DeviceContext->CopySubresourceRegion(frameD3D11->cpuTexturePtr, 0, 0, 0, 0, frameD3D11->gpuTexturePtr, 0, &region);
    }
#endif
    
    
#if SUPPORT_OPENGL
    // OpenGL case
    if (g_DeviceType == kGfxRendererOpenGL)
    {
        FrameOpenGL* frameOpenGL = (FrameOpenGL*)frame;
        glBindTexture(GL_TEXTURE_2D, frameOpenGL->getGPUTextureID());
        //glBindTexture(GL_TEXTURE_2D, tex);
    }
#endif
    
    //while (!frame->getMutex().timed_lock(boost::get_system_time() + boost::posix_time::milliseconds(100)))
    //{
    //    if (!alloServerProcess.isAlive())
    //    {
    //        // Reset the mutex otherwise it will block forever
    //        // Reset the condition otherwise it my be in inconsistent state
    //        // Hacky solution indeed
    //        void* mutexAddr     = &frame->getMutex();
    //        void* conditionAddr = &frame->getNewPixelsCondition();
    //        // That's not possible unfortunately since the mutex is locked and abandoned
    //        //face->getMutex().~interprocess_mutex();
    //        memset(mutexAddr, 0, sizeof(boost::interprocess::interprocess_mutex));
    //        memset(conditionAddr, 0, sizeof(boost::interprocess::interprocess_condition));
    //        new (mutexAddr)     boost::interprocess::interprocess_mutex;
    //        new (conditionAddr) boost::interprocess::interprocess_condition;
    //        return;
    //    }
    //}

    //boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(frame->getMutex(),
    //                                                                               boost::interprocess::accept_ownership);
    // COPY


	{
		boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(frame->getMutex());

#if SUPPORT_D3D9
		// D3D9 case
		if (g_DeviceType == kGfxRendererD3D9)
		{
			/*CubemapFaceD3D9* faceD3D9 = (CubemapFaceD3D9*)face;
			memcpy(faceD3D9->getPixels(),
			faceD3D9->lockedRect.pBits,
			faceD3D9->getWidth() * faceD3D9->getHeight() * 4);*/
		}
#endif


#if SUPPORT_D3D11
		// D3D11 case
		if (g_DeviceType == kGfxRendererD3D11)
		{
			FrameD3D11* frameD3D11 = (FrameD3D11*)frame;
			memcpy(frameD3D11->getPixels(),
				frameD3D11->resource.pData,
				frameD3D11->getWidth() * frameD3D11->getHeight() * 3 / 2);
		}
#endif


#if SUPPORT_OPENGL
		// OpenGL case
		if (g_DeviceType == kGfxRendererOpenGL)
		{
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, frame->getPixels());
		}
#endif
	}

	while (alloServerProcess.isAlive() && !frame->getBarrier().timedWait(std::chrono::milliseconds(1000)))
	{
	}
}

void copyFromGPUtoCPU (std::vector<Frame*>& frames)
{
    if (g_DeviceType == kGfxRendererD3D9 || g_DeviceType == kGfxRendererD3D11)
    {
        std::thread* threads = new std::thread[frames.size()];
        
        for (int i = 0; i < frames.size(); i++)
        {
            threads[i] = std::thread(std::bind(&copyFromGPUToCPU, frames[i]));
        }
        
        for (int i = 0; i < frames.size(); i++)
        {
            threads[i].join();
        }
    }
    else
    {
        for (int i = 0; i < frames.size(); i++)
        {
            copyFromGPUToCPU(frames[i]);
        }
    }

	/*static bool alloServerWasAlive = alloServerProcess.isAlive();
	if (!alloServerProcess.isAlive() && alloServerWasAlive)
	{
		for (auto frame : frames)
		{
			frame->getBarrier().reset();
		}
		alloServerWasAlive = false;
	}*/
}

// --------------------------------------------------------------------------
// UnityRenderEvent
// This will be called for GL.IssuePluginEvent script calls; eventID will
// be the integer passed to IssuePluginEvent. In this example, we just ignore
// that value.

extern "C" void EXPORT_API UnityRenderEvent (int eventID)
{
    // When we use the RenderCubemap.cs and USeRenderingPlugin.cs scripts
    // This function will get called twice (with different eventIDs).
    // We have to make sure that the extraction only happens once for the cubemap
	// and binoculars respectively!
    if (eventID == 1)
    {
        std::unique_lock<std::mutex> lock(mutex);

        // Allocate cubemap the first time we render a frame.
        // By doing so, we can make sure that both
        // the cubemap and the binoculars are fully configured.
        if (!thisProcess)
        {
            allocateSHM(cubemapConfig, binocularsConfig);
        }
        
        presentationTime = std::chrono::system_clock::now();
        
        std::vector<Frame*> frames;
        if (cubemap)
        {
			for (int j = 0; j < cubemap->getEyesCount(); j++)
			{
				Cubemap* eye = cubemap->getEye(j);
				for (int i = 0; i < eye->getFacesCount(); i++)
				{
					frames.push_back(eye->getFace(i)->getContent());
				}
			}
        }
        
        if (binoculars)
        {
            frames.push_back(binoculars->getContent());
        }
        
        copyFromGPUtoCPU(frames);
    }
}

// --------------------------------------------------------------------------
// Start stop management

extern "C" void EXPORT_API ConfigureCubemapFromUnity(void** texturePtrs, int cubemapFacesCount, int width, int height)
{
    
    
    /*glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    char* pixels = new char[resolution * resolution * 3];
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, resolution, resolution, 0, GL_RGB, GL_BYTE, pixels);
    delete[] pixels;*/
    
    cubemapConfig              = new CubemapConfig;
    cubemapConfig->texturePtrs = texturePtrs;
    cubemapConfig->facesCount  = cubemapFacesCount;
    cubemapConfig->width       = width;
	cubemapConfig->height      = height;
}

extern "C" void EXPORT_API ConfigureBinocularsFromUnity(void* texturePtr, int width, int height)
{
    binocularsConfig             = new BinocularsConfig;
    binocularsConfig->texturePtr = texturePtr;
    binocularsConfig->width      = width;
    binocularsConfig->height     = height;
}

extern "C" void EXPORT_API StopFromUnity()
{
    releaseSHM();
}
