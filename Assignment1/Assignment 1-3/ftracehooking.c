#include "ftracehooking.h"

 
static asmlinkage long (*real_os_ftrace)(pid_t pid); // 원래의 os_ftrace system call 주소를 저장할 함수 포인터

/////////////////////////////////////////////
// 1. system call hooking function (os_ftrace() -> my_ftrace() )
///////////////////////////////////////////
asmlinkage long my_ftrace(pid_t pid)
{
   // printk(KERN_INFO "my_ftrace called with pid=%d\n", pid); // for debug
    if (traced_pid && pid ==traced_pid)
    {
      //  printk(KERN_INFO "OS Assignment 2 ftrace [%d] End\n", traced_pid);
        stop_io_trace();
        traced_pid = 0;
	return 0;
    } 
   
    if (pid > 0) { // START
       // printk(KERN_INFO "OS Assignment 2 ftrace [%d] Start\n",pid);
        start_io_trace(pid);
    }
    return 0;
}

////////////////////////////////////////////////
// 2. sys_call_table 수정 관련 함수
// ////////////////////////////////////////////

static unsigned long **sys_call_table; // 시스템 콜 테이블 주소 저장할 포인터

// 쓰기 허용으로 바꾸는 함수
static void make_rw(void *addr)
{
    unsigned int level;
    pte_t *pte = lookup_address((unsigned long)addr, &level);
    if (!(pte->pte & _PAGE_RW))
        pte->pte |= _PAGE_RW;
}

// 읽기 전용으로 바꾸는 함수
static void make_ro(void *addr)
{
    unsigned int level;
    pte_t *pte = lookup_address((unsigned long)addr, &level);
    pte->pte &= ~_PAGE_RW;
}


///////////////////////////////////// ///////////
// 3. 커널 모듈 초기화 (insmod 시 실행됨 )
// ///////////////////////////////////////////////

// 모듈 로드시 후킹
static int __init hook_init(void)
{

    // hijack 
    // sys_call_table의 실제 주소를 커널 심볼 테이블에서 검색
    sys_call_table = (unsigned long**)kallsyms_lookup_name("sys_call_table");
    
    // 예외처리
    if (!sys_call_table) {
        printk(KERN_ERR "Cannot find sys_call_table\n");
        return -1;
    }

    // 원래의 os_ftrace  시스템 콜 주소 백업해놓기
    real_os_ftrace = (void *)sys_call_table[548]; 
    
    // sys_call_table 을 쓰기 가능 상태로 변경 후 함수 주소를 교체
    make_rw(sys_call_table);
    sys_call_table[548] = (unsigned long*)my_ftrace; // 새로운 handler 등
    make_ro(sys_call_table);

    //printk(KERN_INFO "os_ftrace hooked to my_ftrace\n");
    return 0;
}

////////////////////////////////////////////
// 4. 커널 모듈 종료 (rmmod 시 실행됨)
// ////////////////////////////////////////
static void __exit hook_exit(void)
{
    // sys_call_table 이 유요하면 원래의 함수로 복원
    if (sys_call_table && real_os_ftrace)
    {
        make_rw(sys_call_table);
        sys_call_table[548] = (unsigned long*)real_os_ftrace;
        make_ro(sys_call_table);
       // printk(KERN_INFO "os_ftrace restored\n");
    }
}

///////////////////////////////
// 5. 모듈 메타 정보
// //////////////////////////

module_init(hook_init); // insmod 시 호출되는 함수 지정
module_exit(hook_exit); // rmmod 시 호출되는 함수 지정
MODULE_LICENSE("GPL"); //GPL 라이선스 명시
