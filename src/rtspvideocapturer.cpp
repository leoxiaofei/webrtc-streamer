/* ---------------------------------------------------------------------------
 * SPDX-License-Identifier: Unlicense
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
 * software, either in source code form or as a compiled binary, for any purpose,
 * commercial or non-commercial, and by any means.
 *
 * For more information, please refer to <http://unlicense.org/>
 * -------------------------------------------------------------------------*/

#ifdef HAVE_LIVE555

#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"

#include "rtspvideocapturer.h"

RTSPVideoCapturer::RTSPVideoCapturer(const std::string & uri, const std::map<std::string,std::string> & opts, std::unique_ptr<webrtc::VideoDecoderFactory>& videoDecoderFactory, webrtc::scoped_refptr<webrtc::AudioDecoderFactory> audioDecoderFactory)
	: LiveVideoSource(uri, opts, videoDecoderFactory, false)
	, RTSPPlayback(uri)
{
	if (audioDecoderFactory)
	{
		// demux the audio tracks of the connection into an audio source
		m_audioPipe = webrtc::scoped_refptr<AudioPipe>(new webrtc::FinalRefCountedObject<AudioPipe>(audioDecoderFactory, false));
	}
	RTC_LOG(LS_INFO) << "RTSPVideoCapturer " << uri ;

}

RTSPVideoCapturer::~RTSPVideoCapturer()
{
}


void RTSPVideoCapturer::onError(RTSPConnection& connection, const char* error) {
	RTC_LOG(LS_ERROR) << "RTSPVideoCapturer:onError url:" << m_liveclient.getUrl() <<  " error:" << error;
	connection.start(1);
}		


#endif
