#ifndef GSTREAMER_INDEX_HTML_H_
#define GSTREAMER_INDEX_HTML_H_
 
#include <stdio.h>
#include <stdlib.h>

const char index_html[] = " \
<!DOCTYPE html> \n \
<html> \n \
  <head> \n \
    <title>GStreamer</title> \n \
  </head> \n \
  <body> \n \
    <video style='display:block; margin: 0 auto;' id='remoteVideos'></video> \n \
    <script> \n \
      function sendOfferToChannel(sdp) { \n \
        var xhttp = new XMLHttpRequest(); \n \
        xhttp.onreadystatechange = function() { \n \
          if (this.readyState == 4 && this.status == 200) { \n \
            let answer = JSON.parse(this.responseText); \n \
            console.log(answer); \n \
            if(answer.sdp !== undefined) { \n \
              pc.setRemoteDescription(new RTCSessionDescription(JSON.parse(atob(sdp)))); \n \
            } \n \
          } \n \
        }; \n \
        xhttp.open('POST', '/channel/demo'); \n \
        xhttp.setRequestHeader('Content-Type', 'application/json'); \n \
        xhttp.send(JSON.stringify({'type': 'offer', 'sdp': btoa(sdp)})); \n \
      } \n \
      let pc = new RTCPeerConnection({ \n \
        iceServers: [{urls: 'stun:stun.l.google.com:19302'}] \n \
      }); \n \
      let log = msg => { \n \
        console.log(msg); \n \
      }; \n \
 \n \
      pc.ontrack = function (event) { \n \
        var el = document.getElementById('remoteVideos'); \n \
        el.srcObject = event.streams[0]; \n \
        el.autoplay = true; \n \
        el.controls = true; \n \
        el.muted = true; \n \
      }; \n \
 \n \
      pc.oniceconnectionstatechange = e => log(pc.iceConnectionState); \n \
      function jsonRpcHandle(result) { \n \
        let sdp = JSON.parse(result).result; \n \
        if (sdp === '') { \n \
          return alert('Session Description must not be empty'); \n \
        } \n \
        try { \n \
          pc.setRemoteDescription(new RTCSessionDescription(JSON.parse(atob(sdp)))); \n \
        } catch (e) { \n \
          alert(e); \n \
        } \n \
      } \n \
      pc.onicecandidate = event => { \n \
        if (event.candidate === null) { \n \
          sendOfferToChannel(pc.localDescription.sdp) \n \
        } \n \
      }; \n \
      pc.addTransceiver('video', {'direction': 'sendrecv'}) \n \
      pc.createOffer().then(d => pc.setLocalDescription(d)).catch(log); \n \
    </script> \n \
  </body> \n \
</html>";

#endif // GSTREAMER_INDEX_HTML_H_
