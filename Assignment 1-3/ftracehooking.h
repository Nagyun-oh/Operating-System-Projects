#ifndef FTRACEHOOKING_H
#define FTRACEHOOKING_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ftrace.h>
#include <linux/kallsyms.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <asm/pgtable.h>
#include <asm/pgtable_types.h>

// 전역 변수 및 함수 선언

extern pid_t traced_pid; //현재 프로세스의  pid 추적하기 위한 전역변수 선언

extern void start_io_trace(pid_t pid); // iotracehooking.c 에서 hijack start function
extern void stop_io_trace(void); // iotracehooking.c 에서 hijack End function


// iotracehooking.c 에서 hijack 한 fopen ~ fclose 에 대한 횟수및 bytes 를 처리하기 위한 구조체 선언
struct io_trace_stats{
	int open_count;
	int close_count;
	int read_count;
	int write_count;
	int lseek_count;
	size_t read_bytes;
	size_t write_bytes;
	char filename[256];
};

extern struct io_trace_stats stats; // 구조체를  전역 변수로 선언


#endif

