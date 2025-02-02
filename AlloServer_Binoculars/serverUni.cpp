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

#include <thread>

#include <liveMedia.hh>
#define EventTime server_EventTime
#include <BasicUsageEnvironment.hh>
#undef EventTime
//#include <pthread.h>
#include "H264VideoOnDemandServerMediaSubsession.hh"
#include "shared.h"
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
			   char const* streamName, char const* inputFileName); // fwd

static char newMatroskaDemuxWatchVariable;
static MatroskaFileServerDemux* demux;
static void onMatroskaDemuxCreation(MatroskaFileServerDemux* newDemux, void* /*clientData*/) {
  demux = newDemux;
  newMatroskaDemuxWatchVariable = 1;
}
void eventLoop();

void startRTSP(){
//    pthread_t thread;
//    return pthread_create(&thread,NULL,eventLoop, NULL);
    std::thread thread1(eventLoop);
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
    char const* inputFileName = "test.264";
      
    H264VideoOnDemandServerMediaSubsession *subsession = H264VideoOnDemandServerMediaSubsession::createNew(*env, inputFileName, reuseFirstSource);
      
      
    //videoSink = subsession->createNewRTPSink(groupSock, 96);
      
    ServerMediaSession* sms
      = ServerMediaSession::createNew(*env, streamName, streamName,
				      descriptionString);
    sms->addSubsession(subsession);
    rtspServer->addServerMediaSession(sms);

    announceStream(rtspServer, sms, streamName, inputFileName);
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
  env->taskScheduler().doEventLoop(); // does not return

 // return 0; // only to prevent compiler warning
}


static void announceStream(RTSPServer* rtspServer, ServerMediaSession* sms,
			   char const* streamName, char const* inputFileName) {
  char* url = rtspServer->rtspURL(sms);
  UsageEnvironment& env = rtspServer->envir();
  env << "\n\"" << streamName << "\" stream, from the file \""
      << inputFileName << "\"\n";
  env << "Play this stream using the URL \"" << url << "\"\n";
  delete[] url;
}
