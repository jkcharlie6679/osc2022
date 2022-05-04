#include "scheduler.h"
#include "printf.h"
#include "malloc.h"
#include "shell.h"
#include "timer.h"
#include "delay.h"
#include "system.h"

static task *run_queue = NULL;
static task *zombies_queue = NULL;
static int pid = 1;
static void enqueue(task **queue, task *new_task);
static void *dequeue(task **queue);
static void fork_test();
static void foo();

void thread_init(void){
  task *new_task = malloc(sizeof(task));
  new_task->pid = pid++;
  new_task->next = NULL;
  new_task->state = RUNNING;
  new_task->lr = (uint64_t)idle_thread;
  char *addr = malloc(THREAD_SP_SIZE);
  new_task->fp = (uint64_t)addr;
  new_task->sp = (uint64_t)addr;
  enqueue(&run_queue, new_task);
}

void idle_thread(void){
  // for(int i = 0; i < 3; ++i) { 
  //   task_create(foo, USER);
  // }
  // task_create(fork_test, USER);
  write_current((uint64_t)dequeue(&run_queue));
  add_timer(normal_timer, "normal_timer", get_timer_freq()>>5);
  while (1){
    schedule();
    kill_zombies();
  }
}

void *task_create(thread_func func, enum mode mode){
  task *new_task = malloc(sizeof(task));
  new_task->mode = mode;
  new_task->next = NULL;
  new_task->pid = pid++;
  new_task->state = RUNNING;
  if(mode == USER){
    char *addr = malloc(THREAD_SP_SIZE);
    new_task->user_sp = (uint64_t)addr;
    addr = addr + THREAD_SP_SIZE - 16;
    new_task->lr = (uint64_t)switch_to_user_space;
    new_task->target_func = (uint64_t)func;
  }else{
    new_task->lr = (uint64_t)func;
  }
  char *addr = malloc(THREAD_SP_SIZE);
  new_task->sp_addr = (uint64_t)addr;
  addr = addr + THREAD_SP_SIZE - 16;
  new_task->fp = (uint64_t)addr;
  new_task->sp = (uint64_t)addr;
  enqueue(&run_queue, new_task);  
  return new_task;
}

void schedule(){
  task *cur = get_current();
  task *next = dequeue(&run_queue);
  cur->next = NULL;
  if (cur->pid == 1 && !next){
    pid = 2;
    enqueue(&run_queue, cur);
    core_timer_interrupt_disable();
    shell();
  }
  if(cur->state != EXIT)
    enqueue(&run_queue, cur);
  else
    enqueue(&zombies_queue, cur);
  switch_to(cur, next);
}

void kill_zombies(){
  task* cur = dequeue(&zombies_queue);
  if(cur != NULL){
    free(cur);
    free((char *)cur->sp_addr);
    if(cur->mode == USER)
      free((char *)cur->user_sp);
  }
}

void enqueue(task **queue, task *new_task){
  task *cur = *queue;
  task *prev = cur;
  new_task->next = NULL;
  if(*queue  == NULL){
    *queue = new_task;
  }else{
    while (cur){
      prev = cur;
      cur = cur->next;
    }
    prev->next = new_task;
  }
}

void *dequeue(task **queue){
  task *pop_task = *queue;
  *queue = (*queue)->next;
  return pop_task;
}

void kill_thread(int pid){
  task *cur = run_queue;
  task *pre = NULL;
  while (cur){
    if(cur->pid == pid){
      if(pre != NULL)
        pre->next = cur->next;
      else
        run_queue = cur->next;
      enqueue(&zombies_queue, cur);
      break;
    }
    pre = cur;
    cur = cur->next;
  }
  return;
}

void switch_to_user_space() {
  task *cur = get_current();
  asm volatile("mov x0, 0   \n"::);
  asm volatile("msr spsr_el1, x0   \n"::);
  asm volatile("msr elr_el1,  %0   \n"::"r"(cur->target_func));
  asm volatile("msr sp_el0,   %0   \n"::"r"(cur->user_sp + THREAD_SP_SIZE - cur->user_sp%16));
  asm volatile("eret  \n"::);
}




void fork_test(){
  printf("\nFork Test, pid %d\n", sys_getpid());
  int cnt = 1;
  int ret = 0;
  if ((ret = sys_fork()) == 0) { // child
    long long cur_sp;
    asm volatile("mov %0, sp" : "=r"(cur_sp));
    printf("first child pid: %d, cnt: %d, ptr: %x, sp : %x\n", sys_getpid(), cnt, &cnt, cur_sp);
    ++cnt;
    if ((ret = sys_fork()) != 0){
      asm volatile("mov %0, sp" : "=r"(cur_sp));
      printf("first child pid: %d, cnt: %d, ptr: %x, sp : %x\n", sys_getpid(), cnt, &cnt, cur_sp);
    }else{
      while (cnt < 5) {
        asm volatile("mov %0, sp" : "=r"(cur_sp));
        printf("second child pid: %d, cnt: %d, ptr: %x, sp : %x\n", sys_getpid(), cnt, &cnt, cur_sp);
        delay_tick(1000000);
        ++cnt;
      }
    }
    sys_exit();
  }else {
    printf("parent here, pid %d, child %d\n", sys_getpid(), ret);
  }
  sys_exit();
}

void foo(){
  printf("this is foo %d\n\r", sys_getpid());
  for(int i = 0; i < 5; ++i) {
    printf("pid: %d, %d\n\r", sys_getpid(), i);
    if(sys_getpid() == 3 && i == 2){
      sys_fork();
    }
    delay_tick(10000000);
  }
  sys_exit();
}
