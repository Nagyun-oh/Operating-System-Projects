#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h>		// fork(), exit()
#include <sys/wait.h>	// waitpid() 
#include <time.h>		// clock_gettime()

/*
* MAX_PROCESSES
* 
* Defines the number of child processes to be created.
* If not specified at compile time, defaults to 8.
*/
#ifndef MAX_PROCESSES
#define MAX_PROCESSES 8
#endif

/*
* read_input()
* 
* Reads 2* MAX_PROCESSES integers from "./temp.txt"
* and stores them in the provided array.
* 
* Returns:
*  0 on success
* -1 on failure
*/
static int read_input(int* out)
{
	FILE*f = fopen("./temp.txt","r");
	if(!f){
		perror("can't open temp.txt");
		return -1;
	}

	for(int i=0;i<MAX_PROCESSES*2;i++){
		if(fscanf(f,"%d",&out[i]) != 1){
			fclose(f);
			return -1;
		}
	}

	fclose(f);
	return 0;
}

/*
* Converts struct timespec to nanoseconds.
* Used for precise elapsed time measurement
*/
static inline long long ns(const struct timespec *ts){
	return (long long)ts->tv_sec *1000000000LL + ts->tv_nsec;
}

int main(){
	/*
	* Input array:
	* Each child process consumes two consecutive values.
	*/
	int nums[2*MAX_PROCESSES];
	if(read_input(nums)<0){
		fprintf(stderr,"input read error(pos : main() )\n");
		return 1;
	}

	struct timespec t0,t1; 
	clock_gettime(CLOCK_MONOTONIC,&t0);	 // start timing

	int total = 0;						 // final accumlated sum
	pid_t pids[MAX_PROCESSES];			 // child PID list

	/*
	* STEP 1:
	* Create MAX_PROCESSES child processes.
	*/
	for(int i = 0; i < MAX_PROCESSES; i++)
	{
		pid_t pid = fork();

		if(pid<0){
			perror("fork error");
			return 1;
		}

		/*
		* Child process:
		* Each child sums two numbers and exits,
		* passing the result via exit status
		*/
		if(pid == 0){
			int a = nums[2*i]; 
			int b = nums[2*i +1]; 
			int part_sum = a+b; 
	
			/*
			* exit status is limited to 8 bits (0 ~ 255),
			* so higher values are truncated
			*/
			_exit(part_sum & 0xFF); 
			
		}

		/* Parent process:
		*  Store child PID for later synchronization.
		*/
		pids[i] = pid;

	}
	
	/*
	* STEP 2:
	* Wait for all child processes and accumlate results.
	*/
	for(int i = 0; i <MAX_PROCESSES; i++){
		int st = 0;
		if(waitpid(pids[i],&st,0)<0){
			perror("waitpid");
			return 1;
		}

		if (WIFEXITED(st)) 
			total += WEXITSTATUS(st); 
		else{
			fprintf(stderr,"child %d abnormal\n",i);
			return 1;
		}
	}

	clock_gettime(CLOCK_MONOTONIC,&t1);			 // end timing

	double elapsed = (ns(&t1) - ns(&t0)) / 1e6 ; 

	printf("value of fork : %d\n",total);		 
	printf("%.6f\n",elapsed/1000.0);			 // seconds

	return 0;
}

