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

#include <set>

#include "livevideosource.h"
#include "rtspconnectionclient.h"
#include "rtspplayback.h"
#include "audiopipe.h"

#include "rtc_base/time_utils.h"

/* ---------------------------------------------------------------------------
**  RTSP video capturer, optionaly demuxing the audio of the same connection
**  into an audio source (single rtsp connection for both video and audio)
** -------------------------------------------------------------------------*/
class RTSPVideoCapturer : public LiveVideoSource<RTSPConnection>, public RTSPPlayback
{
	public:
		RTSPVideoCapturer(const std::string & uri, const std::map<std::string,std::string> & opts, std::unique_ptr<webrtc::VideoDecoderFactory>& videoDecoderFactory, webrtc::scoped_refptr<webrtc::AudioDecoderFactory> audioDecoderFactory = nullptr);
		virtual ~RTSPVideoCapturer();

		static RTSPVideoCapturer* Create(const std::string & url, const std::map<std::string, std::string> & opts, std::unique_ptr<webrtc::VideoDecoderFactory>& videoDecoderFactory) {
			return new RTSPVideoCapturer(url, opts, videoDecoderFactory);
		}

		static RTSPVideoCapturer* Create(const std::string & url, const std::map<std::string, std::string> & opts, std::unique_ptr<webrtc::VideoDecoderFactory>& videoDecoderFactory, webrtc::scoped_refptr<webrtc::AudioDecoderFactory> audioDecoderFactory) {
			return new RTSPVideoCapturer(url, opts, videoDecoderFactory, audioDecoderFactory);
		}

		// audio source fed by the audio tracks of the connection (null if not demuxing)
		webrtc::scoped_refptr<webrtc::AudioSourceInterface> audioSource() const { return m_audioPipe; }

		// playback controls (forward to the RTSP connection)
		virtual void setScale(float scale) override {
				RTC_LOG(LS_INFO) << "RTSPVideoCapturer::setScale " << scale;
				RTSPPlayback::setScaleState(scale);
				m_liveclient.scale(scale);
		}
		virtual void seek(double nptStartTime) override {
				if (RTSPPlayback::isClockMode()) {
					// clock range source: seek to the absolute time matching the position
					std::string clockTime = RTSPPlayback::clockTime(nptStartTime);
					RTC_LOG(LS_INFO) << "RTSPVideoCapturer::seek npt:" << nptStartTime << " clock:" << clockTime << " origin+" << RTSPPlayback::clockOriginText();
					m_liveclient.seek(clockTime);
				}
				else {
					RTC_LOG(LS_INFO) << "RTSPVideoCapturer::seek npt:" << nptStartTime;
					m_liveclient.seek(nptStartTime);
				}
				RTSPPlayback::onSeek(nptStartTime);
		}
		virtual void pause() override {
				RTC_LOG(LS_INFO) << "RTSPVideoCapturer::pause";
				m_liveclient.pause();
				RTSPPlayback::setPaused(true);
		}
		virtual void resume() override {
				RTC_LOG(LS_INFO) << "RTSPVideoCapturer::resume";
				m_liveclient.resume();
				RTSPPlayback::setPaused(false);
		}

		// overide RTSPConnection::Callback
		virtual void    onConnectionTimeout(RTSPConnection& connection) override {
				connection.start();
		}
		virtual void    onDataTimeout(RTSPConnection& connection) override {
				connection.start();
		}
		virtual void    onError(RTSPConnection& connection,const char* erro) override;
		virtual void    onPlayUpdate(RTSPConnection& connection) override {
				// pull the playback state of the connection (clock mode, scale, duration)
				RTSPPlayback::onPlayUpdate(connection.getClient());
				// a PLAY command may move the position backward (seek), allow frames from the past
				m_prevTimestamp = 0;
		}

		// overide SessionCallback
		virtual bool    onNewSession(const char *id, const char *media, const char *codec, const char *sdp, unsigned int rtpfrequency, unsigned int channels) override {
				// demux the audio tracks to the audio pipeline of the same connection
				if ( (strcmp(media, "audio") == 0) && m_audioPipe )
				{
					bool success = m_audioPipe->onNewSession(id, media, codec, sdp, rtpfrequency, channels);
					if (success)
					{
						m_audioSessions.insert(id);
					}
					return success;
				}
				return LiveVideoSource<RTSPConnection>::onNewSession(id, media, codec, sdp, rtpfrequency, channels);
		}
		virtual bool    onData(const char *id, unsigned char *buffer, ssize_t size, struct timeval presentationTime) override {
				if ( m_audioSessions.find(id) != m_audioSessions.end() )
				{
					return m_audioPipe->onData(id, buffer, size, presentationTime);
				}
				// track the playback position: exact position from the normal play time
				// answered by the connection, estimated from the presentation time of
				// the frames on clock range sources answering no usable NPT
				RTSPPlayback::updatePosition(m_liveclient.getNormalPlayTime(id, presentationTime), presentationTime);
				return LiveVideoSource<RTSPConnection>::onData(id, buffer, size, presentationTime);
		}

		// re-stamp the frames with the wall clock time on playback sources:
		// at a playback speed other than 1x the source timestamps advance faster
		// than real time, which defeats the webrtc sender and receiver pacing
		// (frames considered in the future are never rendered)
		virtual uint64_t presentationTimestamp(struct timeval presentationTime) override {
				if (RTSPPlayback::isClockMode()) {
					return webrtc::TimeMillis();
				}
				return LiveVideoSource<RTSPConnection>::presentationTimestamp(presentationTime);
		}

	protected:
		webrtc::scoped_refptr<AudioPipe>                     m_audioPipe;
		std::set<std::string>                                m_audioSessions;
};
