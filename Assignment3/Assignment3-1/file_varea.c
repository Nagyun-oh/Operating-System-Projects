#include <asm/ptrace.h>	// struct pt_regs
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/highmem.h>
#include <linux/sched/mm.h>	// get_task_mm()
#include <linux/mm_types.h>
#include <linux/kallsyms.h>
#include <linux/init_task.h>
#include <linux/slab.h>	// kmalloc , kfree
#include <linux/fs.h>
#include <linux/dcache.h>	// d_path()
#include <linux/path.h>
#include <linux/limits.h>	// PATH_MAX
#include <linux/mm.h>	// vm_area_struct 

void **syscall_table;	// To store the address of the sys_call_table
void *real_ftrace;		// Backup for the original system call

/* ============================================================
 *  Page permission helpers
 * ============================================================ */
void make_rw(void *addr){

	unsigned int level;
	pte_t *pte = lookup_address((unsigned long)addr,&level);

	if(!(pte->pte & _PAGE_RW))
		pte->pte |= _PAGE_RW;
}

void make_ro(void* addr){

	unsigned int level;
	pte_t *pte = lookup_address((unsigned long)addr,&level);
	pte->pte &=~ _PAGE_RW;
}

/**
 * file_varea - Hooked system call implementation (replaces syscall 336)
 * @regs: User-space registers passed via system call
 * * Retrieves process memory layout (Code, Data, Heap) and prints
 * absolute paths of memory-mapped files to the kernel log.
 */
static asmlinkage long file_varea(const struct pt_regs *regs)
{
	pid_t target = (pid_t)regs->di;	// PID passed from user-space

	struct task_struct *task; 
	struct mm_struct *mm; 
	struct vm_area_struct *vma;
	
	// Locate task_struct for the given PID
	task = pid_task(find_vpid(target),PIDTYPE_PID);
	if(!task) return -ESRCH;
	
	// Get memory descriptor; increments reference count
	mm = get_task_mm(task);
	if(!mm) return -EINVAL;	

	printk(KERN_INFO "######## Loaded files of a process' %s(%d)' in VM ########\n",
			task->comm,target);

	// Acquire read lock to ensure consistency while traversing the VMA
	down_read(&mm->mmap_sem);

	// Iterate through all Virtual Memory Areas (VMAs) of the process
	for(vma = mm->mmap; vma; vma= vma -> vm_next){
		// Process only VMAs that are backed by a file
		if(vma->vm_file){
			char*buf = kmalloc(PATH_MAX,GFP_KERNEL);
			if(!buf){
				printk(KERN_INFO "file_varea : kmalloc PATH_MAX failed\n");
				break;
			}

			// Resolve dentry/vfsmount into a full absoulte path string
			char *path = d_path(&vma->vm_file->f_path,buf,PATH_MAX);
			// Print memory area and file path
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
	up_read(&mm->mmap_sem);	// Release VMA lock
	mmput(mm);				// Decrement mm_struct reference count

	return (long)target;
}

/*
*  info_init - Module entry point
*  Finds the sys_call_table address and hooks system call index 336
*/
static int __init info_init(void){

	// Look up the system call table address by symbol name
	syscall_table = (void**) kallsyms_lookup_name("sys_call_table"); 

	make_rw(syscall_table); // Enable write access to the table
	real_ftrace = syscall_table[336];	// Backup original system call
	syscall_table[336] = file_varea;	// Overwrite with hook function
	make_ro(syscall_table);				// Restore read-only protection
	return 0;
}

/**
 * info_exit - Module exit point
 * Restores the original system call function to index 336.
 */
static void __exit info_exit(void){

	make_rw(syscall_table);
	syscall_table[336] = real_ftrace; // Restore original function pointer
	make_ro(syscall_table);
}

module_init(info_init);
module_exit(info_exit);
MODULE_LICENSE("GPL");



