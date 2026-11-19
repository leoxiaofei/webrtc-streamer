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

#include <atomic>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

#include "rtc_base/logging.h"

#include "rtspconnectionclient.h"

/* ---------------------------------------------------------------------------
**  Mixin holding the playback state of an RTSP source supporting trick play
**  (seek, speed change and pause).
**
**  A capturer mixes this class in: the onPlayUpdate method declared here becomes
**  the final overriders of the RTSPConnection::Callback virtual, the capturer feeds
**  updatePosition using RTSPConnection::getNormalPlayTime from its onData callback, and
**  drives the RTSP connection with setScale/seek/pause/resume.
**
**  Two kinds of sources are supported:
**   - NPT sources (SDP/PLAY answer with "Range: npt=a-b"): the position of each
**     frame is answered by the connection (RTP-Info anchored NPT)
**   - clock sources (NVR playback streams using "Range: clock=..."): the connection
**     usually answers no usable NPT, the position is estimated from the
**     presentation time of the frames, anchored on the play start or the last
**     seek position; the duration comes from the starttime/endtime of the url
** -------------------------------------------------------------------------*/
class RTSPPlayback
{
	public:
		RTSPPlayback(const std::string &uri) : m_position(0.0), m_rangeStart(0.0), m_rangeEnd(0.0), m_scale(1.0)
			, m_clockMode(false), m_paused(false), m_firstPlay(true), m_pendingAnchor(true), m_refPresentationTime(0)
			, m_clockOriginTime(0), m_urlDuration(-1.0)
		{
			// clock range of the url: starttime/endtime give the play range and the origin of the positions
			std::string startTime = getUrlParam(uri, "starttime");
			std::string endTime   = getUrlParam(uri, "endtime");
			if (!startTime.empty())
			{
				m_clockOriginTime = parseClockTime(startTime);
			}
			if ( !startTime.empty() && !endTime.empty() )
			{
				m_urlDuration = difftime( parseClockTime(endTime), m_clockOriginTime );
			}
		}
		virtual ~RTSPPlayback() {}

		// playback controls, implemented by the capturer driving its RTSP connection
		virtual void setScale(float scale) = 0;
		virtual void seek(double nptStartTime) = 0;
		virtual void pause() = 0;
		virtual void resume() = 0;

		// answer of a PLAY command, pull the state of the connection
		void onPlayUpdate(const RTSPConnection::RTSPClientConnection* client)
		{
			if (client != NULL)
			{
				m_clockMode = client->isClockMode();
				m_scale     = client->getScale();

				// playback speeds supported by the server, from the "Media-Properties:" answer
				{
					std::lock_guard<std::mutex> lock(m_scalesMutex);
					m_supportedScales = client->getSupportedScales();
				}

				double duration = 0;
				if (m_urlDuration > 0)
				{
					// the starttime/endtime of the url give the range to play,
					// the range answered by the server may be the end of all its recordings
					duration = m_urlDuration;
				}
				else
				{
					duration = client->getPlayDuration();
				}
				if (duration > 0)
				{
					m_rangeStart = 0;
					m_rangeEnd   = duration;
				}

				// the first PLAY of a clock range source starts at the beginning of the range,
				// the PLAY answer of a seek keeps the position anchored on the seek target
				if (m_clockMode && m_firstPlay && (m_urlDuration > 0))
				{
					m_position = 0;
				}
				m_firstPlay = false;

				RTC_LOG(LS_INFO) << "RTSPPlayback::onPlayUpdate clockMode:" << m_clockMode
					<< " scale:" << m_scale << " duration:" << duration
					<< " (url:" << m_urlDuration << " server:" << client->getPlayDuration() << ")";
			}

			// any PLAY resumes the stream
			m_paused = false;
		}

