#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>   

/*
* MAX_PROCESSES
*
* Defines the number of worker threads.
* If not specified at compile time, defaults to 8.
*/
#ifndef MAX_PROCESSES
#define MAX_PROCESSES 8
#endif

/*
* Task
* 
* Each thread receives a pair of integers
* to be summed independently
*/
typedef struct{
	int a,b;
}Task;

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
static int read_input(int* out){ 
    FILE*f = fopen("./temp.txt","r");
    if(!f){
       perror("can't open temp.txt");
        return -1;
    }

    for(int i=0;i<MAX_PROCESSES*2;i++)
    {   
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

/*
* worker()
* 
* Thread entry function
* Receives a Task structure, computes the sum,
* and returns the result via dynamically allocated memory.
*/
static void* worker(void*arg)
{
	Task* t = (Task*)arg; 
	int* ret = (int*)malloc(sizeof(int)); 

	*ret = t->a + t->b;  
	return (void*)ret; 
}

int main()
{
    int nums[2*MAX_PROCESSES];
    if(read_input(nums)<0)
    {
        fprintf(stderr,"input read error(pos : main() )\n");
        return 1;
    }

    struct timespec t0,t1; 
    clock_gettime(CLOCK_MONOTONIC,&t0);     // start timing

    pthread_t th[MAX_PROCESSES];            // thread identifiers

    Task tasks[MAX_PROCESSES];              // per- thread task data

    /*
    * STEP 1:
    * Create MAX_PROCESSES worker threads
    */
    for(int i = 0; i < MAX_PROCESSES; i++){
	    tasks[i].a = nums[2*i];
	    tasks[i].b = nums[2*i + 1];

        if(pthread_create(&th[i],NULL,worker,&tasks[i]) != 0){
			perror("pthread create");
			return 1;
        }
    }

    /*
    * STEP 2:
    * Join all threads and accmulate results.
    */
	int total = 0;
    for(int i = 0; i <MAX_PROCESSES; i++){
		void *res = NULL;

		if(pthread_join(th[i],&res) != 0){
		    perror("pthread_join");
			return 1;    
        }

		total += *(int*)res;
		free(res); 
    }

    clock_gettime(CLOCK_MONOTONIC,&t1);         // end timing

    double elapsed = (ns(&t1) - ns(&t0)) / 1e6;

    printf("value of thread : %d\n",total); 
    printf("%.6f\n",elapsed/1000.0);            // seconds

    return 0;
}

