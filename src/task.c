#include <pthread.h>

#include "task.h"
#include "utils.h"

static void* task_loop(void *arg) {

  Task *task = (Task*)arg;

  while (task->b_running) {

    task->task_func(task->user_data);
  }

  pthread_exit(NULL);
}

void task_start(Task *task, void (*task_func)(void *user_data), void *user_data) {

  if (task->b_running)
    return;

  task->task_func = task_func;
  task->user_data = user_data;
  task->b_running = 1;
  pthread_create(&task->thread, NULL, task_loop, task);
}

void task_stop(Task *task) {

  if (!task->b_running)
    return;
  task->b_running = 0;
  pthread_join(task->thread, NULL);
}

