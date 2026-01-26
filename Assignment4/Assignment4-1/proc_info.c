#include <linux/module.h>
#include <linux/init.h>
#include <linux/syscalls.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/sched/signal.h>
#include <linux/uidgid.h>
#include <linux/sched/cputime.h>

struct proc_dir_entry *oslab_fp_dir;  // For ->   /proc/proc_2021202089
struct proc_dir_entry *oslab_fp_info;  // For ->  proc/proc_2021202089/processInfo

static int oslab_is_processed = 0; // 한 번 read()가 끝났는지 여부 확인용
static int filter_pid = -1;// write를 통해 넘어온 PID 값 저장, -1이면 프로세스 전체 출력
static int write_flag = 0; // proc 파일에 wrtie 요청이 있었는지 여부 확인 ( 0 이면 no write )


// /proc/proc_2021202089/processinfo 파일을 열 때마다 수행
int oslab_open(struct inode* inode, struct file* file){
	oslab_is_processed = 0;
	return 0;
}


/*
 하나의 task_struct 에 대해서,
 - PID, PPID , UID, GID, utime, stime, state, name 을 buf 에 한줄로 출력
 - buf : 출력 버퍼 시작 주소
 - buf_size : 버퍼 총 크기
 - offset : 현재까지 buf 에 쌓인 바이트 수
 - p : 대상  프로세스의 tast_struct pointer
 - 반환값 : 새로 출력된 만큼 offset 이 증가된 값
  */
static int append_info(char*buf,int buf_size, int offset, struct task_struct *p)
{
	pid_t pid,ppid;
	uid_t uid;
	gid_t gid;
	u64 utime_ns= 0 ; u64 stime_ns = 0;
	char state;
	unsigned long utime_ticks = 0;
	unsigned long stime_ticks = 0;
	const char* state_str;

	// kernel이 관리하는 cputime (ns)를 얻어옴
	task_cputime(p,&utime_ns,&stime_ns);
	
	// ns 단위를 jiffies(커널 틱 단위)로 변환함.
 	utime_ticks = nsecs_to_jiffies(utime_ns);
        stime_ticks = nsecs_to_jiffies(stime_ns);

	// PID , PPID
	pid = p->pid;
	ppid = p->real_parent ? p->real_parent->pid : 0;

	// UID, GID 
	uid = __kuid_val(p->cred->uid);
	gid = __kgid_val(p->cred->gid);

	// task 상태를 문자로 얻음 ( R,S,D,T,Z.. etc) 
	state = task_state_to_char(p);

	// 상태 문자에 따라서 과제 출력형식에 맞게 문자열을 매핑시킴
	switch(state){
		case 'R' : state_str = "(running)"; break;
		case 'S' : state_str = "(sleeping)"; break;
		case 'D' : state_str = "(disk sleep)"; break;
		case 'T' : state_str = "(stopped)"; break; 
		case 't' : state_str = "(tracing stop)"; break;	
		case 'X' : state_str = "(dead)"; break;
		case 'Z' : state_str = "(zombie)"; break;
		case 'P' : state_str = "(parked)"; break;
		case 'I' : state_str = "(idle)"; break;
		default: state_str = "";	
	}

	// offset 갱신
	offset += scnprintf(buf + offset, buf_size-offset,
			"%-7d %-7d %-7d %-7d %-10lu %-10lu %-1c %-15s %s\n",
			pid,ppid,uid,gid,utime_ticks,stime_ticks,state,
			state_str,p->comm);

	return offset;  // 업데이트된 offset 반환

}


//      /proc/proc_2021202089/processinfo 파일에 읽기 연산을 요청할때마다 수행
// For  cat /proc/proc_2021202089/processInfo
ssize_t oslab_read(struct file* f,char __user* data_usr, size_t len, loff_t *off){
	
	char *buf;
	int buf_size = 4096 * 50; // 커널 내부에서 사용할 임시 버퍼 설정 (200 KB) 
	struct task_struct *p;
	int offset = 0;
	
	// 한번 읽고나면, 다음 read() 에서는 반복 출력 방지
	if(oslab_is_processed) return 0;

	// 커널 힙에서 버퍼할당
	buf = kzalloc(buf_size,GFP_KERNEL);
	if(!buf)return -ENOMEM;

	//1)  print header
	offset += scnprintf(buf + offset , buf_size - offset,
			"%-7s %-7s %-7s %-7s %-10s %-10s %-17s %s\n", 
			"Pid", "PPid",  "Uid" ,"Gid" ,"utime","stime","State","Name");
	offset += scnprintf(buf + offset , buf_size - offset,
			"---------------------------------------------------------------------------------------------\n");

	// 1) write x or 모든 프로세스 ( filter_pid ==-1)
	if(!write_flag || filter_pid == -1)
	{
		for_each_process(p){
			offset = append_info(buf,buf_size,offset,p);
			if(offset>= buf_size -200) break; // 버퍼가 거의 다 찼으면 탈출 
		}
	}	
	// 2) 특정 PID만 출력
	else{
		for_each_process(p){
			if(p->pid == filter_pid){
				offset = append_info(buf,buf_size,offset,p);
				break;
			}
		}
	
	}

	if(offset > len) offset = len; // 요청한 길이(len) 보다 많이 만든 경우,  len까지만 잘라서 보내기

	// 커널 버퍼 -> user 버퍼
	if(copy_to_user(data_usr,buf,offset)) return -EFAULT;

	kfree(buf); // 커널 버퍼 해제
	oslab_is_processed = 1; // 한번만 읽히도록 flag설정

	return offset; // 유저에게 전달한 byte 수 리턴
}

// For echo (num) > /proc/proc_2021202089/processInfo
ssize_t oslab_write(struct file*f, const char __user *data_usr,size_t len, loff_t *off)
{
	char buf[32]; // 임시 버퍼
	ssize_t len_copied;
	int ret,pid_val;

	len_copied = len; // 요청한 길이 복사

	// user space -> kernel space
	if(copy_from_user(buf,data_usr,len_copied)) return -EFAULT; 

	buf[len_copied] = '\0'; // 끝에 널 문자 추가


	// 문자열(num)을 정수로 변환 
	ret = kstrtoint(buf,10,&pid_val);
	if(ret<0)filter_pid = -1; // 아닌 경우 -> 전체 출력
	else filter_pid = pid_val; // pid 입력받은 경우

	write_flag = 1; // write 했으니 1로 설정

	return len_copied;

}


// proc 파일 시스템에 등록할 구조체
const struct file_operations oslab_ops =  {
	.owner = THIS_MODULE,
	.open = oslab_open,   // 파일 open시 호출
	.read = oslab_read,   // 파일  read시 호출
	.write = oslab_write, // 파일  write시 호출
};

// 모듈이 로드될 때 실행
int __init oslab_init(void)
{
	oslab_fp_dir = proc_mkdir("proc_2021202089",NULL);
	oslab_fp_info = proc_create("processInfo",0666,oslab_fp_dir,&oslab_ops);

	return 0;
}

// 모듈이 제거될때 실행 
void __exit oslab_exit(void)
{
	remove_proc_entry("processInfo",oslab_fp_dir); // 파일 entry  제거
	remove_proc_entry("proc_2021202089",NULL);  // 디렉토리 entry 제거
}

module_init(oslab_init);
module_exit(oslab_exit);
MODULE_LICENSE("GPL");






