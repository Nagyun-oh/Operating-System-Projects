#include "ftracehooking.h"

MODULE_LICENSE("GPL");

//////////////////////////////////////////////////
// 1. 전역 변수 및 심볼 등록 
///////////////////////////////////////////////////
pid_t traced_pid = 0;
EXPORT_SYMBOL(traced_pid);

struct io_trace_stats stats;
EXPORT_SYMBOL(stats);

// 각 시스템 콜의 원본 함수 포인터 백업
static asmlinkage long (*real_openat)(const struct pt_regs *regs) = NULL;
static asmlinkage long (*real_read)(const struct pt_regs *regs) = NULL;
static asmlinkage long (*real_write)(const struct pt_regs *regs) = NULL;
static asmlinkage long (*real_lseek)(const struct pt_regs *regs) = NULL;
static asmlinkage long (*real_close)(const struct pt_regs *regs) = NULL;

//////////////////////////////////////////////////
// 2. 후킹 함수 정의 (openat -> fopenat, write -> fwrite ....)
///////////////////////////////////////////////////

// openat()
static asmlinkage long ftrace_openat(const struct pt_regs *regs)
{
            long ret;

       
	    stats.open_count++; // open 호출 횟수 증가

	    // 파일명 복사
	    if (stats.filename[0] == '\0') 
	    {
            const char __user *ufn = (const char __user *)regs->si; // 2nd arg
            long n = strncpy_from_user(stats.filename, ufn, sizeof(stats.filename)-1);
            if (n > 0) stats.filename[n] = '\0';

	    }	    
    
    // 실제 시스템 콜 실행
    if(real_openat)ret = real_openat(regs);
    else ret = -ENOENT;

    return ret;

}

// read()
static asmlinkage long ftrace_read(const struct pt_regs *regs)
{
    long ret;

    size_t count = (size_t) regs->dx; // read  bytes

   
    stats.read_count++;
    stats.read_bytes +=count;
   

    if(real_read)ret = real_read(regs);
    else ret = -ENOENT;

    return ret;

}

// write()
static asmlinkage long ftrace_write(const struct pt_regs *regs)
{

    long ret;
   
    size_t count = (size_t) regs->dx; // write bytes 

  
    stats.write_count++;
    stats.write_bytes +=count;
    
    
    if(real_write)ret = real_write(regs);
    else ret = -ENOENT;

    return ret;
}


// lseek()
static asmlinkage long ftrace_lseek(const struct pt_regs *regs)
{
    long ret;
    
    
    
    stats.lseek_count++;
    
    
    if(real_lseek)ret = real_lseek(regs);
    else ret = -ENOENT;

    return ret;
}

//close()
static asmlinkage long ftrace_close(const struct pt_regs *regs)
{
    long ret;
  
    stats.close_count++;
    

    if(real_close) ret = real_close(regs);
    else ret = -ENOENT;


    return ret;

}


//////////////////////////////////////////////////
// 3. sys_call_table -> 쓰기 전용 / 읽기 전용 변환 함수
///////////////////////////////////////////////////

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


//////////////////////////////////////////////////
// 4.  start_io_trace() : hooking open(), read(), write(), lseek(), close()
///////////////////////////////////////////////////

void **syscall_table;

void start_io_trace(pid_t pid)
{

    traced_pid = pid;
    syscall_table = (void**) kallsyms_lookup_name("sys_call_table");

    make_rw((unsigned long)syscall_table); // 쓰기 권한 부여


    // 각 시스템 콜의  원본 주소  백업
    real_openat =(void*) syscall_table[__NR_openat];
    real_read = (void*)syscall_table[__NR_read];
    real_write =(void*) syscall_table[__NR_write];
    real_lseek =(void*) syscall_table[__NR_lseek];
    real_close =(void*) syscall_table[__NR_close];

    // 커널의 시스템 콜 테이블을 ftrace_* 로 바꾸기
    syscall_table[__NR_openat] = ftrace_openat;
    syscall_table[__NR_read] = ftrace_read;
    syscall_table[__NR_write] = ftrace_write;
    syscall_table[__NR_lseek] = ftrace_lseek;
    syscall_table[__NR_close] = ftrace_close;

    make_ro((unsigned long)syscall_table); //시스템콜 테이블 읽기전용으로 바꾸기


    printk(KERN_INFO "OS Assignment2 ftrace [%d] Start\n",current->pid);
}
EXPORT_SYMBOL(start_io_trace);

//////////////////////////////////////////////////
// 5. 후킹 종료 및 복원
///////////////////////////////////////////////////

void stop_io_trace(void)
{

    if(!syscall_table) return;
 
    make_rw((unsigned long)syscall_table);

    // 원본 시스템 콜 복원
    if (real_openat) syscall_table[__NR_openat] = (void *)real_openat;
    if (real_read)   syscall_table[__NR_read]   = (void *)real_read;
    if (real_write)  syscall_table[__NR_write]  = (void *)real_write;
    if (real_lseek)  syscall_table[__NR_lseek]  = (void *)real_lseek;
    if (real_close)  syscall_table[__NR_close]  = (void *)real_close;

    make_ro((unsigned long)syscall_table);


   // 커널 로그 출력
    printk(KERN_INFO "[2021202089] %s file[%s] stats [x] read - %zd / written - %zd",
                    current->comm,stats.filename,stats.read_bytes,stats.write_bytes);
    printk(KERN_INFO "open[%d] close[%d] read[%d] write[%d] lseek[%d]",
                    stats.open_count,stats.close_count,stats.read_count,stats.write_count,stats.lseek_count);


    printk(KERN_INFO "OS Assignment2 ftrace [%d] End\n",current->pid);
}
EXPORT_SYMBOL(stop_io_trace);


//////////////////////////////////////////////////
// 6. 모듈 초기화 및 종료 
///////////////////////////////////////////////////

static int __init iotrace_init(void)
{
    printk(KERN_INFO "iotracehooking loaded (ready)\n");
    memset(&stats,0,sizeof(stats)); // stats 구조체 초기화
    return 0;
}
static void __exit iotrace_exit(void)
{
    stop_io_trace(); 
    printk(KERN_INFO "iotracehooking unloaded\n");
}
module_init(iotrace_init);
module_exit(iotrace_exit);
