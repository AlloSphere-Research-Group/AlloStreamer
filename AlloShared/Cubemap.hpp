#pragma once

#include <boost/cstdint.hpp>
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/managed_heap_memory.hpp>
#include <boost/interprocess/segment_manager.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <libavutil/pixfmt.h>
//#include <boost/chrono/system_clocks.hpp>
#include <array>
#include <chrono>

#include "Allocator.h"
#include "Frame.hpp"

#define CUBEMAP_MAX_FACES_COUNT 6
#define STEREOCUBEMAP_MAX_EYES_COUNT 2

class CubemapFace
{
public:
	bool getNewFaceFlag();
	void setNewFaceFlag(bool);
	typedef boost::interprocess::offset_ptr<CubemapFace> Ptr;
    
    int getIndex();
    Frame* getContent();
    
    static CubemapFace* create(Frame* content,
                               int index,
                               Allocator& allocator);
    static void destroy(CubemapFace* cubemapFace);

protected:
	bool newFaceFlag;
    CubemapFace(Frame* content,
                int index,
                Allocator& allocator);
    ~CubemapFace();
    
    Frame::Ptr content;
    int index;
    Allocator& allocator;
};

class Cubemap
{
public:
    typedef boost::interprocess::offset_ptr<Cubemap> Ptr;

	enum { MAX_FACES_COUNT = 6 };
    
    CubemapFace* getFace(int index, bool force=false);
    int getFacesCount();
    
    static Cubemap* create(std::vector<CubemapFace*> faces,
                           Allocator& allocator);
    static void destroy(Cubemap* cubemap);
    
protected:
    Cubemap(std::vector<CubemapFace*>& faces,
            Allocator& allocator);
    ~Cubemap();
    Allocator& allocator;
    
private:
	std::array<CubemapFace::Ptr, MAX_FACES_COUNT> faces;
    boost::interprocess::interprocess_mutex mutex;
};

class StereoCubemap
{
public:
    typedef boost::interprocess::offset_ptr<StereoCubemap> Ptr;
    //static const size_t MAX_EYES_COUNT;
    
	enum { MAX_EYES_COUNT = 2 };

    Cubemap* getEye(int index);
    int getEyesCount();
    
    static StereoCubemap* create(std::vector<Cubemap*>& eyes,
                                 Allocator& allocator);
    static void destroy(StereoCubemap* stereoCubemap);
    
protected:
    StereoCubemap(std::vector<Cubemap*>& eyes, Allocator& allocator);
    ~StereoCubemap();
    Allocator& allocator;
    
private:
	std::array<Cubemap::Ptr, MAX_EYES_COUNT> eyes;
};
