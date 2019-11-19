#pragma once

#include <FramedSource.hh>
#include <boost/thread/barrier.hpp>
#include <boost/thread/synchronized_value.hpp>
//#include <boost/thread/condition.hpp>
#include <thread>

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavutil/opt.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/time.h>
    #include <libswscale/swscale.h>
    #include <libavformat/avformat.h>
    #include <x264.h>
}

#include "AlloShared/ConcurrentQueue.hpp"
#include "AlloShared/Cubemap.hpp"

class H264NALUSource : public FramedSource
{
public:
	static H264NALUSource* createNew(UsageEnvironment& env,
                                     Frame* content,
                                     int avgBitRate,
									 bool robustSyncing);

	typedef std::function<void(H264NALUSource* self,
		                       uint8_t type,
		                       size_t size)> OnSentNALU;
	typedef std::function<void(H264NALUSource* self)> OnEncodedFrame;

	void setOnSentNALU    (const OnSentNALU&     callback);
	void setOnEncodedFrame(const OnEncodedFrame& callback);

protected:
	H264NALUSource(UsageEnvironment& env,
                   Frame* content,
                   int avgBitRate,
				   bool robustSyncing);
	// called only by createNew(), or by subclass constructors
	virtual ~H264NALUSource();

	OnSentNALU     onSentNALU;
	OnEncodedFrame onEncodedFrame;

private:
	EventTriggerId eventTriggerId;
	static void deliverFrame0(void* clientData);
	static std::mutex triggerEventMutex;
	static std::vector<H264NALUSource*> sourcesReadyForDelivery;
	void deliverFrame();

	// redefined virtual functions:
	virtual void doGetNextFrame();
	//virtual void doStopGettingFrames(); // optional

	int x2yuv(AVFrame *xFrame, AVFrame *yuvFrame, AVCodecContext *c);
	SwsContext *img_convert_ctx;

	// Stores unencoded frames
	ConcurrentQueue<AVFrame*> frameBuffer;
	// Here unused frames are stored. Included so that we can allocate all the frames at startup
	// and reuse them during runtime
	ConcurrentQueue<AVFrame*> framePool;

	// Stores encoded frames
	ConcurrentQueue<AVPacket> pktBuffer;
	ConcurrentQueue<AVPacket> pktPool;

	static unsigned referenceCount; // used to count how many instances of this class currently exist

	Frame* content;
	AVCodecContext* codecContext;

	std::thread frameContentThread;
	std::thread encodeFrameThread;

	void frameContentLoop();
	void encodeFrameLoop();

	bool destructing;

	int64_t lastFrameTime;

	int_least64_t lastPTS;
	bool robustSyncing;
};
