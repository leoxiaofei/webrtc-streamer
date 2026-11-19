/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness
** for any purpose.
**
** liveaudiosource.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include <thread>

#include "environment.h"
#include "audiopipe.h"

/* ---------------------------------------------------------------------------
**  Audio source owning a client connection feeding an AudioPipe
** -------------------------------------------------------------------------*/
template <typename T>
class LiveAudioSource : public AudioPipe, public T::Callback
{
    public:
        void CaptureThread()
        {
            m_env.mainloop();
        }

        // overide T::Callback, feed the audio pipeline
        bool onNewSession(const char *id, const char *media, const char *codec, const char *sdp, unsigned int rtpfrequency, unsigned int channels) override
        {
            return AudioPipe::onNewSession(id, media, codec, sdp, rtpfrequency, channels);
        }
        bool onData(const char *id, unsigned char *buffer, ssize_t size, struct timeval presentationTime) override
        {
            return AudioPipe::onData(id, buffer, size, presentationTime);
        }

    protected:
        LiveAudioSource(webrtc::scoped_refptr<webrtc::AudioDecoderFactory> audioDecoderFactory, const std::string &uri, const std::map<std::string, std::string> &opts, bool wait)
            : AudioPipe(audioDecoderFactory, wait)
            , m_env()
            , m_liveclient(m_env, this, uri.c_str(), opts, webrtc::GetLoggingConfig().debug_severity() <= 2 ? 2 : 0)
        {
            m_liveclient.start();
            m_capturethread = std::thread(&LiveAudioSource::CaptureThread, this);
        }
        virtual ~LiveAudioSource()
        {
            m_liveclient.stop();
            m_env.stop();
            m_capturethread.join();
        }

    private:
        Environment m_env;

    protected:
        T                                               m_liveclient;

    private:
        std::thread                                     m_capturethread;
};
