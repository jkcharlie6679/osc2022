#include "shell.h"
#include "uart.h"
#include "string.h"
#include "reboot.h"
#include "printf.h"
#include "cpio.h"
#include "malloc.h"
#include "scheduler.h"
#include "timer.h"
#include "mailbox.h"


void test(){
  printf("test\n\r");
}

void shell(){
  char *input = malloc(sizeof(char) * BUF_MAX_SIZE);
  char **args = malloc(sizeof(char*) * BUF_ARG_SIZE);
  *input = 0;
  int args_num = 0;
  char read = 0;
  printf("pi#  ");
  while(1){
    read = uart_getc();
    if(read != '\n' && read != 0x7f){
      append_str(input, read);
      printf("%c", read);
    }else if(read == 0x7f){
      if(strlen(input) > 0){
        pop_str(input);
        printf("\b \b");
      }
    }else{
      args_num = spilt_strings(args, input, " ");
      if(args_num != 0){
        printf("\n\r");
        if(!strcmp(args[0], "help")){
          printf("reboot    : reboot the device\n\r");
        }else if(!strcmp(args[0], "reboot")){
          printf("Bye bye~\n\r");
          reset(300);
        }else if(!strcmp(args[0], "ls")){
          cpio_ls();
        }else if(!strcmp(args[0], "run")){
          char *addr = load_program("syscall.img");
          if(addr != 0){
            task_create((thread_func)addr, USER);
            idle_thread();
          }
        }else if (!strcmp(args[0], "p")){
          show_page_list();
        }else if (!strcmp(args[0], "ff")){
          show_frame();
        }else if (!strcmp(args[0], "m")){
          char *addr = malloc((uint64_t)myHex2Int(args[1]));
          printf("0x%lx\n\r", addr);
        }else if (!strcmp(args[0], "f")){
          char *addr = (char *)((uint64_t)myHex2Int(args[1]));
          free(addr);
        }else if(!strcmp(args[0], "bb")){
          volatile unsigned int  __attribute__((aligned(16))) mbox[8];
          mbox[0] = 32; // buffer size in bytes
          mbox[1] = 0x00000000;
          mbox[2] = 0x10002; // tag identifier
          mbox[3] = 8; // maximum of request and response value buffer's length.
          mbox[4] = 0x00000000;
          mbox[5] = 0; // value buffer
          mbox[6] = 0;
          mbox_call(8, (unsigned int *)mbox);
          printf("0x%x\n\r", mbox[5]);
        }
        
        printf("pi#  ");
        input[0] = 0;
      }
    }
  }
}

