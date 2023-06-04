#ifndef TASK_H_
#define TASK_H_

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

typedef struct Task {

  void (*task_func)(void *user_data);
  void *user_data;
  int b_running;
  pthread_t thread;

} Task;

void task_start(Task *task, void (*task_func)(void *user_data), void *user_data);

void task_stop(Task *task);

#endif // TASK_H_
