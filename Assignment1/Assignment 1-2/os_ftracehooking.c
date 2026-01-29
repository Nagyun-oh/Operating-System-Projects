#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kallsyms.h> // kallsyms_lookup_name() 

/* ============================================================
 *  Global variables & typedefs
 * ============================================================ */
#define __NR_os_ftrace 548

/* function pointer type for os_ftrace syscall */
typedef asmlinkage long(*os_ftrace_t)(pid_t);

/* pointer to original os_ftrace syscall */
static os_ftrace_t real_os_ftrace;

/* sys_call_table address resolved via kallsyms */
static unsigned long** sys_call_table;

/* ============================================================
 *  Page permission helpers
 * ============================================================ */
static void make_rw(void *addr)
{
	unsigned int level;
	pte_t *pte = lookup_address((unsigned long)addr,&level);

	if (!pte) return;
	if(!(pte->pte & _PAGE_RW)) pte->pte |= _PAGE_RW;
}

static void make_ro(void* addr)
{
	unsigned int level;
	pte_t *pte = lookup_address((unsigned long)addr,&level);

	if (!pte)return;

	pte->pte &=~ _PAGE_RW;
}


/* ============================================================
 *  Syscall hook function
 * ============================================================ */

/*
* my_ftrace - hooked syscall wrapper
* @pid: target process id
* 
* This function replaces the original os_ftrace syscall
* in sys_call_table . After logging control is forwarded
* to the original syscall implementation
*/
static asmlinkage long my_ftrace(pid_t pid)
{
	printk(KERN_INFO "os_ftrace() hooked! os_ftrace -> my_ftrace");
	return real_os_ftrace(pid);
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
static int  __init hook_init(void)
{
	sys_call_table = (unsigned long**)kallsyms_lookup_name("sys_call_table");

	if(!sys_call_table)
	{
		printk(KERN_ERR "Couldn't find sys_call_table\n");
		return -EINVAL;
	}

	real_os_ftrace = (os_ftrace_t)sys_call_table[__NR_os_ftrace];
	printk(KERN_INFO "saving real_os_ftrace = %px\n", real_os_ftrace);

	make_rw(sys_call_table);
	sys_call_table[__NR_os_ftrace] = (unsigned long*)my_ftrace; 
	make_ro(sys_call_table);

	printk(KERN_INFO "os_ftrace hooked!\n");

	return 0;
}

/*
* hook_exit - module unload routine
* 
* Restore original syscall pointer to prevent
*/
static void __exit hook_exit(void)
{
	if (!sys_call_table || !real_os_ftrace) return;
	
	make_rw(sys_call_table);
	sys_call_table[__NR_os_ftrace] = (unsigned long*)real_os_ftrace;
	make_ro(sys_call_table);

	printk(KERN_INFO "os_ftrace restored\n");
	
}

module_init(hook_init);
module_exit(hook_exit);
MODULE_LICENSE("GPL");



















