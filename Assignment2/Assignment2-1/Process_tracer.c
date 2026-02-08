#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/syscalls.h> // __SYSCALL_DEFINEX() 
#include <linux/kallsyms.h> // kallsyms_lookup_name() 
#include <linux/sched.h>
#include <asm/syscall_wrapper.h> /* __SYCALL_DEFINEx()  */
#include <asm/pgtable_types.h>
#include <asm/ptrace.h>

#define __NR_os_ftrace 336

/* ============================================================
 *  Global variables & typedefs
 * ============================================================ */

/*
* sys_call_table pointer
* Used to hook custom system call (os_ftrace)
* 
* NOTE:
* Direct modification of sys_call_table is unsafe and
* should be used for educational purposes only.
*/
static unsigned long **sys_call_table;

typedef asmlinkage long(*syscall_ptregs_t)(const struct pt_regs *);
static syscall_ptregs_t real_os_ftrace;


/* ============================================================
 *  Page permission helpers
 * ============================================================ */

/*
* make_rw()
* 
* Temporaily enables write permission for the page.
* containing sys_call_table by manipulating its PTE.
* 
* This is required because sys_call_table is normally
* write-protected by the kernel.
*/
void make_rw(void *addr)
{
	unsigned int level;
	pte_t *pte = lookup_address((unsigned long)addr,&level);

	if(!(pte->pte & _PAGE_RW))
		pte->pte |= _PAGE_RW;
}

/*
* make_ro()
* 
* Restores the page containing sys_call_table.
* to read-only after hooking is complete.
*/
void make_ro(void* addr)
{
	unsigned int level;
	pte_t *pte = lookup_address((unsigned long)addr,&level);

	if(pte->pte & _PAGE_RW)
		pte->pte &=~ _PAGE_RW;
}

/*
* Converts task state value into a human-readable string.
*/
const char*state_to_string(long state)
{
	switch(state)
	{
		case TASK_RUNNING:
			return "Running or ready";
		case TASK_INTERRUPTIBLE:
			return "Wait";
		case TASK_UNINTERRUPTIBLE:
			return "Wait with ignoring all signals";
		case TASK_STOPPED:
			return "Stopped";
		case EXIT_ZOMBIE:
			return "Zombie process";
		case EXIT_DEAD:
			return "DEAD";
		default:
			return "etc";
	}
}

/*
* process_tracer()
* 
* Prints detailed process information for the given PID:
* - task state
* - process group leader
* - context switch count
* - parent / sibling / child processes
* 
* Returns:
*  -  PID on success
*  -  -1 if the task does not exist
*/
asmlinkage pid_t process_tracer(pid_t trace_task)
{
	struct task_struct *task = pid_task(find_vpid(trace_task),PIDTYPE_PID);
	if (!task)
		return -ESRCH;

	int sibling_cnt = 0; int child_cnt = 0;
	struct list_head *pos;
	
	printk(KERN_INFO"[OSLab.] ##### TASK INFORMATION of ''[%d] %s'' #####\n",trace_task,task->comm);
	printk(KERN_INFO"[OSLab.] - task state : %s\n",state_to_string(task->state));
	printk(KERN_INFO"[OSLab.] - Process Group Leader : [%d] %s\n",task->group_leader->pid,
			task->group_leader->comm);
	printk(KERN_INFO"[OSLab.] - # of context-switch(es) : %lu\n",task->nvcsw + task->nivcsw);
	printk(KERN_INFO"[OSLab.] - Number of calling fork() : %d\n",task->fork_count);
	printk(KERN_INFO "[OSLab.] - its parent process : [%d] %s\n", task->parent->pid, task->parent->comm);

	printk(KERN_INFO "[OSLab.] - its sibling process(es) : \n");
	list_for_each(pos,&task->parent->children){
		struct task_struct *sibling = list_entry(pos,struct task_struct, sibling);

		if(sibling->pid != task->pid){

			printk(KERN_INFO "[OSLab.]    > [%d] %s\n",sibling->pid,sibling->comm);
			sibling_cnt++;

		}
	}
	if(sibling_cnt ==0)printk(KERN_INFO "[OSLab.]    > It has no sibling.\n");
	else printk(KERN_INFO "[OSLab.]    > This process has %d sibling process(es)\n",sibling_cnt);

	printk(KERN_INFO "[OSLab.] - its child process(es) : \n");
	list_for_each(pos, &task->children) 
	{
        	struct task_struct *child = list_entry(pos, struct task_struct, sibling);
        	printk(KERN_INFO "[OSLab.]    > [%d] %s\n", child->pid, child->comm);
        	child_cnt++;
    }

	if(child_cnt == 0)printk(KERN_INFO "[OSLab.]    > It has no child.\n");
	else printk(KERN_INFO "[OSLab.]    > This process has %d child process(es)\n",child_cnt);

	printk(KERN_INFO "[OSLab.] ##### END OF INFORMATION #####\n");

	return trace_task;
}

/*
* os_ftrace_trampoline()
* 
* Wrapper for os_ftrace system call
* Extracts PID from pt_regs(x86_64 ABI)
* and invokes process_tracer()
*/
asmlinkage long os_ftrace_trampoline(const struct pt_regs *regs)
{
	pid_t pid = (pid_t)regs->di; 
	if(real_os_ftrace)real_os_ftrace(regs); 
	
	return (long)process_tracer(pid); 
}

/* ============================================================
 * Module
 * ============================================================ */

 /*
 * hook_init
 *
 * 1. Locate sys_call_table using kallsyms
 * 2. Save original syscall pointer
 * 3. Temporarily disable write protection
 * 4. Replace syscall entry with fook function
 */
static int  __init hook_init(void){

	sys_call_table = (unsigned long**)kallsyms_lookup_name("sys_call_table");

	if(!sys_call_table)
	{
		printk(KERN_ERR "Couldn't find sys_call_table\n");
		return -1;
	}

	real_os_ftrace = (syscall_ptregs_t)sys_call_table[__NR_os_ftrace];

	make_rw(sys_call_table);
	sys_call_table[__NR_os_ftrace] = (unsigned long*)os_ftrace_trampoline;
	make_ro(sys_call_table);

	return 0;
}

/*
* hook_exit - module unload routine
*
* Restore original syscall pointer to prevent
*/
static void __exit hook_exit(void){

	if(sys_call_table&& real_os_ftrace)
	{
		make_rw(sys_call_table);
		sys_call_table[__NR_os_ftrace] = (unsigned long*)real_os_ftrace;
		make_ro(sys_call_table);
	}
}

module_init(hook_init);
module_exit(hook_exit);
MODULE_LICENSE("GPL");

