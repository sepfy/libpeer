#ifndef SCREENCAST_INDEX_HTML_H_
#define SCREENCAST_INDEX_HTML_H_
 
#include <stdio.h>
#include <stdlib.h>

const char index_html[] = " \
<!DOCTYPE html> \n \
<html> \n \
  <head> \n \
    <title>Screencast</title> \n \
  </head> \n \
  <body> \n \
    <script> \n \
      var pc = new RTCPeerConnection({ \n \
        iceServers: [{urls: 'stun:stun.l.google.com:19302'}] \n \
      }); \n \
      var log = msg => { console.log(msg); }; \n \
      function sendOfferToChannel(sdp) { \n \
        var xhttp = new XMLHttpRequest(); \n \
        xhttp.onreadystatechange = function() { \n \
          if (this.readyState == 4 && this.status == 200) { \n \
            let res = JSON.parse(atob(this.responseText)); \n \
            console.log(res); \n \
            if(res.type == 'answer') { \n \
              pc.setRemoteDescription(new RTCSessionDescription(res)); \n \
            } \n \
          } \n \
        }; \n \
        xhttp.open('POST', '/channel/demo'); \n \
        xhttp.setRequestHeader('Content-Type', 'plain/text'); \n \
        xhttp.send(btoa(JSON.stringify({'type': 'offer', 'sdp': sdp}))); \n \
      } \n \
      pc.oniceconnectionstatechange = e => log(pc.iceConnectionState); \n \
      pc.onicecandidate = event => { \n \
        if(event.candidate === null) sendOfferToChannel(pc.localDescription.sdp) \n \
      }; \n \
      navigator.mediaDevices.getDisplayMedia({ video: true, audio: false }) \n \
        .then(stream => { \n \
          stream.getTracks().forEach(track => pc.addTrack(track, stream)); \n \
            pc.createOffer().then(d => pc.setLocalDescription(d)).catch(log) \n \
          }).catch(log) \n \
    </script> \n \
  </body> \n \
</html>";

#endif // SCREENCAST_INDEX_HTML_H_
