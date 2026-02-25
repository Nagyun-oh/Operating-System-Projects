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

/* Global variables for proc directory entries */
struct proc_dir_entry *oslab_fp_dir;  // Target path: /proc/proc_2021202089
struct proc_dir_entry *oslab_fp_info; // Target path: /proc/proc_2021202089/processInfo

/* State variables for process filtering and read control */
static int oslab_is_processed = 0;	// Flag to prevent redundant output in a single read session
static int filter_pid = -1;			// PID to filter; -1 indicates printing all processes
static int write_flag = 0;			// Indicates whether a PID has been specified via write()

/**
 * oslab_open - Triggered when /proc/.../processInfo is opened
 * Resets the processed flag to allow new read operations.
 */
int oslab_open(struct inode* inode, struct file* file){
	oslab_is_processed = 0;
	return 0;
}

/**
 * append_info - Appends formatted process information to the buffer
 * @buf: Destination buffer in kernel space
 * @buf_size: Total size of the buffer
 * @offset: Current writing position (offset) in the buffer
 * @p: Pointer to the task_struct of the target process
 * * Returns the updated offset after appending.
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

	/* Retrieve CPU time in nanoseconds and convert to jiffies */
	task_cputime(p,&utime_ns,&stime_ns);
 	utime_ticks = nsecs_to_jiffies(utime_ns);
    stime_ticks = nsecs_to_jiffies(stime_ns);

	/* Get Process IDs and Credentials */
	pid = p->pid;
	ppid = p->real_parent ? p->real_parent->pid : 0;
	uid = __kuid_val(p->cred->uid);
	gid = __kgid_val(p->cred->gid);

	/* Get process state character and map to descriptive string */
	state = task_state_to_char(p);
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

	/* Append formatted string to buffer and update offset */
	offset += scnprintf(buf + offset, buf_size-offset,
			"%-7d %-7d %-7d %-7d %-10lu %-10lu %-1c %-15s %s\n",
			pid,ppid,uid,gid,utime_ticks,stime_ticks,state,
			state_str,p->comm);

	return offset;  
}


/**
 * oslab_read - Handler for read() syscall on the proc file
 * Iterates through task list and copies data to user space.
 */
ssize_t oslab_read(struct file* f,char __user* data_usr, size_t len, loff_t *off){
	
	char *buf;
	int buf_size = 4096 * 50; // Temporary kernel buffer (200 KB)
	struct task_struct *p;
	int offset = 0;
	
	/* Ensure the content is only read once per open session */
	if(oslab_is_processed) return 0;

	/* Allocate memory in kernel heap */
	buf = kzalloc(buf_size,GFP_KERNEL);
	if(!buf)return -ENOMEM;

	/* 1. Print Table Header */
	offset += scnprintf(buf + offset , buf_size - offset,
			"%-7s %-7s %-7s %-7s %-10s %-10s %-17s %s\n", 
			"Pid", "PPid",  "Uid" ,"Gid" ,"utime","stime","State","Name");
	offset += scnprintf(buf + offset , buf_size - offset,
			"---------------------------------------------------------------------------------------------\n");

	/* 2. Traverse tasks based on filter_pid */
	if(!write_flag || filter_pid == -1){
		/* Case: Print all processes */
		for_each_process(p){
			offset = append_info(buf,buf_size,offset,p);
			if(offset>= buf_size -200) break; // Buffer safety margin
		}
	}else{
		/* Case: Print specific process by PID */
		for_each_process(p){
			if(p->pid == filter_pid){
				offset = append_info(buf,buf_size,offset,p);
				break;
			}
		}
	}

	/* Boundary check: Do not exceed user-requested length */
	if(offset > len) offset = len; 

	/* Copy data from kernel space to user space */
	if(copy_to_user(data_usr,buf,offset)) return -EFAULT;

	kfree(buf);
	oslab_is_processed = 1; 

	return offset; 
}

/**
 * oslab_write - Handler for write() syscall on the proc file
 * Receives a PID from the user to filter subsequent read results.
 */
ssize_t oslab_write(struct file*f, const char __user *data_usr,size_t len, loff_t *off)
{
	char buf[32];
	ssize_t len_copied;
	int ret,pid_val;

	len_copied = len; // temp
	/* Copy data from user space to kernel space */
	if(copy_from_user(buf,data_usr,len_copied)) return -EFAULT; 
	buf[len_copied] = '\0'; 

	/* Convert string input to integer PID */
	ret = kstrtoint(buf,10,&pid_val);
	if(ret<0)
		filter_pid = -1; // Reset to 'all' if input is invalid
	else 
		filter_pid = pid_val; 

	write_flag = 1; 
	return len_copied;
}


/* File operations structure for the proc file */
const struct file_operations oslab_ops =  {
	.owner = THIS_MODULE,
	.open = oslab_open,   
	.read = oslab_read,   
	.write = oslab_write, 
};

/**
 * oslab_init - Module entry point
 * Creates the proc directory and the processInfo file.
 */
int __init oslab_init(void)
{
	oslab_fp_dir = proc_mkdir("proc_2021202089",NULL);
	oslab_fp_info = proc_create("processInfo",0666,oslab_fp_dir,&oslab_ops);

	return 0;
}

/**
 * oslab_exit - Module cleanup point
 * Removes the proc entries before unloading the module.
 */
void __exit oslab_exit(void)
{
	remove_proc_entry("processInfo",oslab_fp_dir); 
	remove_proc_entry("proc_2021202089",NULL);  
}

module_init(oslab_init);
module_exit(oslab_exit);
MODULE_LICENSE("GPL");






