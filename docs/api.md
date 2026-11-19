# api

The WebRTC signaling is implemented through HTTP requests:

# get Ice configuration  
 - /api/getIceServers :  get IceServers configuration used by streamer side 

 # initiatiate communication as a caller
 - /api/call          : send offer and get answer
 - /api/whep          : similar to /api/call using [WHEP](https://www.ietf.org/archive/id/draft-murillo-whep-02.txt)
# initiatiate communication asking to be called 
 - /api/createOffer   : create an offer 
 - /api/setAnswer     : set an answer

# call communication  
 - /api/hangup        : close a call

# Ice notifications  
 - /api/addIceCandidate : add a candidate
 - /api/getIceCandidate : get the list of candidates

# playback control (rtsp sources supporting seek/scale)
 - /api/speed          : change the playback speed of the stream of a peer connection (rate parameter)
 - /api/seek           : seek the stream of a peer connection to a position in seconds (time parameter)
 - /api/pause          : pause the stream of a peer connection (RTSP "PAUSE" command)
 - /api/resume         : resume a paused stream of a peer connection
 - /api/progress       : get the playback state {position, rangeStart, rangeEnd, duration, scale, seekable, paused} of the stream of a peer connection

```
curl "http://host:8000/api/speed?peerid=<peerid>&rate=2"
curl "http://host:8000/api/seek?peerid=<peerid>&time=30"
curl "http://host:8000/api/pause?peerid=<peerid>"
curl "http://host:8000/api/resume?peerid=<peerid>"
curl "http://host:8000/api/progress?peerid=<peerid>"
```

The same commands are available on the WebRTC data channel created by the server, using JSON messages:

```
{"cmd":"speed","value":2}
{"cmd":"seek","value":30}
{"cmd":"pause"}
{"cmd":"resume"}
{"cmd":"progress"}
```

Each command is answered with the playback state JSON, allowing a client to refresh its UI.

# playback options

Options can be passed using the options parameter of /api/call or in the JSON url configuration:
 - rate (or scale)   : initial playback speed sent with the RTSP PLAY (default 1.0)
 - closewait         : delay in seconds before closing the RTSP connection when the last client hangs up (override the rtsp-close-delay command line argument)

The initial position can be set in the RTSP url:
 - rtsp://host/stream?ntptime=60            : start playing at 60s from the beginning
 - rtsp://host/stream?starttime=20210129T103000Z : start playing at an absolute date

Playback sources using an absolute clock range (NVR recordings, e.g. Hikvision
`/Streaming/tracks/...`) are supported: pass the start and end of the recording
in the url, the progress is reported in seconds from the starttime:

```
rtsp://host/Streaming/tracks/101/?starttime=20251222T100549Z&endtime=20251222T110614Z
```

The seek and speed commands of such sources are implemented by reconnecting at
the requested position (more reliable with NVRs). The endtime also bounds the
play range, without it the server may answer the end of all its recordings.

Beware that in a web page url, the parameters of the video url after `?` must
be encoded (`&` as `%26`), otherwise they become parameters of the page:

```
webrtcstreamer.html?video=rtsp%3A%2F%2Fhost%2Ftracks%2F101%2F%3Fstarttime%3D20251222T100549Z%26endtime%3D20251222T110614Z
```

Negative rates (reverse playback) can be sent with the REST api for sources
advertising them (e.g. `Scales="-1,..."` in the PLAY answer):
```
curl "http://host:8000/api/speed?peerid=<peerid>&rate=-1"
```

The list of HTTP API is available using /api/help.
