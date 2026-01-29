#ifndef FTRACEHOOKING_H
#define FTRACEHOOKING_H
/*
* ftracehooking.h
* 
* Header file for ftrace-based I/O tracing module
* 
* This file declares:
*  - shared global variables
*  - cross-module fuction prototypes
*  - data structures used for I/O statistics collection
* 
* Source files using this header:
* - ftracehooking.c
* - iotracehooking.c
* 
*/

/* ============================================================
 *  Kernel headers
 * ============================================================ */

/* core kernel*/
#include <linux/module.h>
#include <linux/kernel.h>

/* ftrace framework */
#include <linux/ftrace.h>
#include <linux/kallsyms.h>

/* process / syscall */
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/syscalls.h>

/* file system */
#include <linux/fs.h>
#include <linux/uaccess.h>

/* memory management */
#include <linux/mm.h>
#include <asm/pgtable.h>
#include <asm/pgtable_types.h>


/* ============================================================
 *  Global variables
 * ============================================================ */

/*
* traced_pid
* 
* Process ID currently being traced
* All file I/O activities are filtered
* based on this PID
*/
extern pid_t traced_pid; 

/*
* start_io_trace - enable I/O tracing for a process
* @pid : target process ID
* 
* Installs ftrace hooks on file_related system calls
* such as open/read/write/close for the given process
* 
* Implemented in iotracehooking.c
*/
extern void start_io_trace(pid_t pid); 

/*
* stop_io_trace - disable I/O tracing
* 
* Removes all installed ftrace hooks and
* stops collecting I/O statistics
* 
* Implemented in iotracehooking.c
*/
extern void stop_io_trace(void); 

/*
* struct io_trace_stats - I/O tracing statistics
* 
* This structure stores statistics of file I/O operations
* performed by the traced process.
* 
* Counters are increased whenever the hooked functions
* are invoked through ftracehooking.c
*/
struct io_trace_stats{
	int		open_count;		/* number of open calls */
	int		close_count;	/* number of close calls */
	int		read_count;		/* number of read calls */
	int		write_count;	/* number of write calls */
	int		lseek_count;	/* number of lseek calls */

	size_t	read_bytes;		/* total bytes read */
	size_t	write_bytes;	/* total bytes written */

	char	filename[256];	/* last accessed file name */
};

/*stats
* 
* Global I/O statistics shared between modules
* Updated by ftrace hooks during runtime
*/
extern struct io_trace_stats stats; 

#endif

