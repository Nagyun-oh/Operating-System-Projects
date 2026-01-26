#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/syscalls.h> // __SYSCALL_DEFINEX() 
#include <linux/kallsyms.h> // kallsyms_lookup_name() 
#include <linux/sched.h>
#include <asm/syscall_wrapper.h> /* __SYCALL_DEFINEx()  */

// sys_call_table pointer
unsigned long **sys_call_table;

// os_ftrace 포인터 저장
typedef asmlinkage long(*os_ftrace_t)(pid_t);
static os_ftrace_t real_os_ftrace;

// wrapper function
__SYSCALL_DEFINEx(1,os_ftrace_hook,pid_t,pid)
{
	printk(KERN_INFO "os_ftrace() hooked! os_ftrace -> my_ftrace");
	return real_os_ftrace(pid); // 원래 함수 호출
}

// sys_call_table 쓰기 on
void make_rw(void *addr)
{
	unsigned int level;
	pte_t *pte = lookup_address((unsigned long)addr,&level);

	if(pte->pte & ~ _PAGE_RW)
		pte->pte |= _PAGE_RW;
}

// sys_call_table 다시 읽기 모드로 바꾸기
void make_ro(void* addr)
{
	unsigned int level;
	pte_t *pte = lookup_address((unsigned long)addr,&level);

	pte->pte &=~ _PAGE_RW;
}


// hooking func
static asmlinkage long my_ftrace(pid_t pid){
	printk(KERN_INFO "os_ftrace() hooked! os_ftrace -> my_ftrace");
	return 0;
}


// load module
static int  __init hook_init(void){
	sys_call_table = (unsigned long**)kallsyms_lookup_name("sys_call_table");

	if(!sys_call_table)
	{
		printk(KERN_ERR "Couldn't find sys_call_table\n");
		return -1;
	}

	real_os_ftrace = (os_ftrace_t)sys_call_table[548];
	printk(KERN_INFO "saved real_os_ftrace = %px\n", real_os_ftrace);

	// hooking
	make_rw(sys_call_table);
	sys_call_table[548] = (unsigned long*)my_ftrace; // hooking
	make_ro(sys_call_table);

	printk(KERN_INFO "os_ftrace hooked!\n");

	return 0;
}

// del module
static void __exit hook_exit(void){


	if(sys_call_table&& real_os_ftrace)
	{
		make_rw(sys_call_table);
		sys_call_table[548] = (unsigned long*)real_os_ftrace;
		make_ro(sys_call_table);
		printk(KERN_INFO "os_ftrace restored\n");
	}		
}


module_init(hook_init);
module_exit(hook_exit);
MODULE_LICENSE("GPL");



















