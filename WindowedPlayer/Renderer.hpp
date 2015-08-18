#pragma once


#include <SDL.h>
#undef main
#include <boost/thread.hpp>
#include "AlloShared/concurrent_queue.h"
#include "AlloReceiver/AlloReceiver.h"

class Renderer
{
public:
    Renderer(CubemapSource* cubemapSource);
    virtual ~Renderer();
    
	void start();

    void setOnDisplayedFrame(std::function<void (Renderer*)>& callback);
    void setOnDisplayedCubemapFace(std::function<void (Renderer*, int)>& callback);
    
protected:
    std::function<void (Renderer*)> onDisplayedFrame;
    std::function<void (Renderer*, int)> onDisplayedCubemapFace;

private:
	StereoCubemap* onNextCubemap(CubemapSource* source, StereoCubemap* cubemap);
	void renderLoop();
	void createTextures(size_t number, size_t resolution);

	boost::thread                    renderThread;
	CubemapSource*                   cubemapSource;
	concurrent_queue<StereoCubemap*> cubemapBuffer;
	concurrent_queue<StereoCubemap*> cubemapPool;
	SDL_Window*                      window;
	SDL_Renderer*                    renderer;
	std::vector<SDL_Texture*>        textures;
};
