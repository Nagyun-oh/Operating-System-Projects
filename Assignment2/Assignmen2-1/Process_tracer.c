#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/syscalls.h> // __SYSCALL_DEFINEX() 
#include <linux/kallsyms.h> // kallsyms_lookup_name() 
#include <linux/sched.h>
#include <asm/syscall_wrapper.h> /* __SYCALL_DEFINEx()  */
#include <asm/pgtable_types.h>
#include <asm/ptrace.h>

// sys_call_table pointer
unsigned long **sys_call_table;

// os_ftrace 포인터 저장
typedef asmlinkage long(*syscall_ptregs_t)(const struct pt_regs *);
static syscall_ptregs_t real_os_ftrace;


// sys_call_table 쓰기 on
void make_rw(void *addr)
{
	unsigned int level;
	pte_t *pte = lookup_address((unsigned long)addr,&level);

	if(!(pte->pte & _PAGE_RW))
		pte->pte |= _PAGE_RW;
}

// sys_call_table 다시 읽기 모드로 바꾸기
void make_ro(void* addr)
{
	unsigned int level;
	pte_t *pte = lookup_address((unsigned long)addr,&level);

	if(pte->pte & _PAGE_RW)
		pte->pte &=~ _PAGE_RW;
}

// task_struct의  state 값을 문자열로 반한
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

// PID를 바탕으로 과제에서 요구하는 프로세스 정보 출력하는 기능 수행
asmlinkage pid_t process_tracer(pid_t trace_task)
{
	// PID로  task_struct 얻기
	struct task_struct *task = pid_task(find_vpid(trace_task),PIDTYPE_PID);
	int sibling_cnt = 0; int child_cnt = 0;
	struct list_head *pos;
	if(!task)return -1;


	// 해당 프로세스의 주요 정보 출력
	printk(KERN_INFO"[OSLab.] ##### TASK INFORMATION of ''[%d] %s'' #####\n",trace_task,task->comm);
	printk(KERN_INFO"[OSLab.] - task state : %s\n",state_to_string(task->state));
	printk(KERN_INFO"[OSLab.] - Process Group Leader : [%d] %s\n",task->group_leader->pid,
			task->group_leader->comm);
	printk(KERN_INFO"[OSLab.] - # of context-switch(es) : %lu\n",task->nvcsw + task->nivcsw);
	printk(KERN_INFO"[OSLab.] - Number of calling fork() : %d\n",task->fork_count);
	printk(KERN_INFO "[OSLab.] - its parent process : [%d] %s\n", task->parent->pid, task->parent->comm);

	// 형제 프로세스 정보 출력
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

	// 자식 프로세스 정보 출력
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

// pt_regs -> pid -> wrapping -> process_tracer
asmlinkage long os_ftrace_trampoline(const struct pt_regs *regs)
{
	pid_t pid = (pid_t)regs->di; //(x86_64 calling convention)
	if(real_os_ftrace)real_os_ftrace(regs); // 원본 os_ftrace  수행
	
	return (long)process_tracer(pid); // 코드에서 정외된 process_tracer 수
}



// 모듈 로드 
static int  __init hook_init(void){

	// sys_call_table 주소 획득
	sys_call_table = (unsigned long**)kallsyms_lookup_name("sys_call_table");

	if(!sys_call_table)
	{
		printk(KERN_ERR "Couldn't find sys_call_table\n");
		return -1;
	}

	// 원본 os_ftrace 저장
	real_os_ftrace = (syscall_ptregs_t)sys_call_table[336];
//	printk(KERN_INFO "saved real_os_ftrace = %px\n", real_os_ftrace);

	// hooking
	make_rw(sys_call_table);
	sys_call_table[336] = (unsigned long*)os_ftrace_trampoline; // hooking행
	make_ro(sys_call_table);


	return 0;
}

// 모듈 제거 시 원 래함수 복원
static void __exit hook_exit(void){


	if(sys_call_table&& real_os_ftrace)
	{
		make_rw(sys_call_table);
		sys_call_table[336] = (unsigned long*)real_os_ftrace;
		make_ro(sys_call_table);
	//	printk(KERN_INFO "os_ftrace restored\n");
	}
}


module_init(hook_init);
module_exit(hook_exit);
MODULE_LICENSE("GPL");

