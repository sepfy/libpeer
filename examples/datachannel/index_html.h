#ifndef DATACHANNEL_INDEX_HTML_H_
#define DATACHANNEL_INDEX_HTML_H_
 
#include <stdio.h>
#include <stdlib.h>

const char index_html[] = " \
<!DOCTYPE html> \n \
<html> \n \
  <head> \n \
    <title>Datachannel</title> \n \
  </head> \n \
  <body> \n \
    <label for='msg'>Message:</label><br> \n \
    <input type='text' id='msg' name='msg'><br><br> \n \
    <input type='submit' value='Send' onclick='sendMessage()'> \n \
    <script> \n \
      var pc = new RTCPeerConnection({ \n \
        iceServers: [{urls: 'stun:stun.l.google.com:19302'}] \n \
      }); \n \
      var log = msg => { console.log(msg); }; \n \
      function sendOfferToCall(sdp) { \n \
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
        xhttp.open('POST', '/call/demo'); \n \
        xhttp.setRequestHeader('Content-Type', 'plain/text'); \n \
        xhttp.send(btoa(JSON.stringify({'type': 'offer', 'sdp': sdp}))); \n \
      } \n \
      const sendChannel = pc.createDataChannel('pear') \n \
      pc.ondatachannel = () => { \n \
        console.log('ondatachannel');\n \
      } \n \
      sendChannel.onclose = () => console.log('sendChannel has closed'); \n \
      sendChannel.onopen = () => console.log('sendChannel has opened'); \n \
      sendChannel.onmessage = e => log(`Message from DataChannel '${sendChannel.label}' payload '${e.data}'`); \n \
      pc.oniceconnectionstatechange = e => log(pc.iceConnectionState); \n \
      pc.onicecandidate = event => { \n \
//        if(event.candidate === null) //sendOfferToCall(pc.localDescription.sdp) \n \
      }; \n \
      pc.createOffer().then(d => pc.setLocalDescription(d)).catch(log); \n \
      setTimeout(function() { sendOfferToCall(pc.localDescription.sdp); }, 1000); \n \
      function sendMessage() { sendChannel.send(document.getElementById('msg').value); } \n \
    </script> \n \
  </body> \n \
</html>";

#endif // DATACHANNEL_INDEX_HTML_H_
