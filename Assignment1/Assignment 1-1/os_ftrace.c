#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/types.h>

// Define System call
SYSCALL_DEFINE1(os_ftrace,pid_t,pid)
{
	printk(KERN_INFO"ORIGINAL ftrace() called! PID is [%d]\n",pid);
	return 0;

}
