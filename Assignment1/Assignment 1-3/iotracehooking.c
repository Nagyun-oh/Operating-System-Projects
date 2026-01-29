#include "ftracehooking.h"

/* ============================================================
 *  Global symbols
 * ============================================================ */
pid_t traced_pid = 0;
EXPORT_SYMBOL(traced_pid);

struct io_trace_stats stats;
EXPORT_SYMBOL(stats);

void** syscall_table;

/* ============================================================
 *  Original syscall pointers
 * ============================================================ */
static asmlinkage long (*real_openat)(const struct pt_regs* regs);
static asmlinkage long (*real_read)(const struct pt_regs* regs);
static asmlinkage long (*real_write)(const struct pt_regs *regs);
static asmlinkage long (*real_lseek)(const struct pt_regs *regs);
static asmlinkage long (*real_close)(const struct pt_regs *regs);

/* ============================================================
 *  Hooked syscall implementations
 * ============================================================ */

/*========================= open ===============================*/
static asmlinkage long ftrace_openat(const struct pt_regs *regs)
{
    long ret;       
	stats.open_count++; 

	if (stats.filename[0] == '\0') {
        const char __user *ufn = (const char __user *)regs->si; 
        long n = strncpy_from_user(stats.filename, ufn, sizeof(stats.filename)-1);
        if (n > 0) stats.filename[n] = '\0';
	}	    
    
    ret = real_openat ? real_openat(regs) : -ENOENT;
    return ret;

}

/*========================= read ===============================*/
static asmlinkage long ftrace_read(const struct pt_regs *regs)
{
    long ret;

    size_t count = (size_t) regs->dx; 

    stats.read_count++;
    stats.read_bytes +=count;
     
    ret = real_read ? real_read(regs) : -ENOENT;
    return ret;
}

/*========================= wrtie ===============================*/
static asmlinkage long ftrace_write(const struct pt_regs *regs)
{
    long ret;
  
    size_t count = (size_t) regs->dx; 

    stats.write_count++;
    stats.write_bytes +=count;
      
    ret = real_write ? real_write(regs) : -ENOENT;
    return ret;
}


/*========================= lseek ===============================*/
static asmlinkage long ftrace_lseek(const struct pt_regs *regs)
{
    long ret;
    
    stats.lseek_count++;
 
    ret = real_lseek ? real_lseek(regs) : -ENOENT;
    return ret;
}

/*========================= close ===============================*/
static asmlinkage long ftrace_close(const struct pt_regs *regs)
{
    long ret;
  
    stats.close_count++;

    ret = real_close ? real_close(regs) : -ENOENT;
    return ret;
}

/* ============================================================
 *  Page permission helpers
 * ============================================================ */
static int make_rw(unsigned long address)
{
    unsigned int level;
    pte_t *pte = lookup_address(address, &level);
    if(pte->pte &~ _PAGE_RW)
        pte->pte |= _PAGE_RW;
    return 0;
}

static int make_ro(unsigned long address)
{
    unsigned int level;
    pte_t *pte = lookup_address(address, &level);
    pte->pte = pte->pte &~ _PAGE_RW;
    return 0;
}


/* ============================================================
 *  Trace control
 * ============================================================ */

void start_io_trace(pid_t pid)
{
    traced_pid = pid;
    syscall_table = (void**) kallsyms_lookup_name("sys_call_table");

    make_rw((unsigned long)syscall_table); 

    real_openat =(void*) syscall_table[__NR_openat];
    real_read = (void*)syscall_table[__NR_read];
    real_write =(void*) syscall_table[__NR_write];
    real_lseek =(void*) syscall_table[__NR_lseek];
    real_close =(void*) syscall_table[__NR_close];

    syscall_table[__NR_openat] = ftrace_openat;
    syscall_table[__NR_read] = ftrace_read;
    syscall_table[__NR_write] = ftrace_write;
    syscall_table[__NR_lseek] = ftrace_lseek;
    syscall_table[__NR_close] = ftrace_close;

    make_ro((unsigned long)syscall_table); 

    printk(KERN_INFO "[iotrace] pid%d tracing start\n",pid);
}
EXPORT_SYMBOL(start_io_trace);

void stop_io_trace(void)
{
    if(!syscall_table) return;
 
    make_rw((unsigned long)syscall_table);

    if (real_openat) syscall_table[__NR_openat] = (void *)real_openat;
    if (real_read)   syscall_table[__NR_read]   = (void *)real_read;
    if (real_write)  syscall_table[__NR_write]  = (void *)real_write;
    if (real_lseek)  syscall_table[__NR_lseek]  = (void *)real_lseek;
    if (real_close)  syscall_table[__NR_close]  = (void *)real_close;

    make_ro((unsigned long)syscall_table);

    printk(KERN_INFO "[2021202089] %s file[%s] stats [x] read - %zd / written - %zd",
                    current->comm,stats.filename,stats.read_bytes,stats.write_bytes);
    printk(KERN_INFO "open[%d] close[%d] read[%d] write[%d] lseek[%d]",
                    stats.open_count,stats.close_count,stats.read_count,stats.write_count,stats.lseek_count);

    printk(KERN_INFO "OS Assignment2 ftrace [%d] End\n",current->pid);
}
EXPORT_SYMBOL(stop_io_trace);


/* ============================================================
 *  Module init / exit
 * ============================================================ */
static int __init iotrace_init(void)
{
    memset(&stats,0,sizeof(stats));
    printk(KERN_INFO "[iotrace] module loaded\n");
    return 0;
}
static void __exit iotrace_exit(void)
{
    stop_io_trace(); 
    printk(KERN_INFO "[iotrace] module unloaded\n");
}

module_init(iotrace_init);
module_exit(iotrace_exit);
MODULE_LICENSE("GPL");