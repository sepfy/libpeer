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
        iceServers: [{urls: 'stun:142.250.21.127:19302'}] \n \
      }); \n \
      var log = msg => { console.log(msg); }; \n \
      function sdpExchange(sdp) { \n \
        var xhttp = new XMLHttpRequest(); \n \
        xhttp.onreadystatechange = function() { \n \
          if (this.readyState == 4 && this.status == 200) { \n \
            pc.setRemoteDescription(new RTCSessionDescription({'type': 'answer', 'sdp': this.responseText})); \n \
          } \n \
        }; \n \
        xhttp.open('POST', '/sdpexchange'); \n \
        xhttp.setRequestHeader('Content-Type', 'plain/text'); \n \
        xhttp.send(sdp); \n \
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
        if(event.candidate === null) sdpExchange(pc.localDescription.sdp) \n \
      }; \n \
      pc.addTransceiver('video', {'direction': 'sendrecv'}) \n \
      pc.createOffer().then(d => pc.setLocalDescription(d)).catch(log); \n \
    </script> \n \
  </body> \n \
</html>";

#endif // SURVEILLANCE_INDEX_HTML_H_
