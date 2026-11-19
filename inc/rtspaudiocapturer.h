/* ---------------------------------------------------------------------------
 * SPDX-License-Identifier: Unlicense
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
 * software, in source code form, or as a compiled binary, for any purpose,
 * commercial or non-commercial, and by any means.
 *
 * For more information, please refer to <http://unlicense.org/>
 * -------------------------------------------------------------------------*/

#pragma once

#include "liveaudiosource.h"
#include "rtspconnectionclient.h"
#include "rtspplayback.h"

#include "rtc_base/ref_counted_object.h"

class RTSPAudioSource : public LiveAudioSource<RTSPConnection>, public RTSPPlayback {
	public:
		static webrtc::scoped_refptr<RTSPAudioSource> Create(webrtc::scoped_refptr<webrtc::AudioDecoderFactory> audioDecoderFactory, const std::string & uri, const std::map<std::string,std::string> & opts) {
			webrtc::scoped_refptr<RTSPAudioSource> source(new webrtc::FinalRefCountedObject<RTSPAudioSource>(audioDecoderFactory, uri, opts));
			return source;
		}

		// playback controls (forward to the RTSP connection)
		virtual void setScale(float scale) override {
			RTSPPlayback::setScaleState(scale);
			m_liveclient.scale(scale);
		}
		virtual void seek(double nptStartTime) override {
			if (RTSPPlayback::isClockMode()) {
				// clock range source: seek to the absolute time matching the position
				m_liveclient.seek(RTSPPlayback::clockTime(nptStartTime));
			}
			else {
				m_liveclient.seek(nptStartTime);
			}
			RTSPPlayback::onSeek(nptStartTime);
		}
		virtual void pause() override {
			m_liveclient.pause();
			RTSPPlayback::setPaused(true);
		}
		virtual void resume() override {
			m_liveclient.resume();
			RTSPPlayback::setPaused(false);
		}

		// playback state from the RTSP connection
		virtual void onPlayUpdate(RTSPConnection& connection) override {
			// pull the playback state of the connection (clock mode, scale, duration)
			RTSPPlayback::onPlayUpdate(connection.getClient());
		}
		virtual bool onData(const char *id, unsigned char *buffer, ssize_t size, struct timeval presentationTime) override {
			// track the playback position when the audio connection is the only source
			// (exact NPT from the connection, estimated from the presentation time otherwise)
			RTSPPlayback::updatePosition(m_liveclient.getNormalPlayTime(id, presentationTime), presentationTime);
			return LiveAudioSource<RTSPConnection>::onData(id, buffer, size, presentationTime);
		}

	protected:
		RTSPAudioSource(webrtc::scoped_refptr<webrtc::AudioDecoderFactory> audioDecoderFactory, const std::string & uri, const std::map<std::string,std::string> & opts);
		virtual ~RTSPAudioSource();
};
