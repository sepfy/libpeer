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
      const mediaStream = new MediaStream(); \n \
      let el = document.getElementById('remoteVideos'); \n \
      el.srcObject = mediaStream; \n \
      el.autoplay = true; \n \
      el.controls = true; \n \
      el.muted = true;   \n \
\n \
      function jsonRpc(payload, cb) { \n \
        var xhttp = new XMLHttpRequest(); \n \
        xhttp.onreadystatechange = function() { \n \
          if (this.readyState == 4 && this.status == 200) { \n \
            cb(this.responseText); \n \
          } \n \
        }; \n \
        xhttp.open('POST', '/api'); \n \
        xhttp.setRequestHeader('Content-Type', 'application/json'); \n \
        xhttp.send(JSON.stringify(payload)); \n \
      } \n \
      let pc = new RTCPeerConnection({ \n \
        iceServers: [{urls: 'stun:stun.l.google.com:19302'}] \n \
      }); \n \
      let log = msg => { \n \
        console.log(msg); \n \
      }; \n \
 \n \
      pc.ontrack = (event) => { \n \
        var el = document.getElementById('remoteVideos'); \n \
        el.srcObject.addTrack(event.streams[0].getTracks()[0]); \n \
      } \n \
 \n \
      pc.oniceconnectionstatechange = e => log(pc.iceConnectionState); \n \
      function jsonRpcHandle(result) { \n \
        let sdp = JSON.parse(result).result; \n \
        if (sdp === '') { \n \
          return alert('Session Description must not be empty'); \n \
        } \n \
        try { \n \
console.log(JSON.parse(atob(sdp))); \n \
          pc.setRemoteDescription(new RTCSessionDescription(JSON.parse(atob(sdp)))); \n \
        } catch (e) { \n \
          alert(e); \n \
        } \n \
      } \n \
      pc.onicecandidate = event => { \n \
        if (event.candidate === null) { \n \
          var lines = pc.localDescription.sdp.split('\\n'); \n \
          for(let i = 0; i < lines.length; i++) { \n \
            // remove candidate which libnice cannot parse. \n \
            if(lines[i].search('candidate') != -1 && lines[i].search('local') != -1) { \n \
              lines.splice(i, 1); \n \
              i--; \n \
            } \n \
          } \n \
          sdp = lines.join('\\n'); \n \
          var payload = {\"jsonrpc\": \"2.0\", \"method\": \"call\", \"params\": btoa(sdp)}; \n \
          jsonRpc(payload, jsonRpcHandle); \n \
        } \n \
      }; \n \
      pc.addTransceiver('audio', {'direction': 'sendrecv'}) \n \
      pc.addTransceiver('video', {'direction': 'sendrecv'}) \n \
      navigator.mediaDevices.getDisplayMedia({ video: true, audio: true }) \n \
        .then(stream => { \n \
          stream.getTracks().forEach(track => pc.addTrack(track, stream)); \n \
            pc.createOffer().then(d => pc.setLocalDescription(d)).catch(log) \n \
          }).catch(log) \n \
      window.startSession = () => { \n \
        let sd = document.getElementById('remoteSessionDescription').value; \n \
      } \n \
    </script> \n \
  </body> \n \
</html>";

#endif // GSTREAMER_INDEX_HTML_H_
