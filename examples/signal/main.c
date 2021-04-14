#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>
#include <cjson/cJSON.h>

#include "utils.h"

typedef struct options_t {

  int port;
  const char *host;
  const char *root;

} options_t;

static void api_request_cb(struct evhttp_request *req, void *arg) {

  struct evbuffer *buf = NULL;
  cJSON *json = NULL;
  cJSON *sdp = NULL;
  char *payload = NULL;
  size_t len;

  if(evhttp_request_get_command(req) != EVHTTP_REQ_POST) {
    evhttp_send_error(req, HTTP_BADMETHOD, 0);
    return;
  }

  buf = evhttp_request_get_input_buffer(req);

  len = evbuffer_get_length(buf);
  payload = (char*)malloc(len);
  if(payload == NULL) {
    evhttp_send_error(req, HTTP_INTERNAL, 0);
    return;
  }

  evbuffer_remove(buf, payload, len);

  json = cJSON_Parse(payload);
  sdp = cJSON_GetObjectItemCaseSensitive(json, "sdp");
  if(cJSON_IsString(sdp) && (sdp->valuestring != NULL)) {
    printf("%s\n", sdp->valuestring);
  }

  if(payload != NULL)
    free(payload);

  evhttp_send_reply(req, HTTP_OK, "OK", NULL);
}

static void index_request_cb(struct evhttp_request *req, void *arg) {

  struct evbuffer *evb = NULL;
  int fd = -1;
  struct stat st;

  if(evhttp_request_get_command(req) != EVHTTP_REQ_GET) {
    evhttp_send_error(req, HTTP_BADMETHOD, 0);
    return;
  }

  if((fd = open("index.html", O_RDONLY)) < 0) {
    LOG_ERROR("%s", strerror(errno));
    evhttp_send_error(req, HTTP_NOTFOUND, "Document was not found");
    return;
  }

  if(fstat(fd, &st) < 0) {
    LOG_ERROR("%s", strerror(errno));
    evhttp_send_error(req, HTTP_NOTFOUND, "Document was not found");
    return;
  }

  evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "text/html");
  evb = evbuffer_new();
  evbuffer_add_file(evb, fd, 0, st.st_size);
  evhttp_send_reply(req, HTTP_OK, "OK", evb);

  if(evb)
    evbuffer_free(evb);
}

static void print_usage(const char *prog) {

  printf("Usage: %s \n"
   " -p      - port (default: 8080)\n"
   " -H      - address to bind (default: 0.0.0.0)\n"
   " -r      - document root\n"
   " -h      - print help\n", prog);

}

void parse_argv(int argc, char **argv, options_t *options) {

  int opt;

  while((opt = getopt(argc, argv, "p:H:r:h")) != -1) {
    switch(opt) {
      case 'p':
        options->port = atoi(optarg);
        break;
      case 'H':
        options->host = optarg;
        break;
      case 'r':
        options->root = optarg;
        break;
      case 'h':
        print_usage(argv[0]);
        exit(1);
        break;
      default :
        printf("Unknown option %c\n", opt);
        break;
    }
  }

}


int main(int argc, char **argv) {

  struct event_base *base;
  struct evhttp *http;
  struct evhttp_bound_socket *handle;

  options_t options = {8080, "0.0.0.0", "root"};

  parse_argv(argc, argv, &options);

  base = event_base_new();
  if(!base) {
    LOG_ERROR("Couldn't create an event_base: exiting");
    return 1;
  }

  http = evhttp_new(base);
  if(!http) {
    LOG_ERROR("Couldn't create evhttp. Exiting.");
    return 1;
  }

  evhttp_set_cb(http, "/api", api_request_cb, NULL);
  evhttp_set_cb(http, "/", index_request_cb, NULL);

  handle = evhttp_bind_socket_with_handle(http, options.host, options.port);
  if(!handle) {
    LOG_ERROR("couldn't bind to %s:%d. Exiting.\n", options.host, options.port);
    return 1;
  }

  printf("Listening %s:%d\n", options.host, options.port); 

  event_base_dispatch(base);

  return 0;
}