		// normal play time of a frame answered by the connection (negative when unavailable),
		// the position is estimated from the presentation time when no usable NPT is answered
		void updatePosition(double npt, struct timeval presentationTime)
		{
			if ( !m_clockMode && (npt >= 0) && (npt < 100000) )
			{
				// the connection answers a usable normal play time, relative to the
				// start of the presentation (not usable on clock range sources: the NPT
				// restarts at 0 at each seek because it is relative to the last PLAY)
				m_position = npt;
				return;
			}

			if ( (presentationTime.tv_sec == 0) && (presentationTime.tv_usec == 0) )
			{
				// frames injected before the stream starts have no presentation time
				return;
			}
			if (!m_clockMode)
			{
				return;
			}

			// clock range source: estimate the position from the presentation time of the frames
			int64_t presentationTimeUs = static_cast<int64_t>(presentationTime.tv_sec) * 1000000 + presentationTime.tv_usec;
			if (m_pendingAnchor)
			{
				m_refPresentationTime = presentationTimeUs;
				m_pendingAnchor = false;
				return;
			}

			// signed delta: the position moves forward or backward
			// (reverse playback) following the presentation time of the frames
			double delta = static_cast<double>(presentationTimeUs - m_refPresentationTime) / 1000000.0;
			if ( (delta < 5.0) && (delta > -5.0) )
			{
				m_position = m_position + delta;
			}
			m_refPresentationTime = presentationTimeUs;
		}

		// anchor the position estimator on a seek target
		void onSeek(double nptStartTime)
		{
			RTC_LOG(LS_INFO) << "RTSPPlayback::onSeek anchor npt:" << nptStartTime;
			m_position = nptStartTime;
			m_pendingAnchor = true;
		}

		void setPaused(bool paused)
		{
			m_paused = paused;
		}

		// convert a position in seconds into an absolute clock time of the url range
		std::string clockTime(double nptStartTime) const
		{
			return formatClockTime(m_clockOriginTime + static_cast<time_t>(nptStartTime));
		}

		// clock time of the beginning of the play range (npt position 0), for logging
		std::string clockOriginText() const
		{
			return formatClockTime(m_clockOriginTime);
		}

		double position()   const { return m_position; }
		double rangeStart() const { return m_rangeStart; }
		double rangeEnd()   const { return m_rangeEnd; }
		double duration()   const { return m_rangeEnd - m_rangeStart; }
		float  scale()      const { return m_scale; }
		bool   seekable()   const { return m_rangeEnd > m_rangeStart; }
		bool   isClockMode() const { return m_clockMode; }
		bool   isPaused()   const { return m_paused; }
		// playback speeds supported by the server (empty when not advertised)
		std::vector<float> supportedScales() const {
			std::lock_guard<std::mutex> lock(m_scalesMutex);
			return m_supportedScales;
		}

	protected:
		// the capturer calls this when a new speed is requested
		void setScaleState(float scale)
		{
			if (scale != 0)
			{
				m_scale = scale;
			}
		}

		std::atomic<double> m_position;
		std::atomic<double> m_rangeStart;
		std::atomic<double> m_rangeEnd;
		std::atomic<float>  m_scale;
		std::atomic<bool>   m_paused;

		// position estimation state for clock sources
		bool                m_clockMode;
		bool                m_firstPlay;
		bool                m_pendingAnchor;
		int64_t             m_refPresentationTime;

		// clock range of the url
		time_t              m_clockOriginTime;
		double              m_urlDuration;

	private:
		mutable std::mutex  m_scalesMutex;
		std::vector<float>  m_supportedScales;

	private:
		static std::string getUrlParam(const std::string &url, const std::string &param)
		{
			std::string::size_type pos = url.find(param + "=");
			if (pos == std::string::npos)
			{
				return "";
			}
			std::string value = url.substr(pos + param.size() + 1);
			std::string::size_type end = value.find_first_of("&");
			if (end != std::string::npos)
			{
				value = value.substr(0, end);
			}
			return value;
		}

		static time_t parseClockTime(const std::string& clockTime)
		{
			struct tm tmValue;
			memset(&tmValue, 0, sizeof(tmValue));
			if (sscanf(clockTime.c_str(), "%04d%02d%02dT%02d%02d%02dZ", &tmValue.tm_year, &tmValue.tm_mon, &tmValue.tm_mday, &tmValue.tm_hour, &tmValue.tm_min, &tmValue.tm_sec) == 6)
			{
				tmValue.tm_year -= 1900;
				tmValue.tm_mon -= 1;
				return timegm(&tmValue);
			}
			return 0;
		}

		static std::string formatClockTime(time_t clockTime)
		{
			struct tm tmValue;
			gmtime_r(&clockTime, &tmValue);
			char buffer[21];
			strftime(buffer, sizeof(buffer), "%Y%m%dT%H%M%SZ", &tmValue);
			return std::string(buffer);
		}
};
