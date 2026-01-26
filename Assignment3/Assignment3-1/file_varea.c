#include <asm/ptrace.h> // struct pt_regs
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/highmem.h>
#include <linux/sched/mm.h> // get_task_mm()
#include <linux/mm_types.h>
#include <linux/kallsyms.h>
#include <linux/init_task.h>
#include <linux/slab.h> // kmalloc , kfree
#include <linux/fs.h>
#include <linux/dcache.h> // d_path()
#include <linux/path.h>
#include <linux/limits.h> // PATH_MAX
#include <linux/mm.h> // vm_area_struct 

void **syscall_table;  // sys_call_table address 저장
void *real_ftrace;

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

	pte->pte &=~ _PAGE_RW;
}


/*
 * file_varea() - 과제 요구사항에 맞게 출력하는 부분
 * PID를 입력받아서 해당 프로세스의
 * 	- 코드영역, 데이터 영역, 힙 영역 출력
 * 	-mmap된 파일들의 실제 경로를 출력
*/
static asmlinkage long file_varea(const struct pt_regs *regs)
{
	pid_t target = (pid_t)regs->di;  // user가 넘긴 PID

	struct task_struct *task; // PID -> task_struct
	struct mm_struct *mm; // Memory descriptor of process
	struct vm_area_struct *vma; // for VMA 
	
	// PID -> task_struct 가져오기
	task = pid_task(find_vpid(target),PIDTYPE_PID);
	if(!task) return -ESRCH;
	
	// user process가 아니면 mm이 없을 수 있음
	mm = get_task_mm(task);
	if(!mm) return -EINVAL;	

	// 프로세스 이름/코드/데이터/힙 영역 경계 출력
	printk(KERN_INFO "######## Loaded files of a process' %s(%d)' in VM ########\n",
			task->comm,target);

	// mmap 리스트를 읽기위해서, read lock
	down_read(&mm->mmap_sem);

	// process의 모든 VMA 순회 
	for(vma = mm->mmap; vma; vma= vma -> vm_next){
		
		// 파일이 매핑된 경우에만 처리
		if(vma->vm_file)
		{
			
			// 파일 경로 출력 버퍼
			char*buf = kmalloc(PATH_MAX,GFP_KERNEL);
			if(!buf){
				printk(KERN_INFO"file_varea : kmalloc PATH_MAX failed\n");
				break;
			}

			// dentry -> full path로 변환
			char*path = d_path(&vma->vm_file->f_path,buf,PATH_MAX);
			// 메모리 영역 및 파일경로 출력
			if(!IS_ERR(path)){
				printk("mem(%lx~%lx) code(%lx~%lx) data(%lx~%lx) heap(%lx~%lx) %s\n",
						(unsigned long)vma->vm_start,(unsigned long)vma->vm_end,
						(unsigned long)mm->start_code,(unsigned long)mm->end_code,
						(unsigned long)mm->start_data,(unsigned long)mm->end_data,
						(unsigned long)mm->start_brk,(unsigned long)mm->brk,
						path);
			
			
			}
			kfree(buf);
		}
		
	}

	up_read(&mm->mmap_sem); // unlock

	mmput(mm);// get_task_mm() 으로 참조 카운트 올렸으니 mmput()으로 카운트 감소

	return (long)target;



}


/*
 module init - system call 336 hooking 
*/
static int __init info_init(void)
{
	syscall_table = (void**) kallsyms_lookup_name("sys_call_table"); // sys_call_table 주소 가져오기

	make_rw(syscall_table); // 쓰기 권한 부여

	real_ftrace = syscall_table[336]; // 원래의 시스템콜 백업
	syscall_table[336] = file_varea; // 336번 시스템 콜 -> file_varea

	make_ro(syscall_table); // 다시 읽기 전용으로 설정
	return 0;
}

/*
 module exit - recover systemcall
 */
static void __exit info_exit(void)
{
	make_rw(syscall_table);

	syscall_table[336] = real_ftrace;; // reconstruct

	make_ro(syscall_table);
}

module_init(info_init);
module_exit(info_exit);
MODULE_LICENSE("GPL");



