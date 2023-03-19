#ifndef SURVEILLANCE_INDEX_HTML_H_
#define SURVEILLANCE_INDEX_HTML_H_
 
#include <stdio.h>
#include <stdlib.h>

const char index_html[] = " \
<!DOCTYPE html> \n \
<html> \n \
  <head> \n \
    <title>Surveillance</title> \n \
  </head> \n \
  <body> \n \
    <video width='100%' id='remoteCamera'></video> \n \
    <script> \n \
      var pc = new RTCPeerConnection({ \n \
        iceServers: [{urls: 'stun:stun.l.google.com:19302'}] \n \
      }); \n \
      var log = msg => { console.log(msg); }; \n \
      function httpPost(url, payload, callback) { \n \
        var xhttp = new XMLHttpRequest(); \n \
        xhttp.onreadystatechange = function() { \n \
          if (this.readyState == 4 && this.status == 200) { \n \
            callback(this.responseText); \n \
          } \n \
        }; \n \
        xhttp.open('POST', url); \n \
        xhttp.send(payload); \n \
      } \n \
      function pollAnswer() { \n \
        httpPost('/answer', '', function(sdp) { \n \
          if(sdp == '') \n \
            setTimeout(pollAnswer, 2000); \n \
          else \n \
            pc.setRemoteDescription(new RTCSessionDescription({'type': 'answer', 'sdp': sdp})); \n \
        }); \n \
      } \n \
      pc.ontrack = function (event) { \n \
        var el = document.getElementById('remoteCamera'); \n \
        el.srcObject = event.streams[0]; \n \
        el.autoplay = true; \n \
        el.controls = true; \n \
        el.muted = true; \n \
      }; \n \
      pc.oniceconnectionstatechange = e => log(pc.iceConnectionState); \n \
      pc.onicecandidate = event => { \n \
        if(event.candidate === null) httpPost('/offer', pc.localDescription.sdp, pollAnswer) \n \
      }; \n \
      pc.addTransceiver('audio', {'direction': 'sendrecv'}) \n \
      pc.addTransceiver('video', {'direction': 'sendrecv'}) \n \
      navigator.mediaDevices.getUserMedia({ video: false, audio: true }) \n \
        .then(stream => { \n \
          stream.getTracks().forEach(track => pc.addTrack(track, stream)); \n \
          pc.createOffer().then(d => pc.setLocalDescription(d)).catch(log) \n \
        }).catch(log) \n \
    </script> \n \
  </body> \n \
</html>";

#endif // SURVEILLANCE_INDEX_HTML_H_
