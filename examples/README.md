# Examples

Here are examples to build WebRTC applications with Raspberry Pi or x86 PC, include video, audio streaming and display.

All solutions are based on gstreamer, so the gstreamer plugin is necessary. Install the gstreamer plugin by the following command.

```
sudo apt-get install -y gstreamer1.0-tools gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-plugins-good libgstreamer1.0-dev
```

And use the following command to enable building whole examples.

```
cd pear/cmake
cmake -DENABLE_EXAMPLES=on ..
make
```

If enable building these examples, the project will build library with a simple signaling service by HTTP server to help us exchange SDP, but it still need to connect to the STUN service with internet. So make sure your your internet is available.

Due to some permission restriction, recommand to enable HTTPS instead of the default singaling HTTP server. Here is a sample to use NGINX to reverse proxy:

```
sudo apt install nginx
sudo mkdir /etc/nginx/ssl
sudo openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout /etc/nginx/ssl/nginx.key -out /etc/nginx/ssl/nginx.crt
```

Modify the configure file of NGINX /etc/nginx/sites-available/default to:

```
server {
  listen 80 default_server;
  listen [::]:80 default_server;

  listen 443 ssl default_server;
  listen [::]:443 ssl default_server;

  ssl_certificate /etc/nginx/ssl/nginx.crt;
  ssl_certificate_key /etc/nginx/ssl/nginx.key;

  ...

  location / {
      proxy_pass http://localhost:8000;
  }

  location /channel/demo {
      proxy_pass http://localhost:8000;
  }

```

Besides, these application may require hardware support, such as camera, sound card and HDMI monitor. You can reference my hardware configuration or use your scenario.

