#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "buffer.h"

#define MTU 800
int g_data_length[8] = {100, 200, 300, 400, 500, 600, 700, 800};
int g_testing = 1;

int check_data(uint8_t *data, int length) {

  int sum = 0;
  int value = length*(length/100 + '0');

  for (int i = 0; i < length; i++) {

    sum += data[i];
  }

  printf("sum: %d, value: %d\n", sum, value);
  return sum == value;
}

void* test_thread(void* arg) {

  Buffer *rb = (Buffer*) arg;

  int size = 0;

  uint8_t *data = NULL;

  while (g_testing) {

    data = buffer_peak_head(rb, &size);

    if (data) {

      if (check_data(data, size) == 0) {

        printf("data error\n");
        exit(1);
      }

      buffer_pop_head(rb);

    } else {

      usleep(1000);
    }

  }

  return NULL;
}


int main(int argc, char *argv[]) {

  Buffer *rb = buffer_new(2000);
  pthread_t thread;

  pthread_create(&thread, NULL, test_thread, rb);

  uint8_t data[MTU];

  for (int i = 0; i < 8; i++) {

    memset(data, i + '1', MTU);
    buffer_push_tail(rb, data, g_data_length[i]);
    usleep(1000);
  }

  usleep(100*1000);

  g_testing = 0;

  pthread_join(thread, NULL);

  printf("test success\n");
  return 0;
}
