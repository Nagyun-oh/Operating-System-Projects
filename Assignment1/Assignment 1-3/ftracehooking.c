#include "ftracehooking.h"

/* ============================================================
 *  Global variables
 * ============================================================ */

/* syscall number */
#define __NR_OS_FTRACE 336

/* original syscall pointer */
typedef asmlinkage long(*os_ftrace_t)(pid_t);

static os_ftrace_t real_os_ftrace;
static unsigned long **sys_call_table;

/* ============================================================
 *  Page permission helpers
 * ============================================================ */
static void make_rw(void* addr)
{
    unsigned int level;
    pte_t* pte = lookup_address((unsigned long)addr, &level);

    if (!pte) return;
    if (!(pte->pte & _PAGE_RW))pte->pte |= _PAGE_RW;
}

static void make_ro(void* addr)
{
    unsigned int level;
    pte_t* pte = lookup_address((unsigned long)addr, &level);

    if (!pte) return;
    pte->pte &= ~_PAGE_RW;
}

/* ============================================================
 *  Hooked os_ftrace syscall
 * ============================================================ */
asmlinkage long my_ftrace(pid_t pid)
{
    /* stop tracing */
    if (traced_pid && pid ==traced_pid){
        stop_io_trace();
        traced_pid = 0;
	    return 0;
    } 
   
    /* start tracing */
    if (pid > 0) { 
        start_io_trace(pid);
    }

    return 0;
}


/* ============================================================
 *  Module init / exit
 * ============================================================ */
static int __init hook_init(void)
{
    sys_call_table = (unsigned long**)kallsyms_lookup_name("sys_call_table");
    
    if (!sys_call_table) {
        printk(KERN_ERR "[ftrace] sys_call_table not found\n");
        return -EINVAL;
    }

    /* backup original syscall */
    real_os_ftrace = (os_ftrace_t)sys_call_table[__NR_OS_FTRACE]; 
    
    make_rw(sys_call_table);
    sys_call_table[__NR_OS_FTRACE] = (unsigned long*)my_ftrace; 
    make_ro(sys_call_table);

    printk(KERN_INFO "[ftrace] os_ftrace hooked\n");
    return 0;
}

static void __exit hook_exit(void)
{
    if (!sys_call_table || !real_os_ftrace) return;
     
     make_rw(sys_call_table);
     sys_call_table[__NR_OS_FTRACE] = (unsigned long*)real_os_ftrace;
     make_ro(sys_call_table);
     
     printk(KERN_INFO "[ftrace] os_ftrace restored\n");
    
}

module_init(hook_init); 
module_exit(hook_exit); 
MODULE_LICENSE("GPL"); 
