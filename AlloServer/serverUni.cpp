/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// Copyright (c) 1996-2013, Live Networks, Inc.  All rights reserved
// A test program that demonstrates how to stream - via unicast RTP
// - various kinds of file on demand, using a built-in RTSP server.
// main program
#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif

#include "liveMedia.hh"
#define EventTime server_EventTime
#include "BasicUsageEnvironment.hh"
#undef EventTime
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include "H264VideoOnDemandServerMediaSubsession.h"
#include "shared.h"
#include "config.h"
#include "RandomFramedSource.h"
#include "AlloShared/CubemapFace.h"
#include "AlloShared/Signals.h"
#include "AlloServer.h"

//RTPSink* videoSink;
UsageEnvironment* env;

// To make the second and subsequent client for each stream reuse the same
// input stream as the first client (rather than playing the file from the
// start for each client), change the following "False" to "True":
Boolean reuseFirstSource = True;

// To stream *only* MPEG-1 or 2 video "I" frames
// (e.g., to reduce network bandwidth),
// change the following "False" to "True":
Boolean iFramesOnly = False;


static void announceStream(RTSPServer* rtspServer, ServerMediaSession* sms,
			   char const* streamName); // fwd

static char newMatroskaDemuxWatchVariable;
static MatroskaFileServerDemux* demux;
static void onMatroskaDemuxCreation(MatroskaFileServerDemux* newDemux, void* /*clientData*/) {
  demux = newDemux;
  newMatroskaDemuxWatchVariable = 1;
}
void eventLoop();
void addFaceSubstream();

void startRTSP(){
//    pthread_t thread;
//    return pthread_create(&thread,NULL,eventLoop, NULL);
    boost::thread thread1(eventLoop);
}

ServerMediaSession* sms;

EventTriggerId addFaceSubstreamTriggerId;

void addFaceSubstream0(void* face) {

	//for (int i = 0; i < cubemap.faces.size(); i++) {

		H264VideoOnDemandServerMediaSubsession *subsession = H264VideoOnDemandServerMediaSubsession::createNew(*env, reuseFirstSource, (CubemapFace*)face);
		sms->addSubsession(subsession);
	//}

}

std::vector<H264VideoOnDemandServerMediaSubsession*> faceSubstreams;

void addFaceSubstream()
{
	while (true)
	{
		boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(cubemap->mutex);
		std::vector<CubemapFace*> facesToAdd;
		for (int i = faceSubstreams.size(); i < cubemap->count(); i++)
		{
			facesToAdd.push_back(cubemap->getFace(i).get());
		}
		for (int i = 0; i < facesToAdd.size(); i++)
		{
			env->taskScheduler().triggerEvent(addFaceSubstreamTriggerId, facesToAdd[i]);
		}
		cubemap->newFaceCondition.wait(lock);
	}
}

void eventLoop() {
  // Begin by setting up our usage environment:
  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
  env = BasicUsageEnvironment::createNew(*scheduler);

  UserAuthenticationDatabase* authDB = NULL;
#ifdef ACCESS_CONTROL
  // To implement client access control to the RTSP server, do the following:
  authDB = new UserAuthenticationDatabase;
  authDB->addUserRecord("username1", "password1"); // replace these with real strings
  // Repeat the above with each <username>, <password> that you wish to allow
  // access to the server.
#endif

  // Create the RTSP server:
  RTSPServer* rtspServer = RTSPServer::createNew(*env, 8554, authDB);
//RTSPServer* rtspServer = RTSPServer::createNew(*env, 8555, authDB);
  if (rtspServer == NULL) {
    *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
    exit(1);
  }

  char const* descriptionString
    = "Session streamed by \"testOnDemandRTSPServer\"";

  // Set up each of the possible streams that can be served by the
  // RTSP server.  Each such stream is implemented using a
  // "ServerMediaSession" object, plus one or more
  // "ServerMediaSubsession" objects for each audio/video substream.

    
  OutPacketBuffer::maxSize = 4000000;

  // A H.264 video elementary stream:
  {
    char const* streamName = "h264ESVideoTest";
    
    sms = ServerMediaSession::createNew(*env, streamName, streamName,
				      descriptionString);
    
    
      
    
    //H264VideoOnDemandServerMediaSubsession *subsession2 = H264VideoOnDemandServerMediaSubsession::createNew(*env, reuseFirstSource, "two");
      
      
    //videoSink = subsession->createNewRTPSink(groupSock, 96);
      
    
    //sms->addSubsession(subsession1);
    
    rtspServer->addServerMediaSession(sms);

    announceStream(rtspServer, sms, streamName);
  }

 
  // Also, attempt to create a HTTP server for RTSP-over-HTTP tunneling.
  // Try first with the default HTTP port (80), and then with the alternative HTTP
  // port numbers (8000 and 8080).
  
  if (rtspServer->setUpTunnelingOverHTTP(80) || rtspServer->setUpTunnelingOverHTTP(8000) || rtspServer->setUpTunnelingOverHTTP(8080)) {
    *env << "\n(We use port " << rtspServer->httpServerPortNum() << " for optional RTSP-over-HTTP tunneling.)\n";
  } else {
    *env << "\n(RTSP-over-HTTP tunneling is not available.)\n";
  }
    
    /*
    // Start the streaming:
    *env << "Beginning streaming...\n";
    startPlay();
    //play(NULL);
    usleep(5000000);
    printf("done play\n");
    */
  addFaceSubstreamTriggerId = env->taskScheduler().createEventTrigger(&addFaceSubstream0);

  boost::thread thread2(&addFaceSubstream);

  env->taskScheduler().doEventLoop(); // does not return

 // return 0; // only to prevent compiler warning
}


static void announceStream(RTSPServer* rtspServer, ServerMediaSession* sms,
			   char const* streamName) {
  char* url = rtspServer->rtspURL(sms);
  UsageEnvironment& env = rtspServer->envir();
  env << "Play this stream using the URL \"" << url << "\"\n";
  delete[] url;
}