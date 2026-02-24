/*
* CPU Scheduling Simulator
* 
* Supports algorithms:
* - FCFS (First-come, First-Served, Non-preemptive)
* - SJF  (Shortest Job First, Non-preemptvie)
* - SRTF (Shortest Remaining Time First, Preemptive)
* - Round Robin (Time-Quantum based, Preemptive)
* 
* Design focuses on:
* - Accurate modeling of context switch overhead
* - Precise measurement of response / waiting / turnaround times 
* - Gantt chart visualization via execution time segments
* 
* Time unit: milliseconds (ms)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ------------------------------ Configuration ----------------------------- */
#define MAX_TASKS 1000        // Maximum number of processes
#define CS_OVERHEAD 0.1       //  Context switch overhead 

/* Special PIDs for Gantt chart visualization */
#define PID_IDLE -1           //  CPU idle period
#define PID_CS -2             //  Context switch interval

/* ------------------------------ Process Model ------------------------------ */

/*
 * Proc
 * ----
 * Represents a process in the scheduling simulation.
 */
typedef struct 
{
	int pid;                // Logical process ID
	int arrival;            // Arrival time (ms)
	int burst;              // Total CPU burst time (ms)
	double remain;          // Remaining execution time (for preemptive algorithms)
	double first_start;     // First time this process gets CPU (response time)
	double finish;          // Completion time
	int started;            // Whether the porcess has ever been scheduled
	int done;               // Completion flag
}Proc;

/* Global process table */
static Proc P[MAX_TASKS];
static int N =0; 

/* ------------------------------ Gantt Segments ----------------------------- */

/*
* Segment
* 
* Represents a continuous execution interval on the CPU.
* Used to build a Gantt chart with context switch and idle periods
*/
typedef struct{
	double start;
	double end;
	int pid;        // PID_IDLE, PID_CS , or actual process pid
}Segment;

static Segment *segs = NULL;  // Dynamic segment array
static int segc = 0;          // Segment count
static int segcap = 0;        // Segment capacity

/*
* add_seg()
* 
* Records a CPU execution interval [start,end) for a given pid
* If the new segment is contiguous with the previous one and
* has the same pid, the segments are merged to reduce fragmentation
*/
static void add_seg(double start, double end, int pid){
    if (end <= start) return; 

    /* Merge with previous segment if possible */
    if (segc && (fabs(segs[segc-1].end - start) < 1e-9 )&& (segs[segc-1].pid == pid))
    {
        segs[segc-1].end = end;
        return;
    }
      
    /* Expand segment array if needed */
    if (segc == segcap)
    {    
        segcap = segcap ? segcap * 2 : 128; 
        segs = (Segment*)realloc(segs, sizeof(Segment)*segcap);
    }

    segs[segc++] = (Segment){start, eend, pid}; 
}


/* ------------------------------ Utility Functions -------------------------- */

/*
 * Sort processes by arrival time.
 * Ties are broken by PID.
 */
static int cmp_arrival(const void* a, const void* b)
{
	const Proc *x = a;
	const Proc* y = b;
	if(x->arrival != y->arrival ) return x->arrival - y->arrival; 
	else return x->pid - y->pid;  
}

/* Check if any process is ready at time now */
static int any_ready(double now)
{
	for(int i = 0;i<N;i++)
		if (!P[i].done && P[i].arrival <= now + 1e-9) return 1;
	return 0;
}

/* Check whether all processes have finished */
static int all_done()
{
	for(int i = 0;i <N; i++) 
		if(!P[i].done) return 0;
		
	return 1;
}

/* Find the next arrival after time now */
static int next_arrival_after(double now){
    int found = -1;
    for (int i=0;i<N;i++){
        if (!P[i].done && P[i].arrival > now + 1e-9){
            if (found==-1 || P[i].arrival < P[found].arrival)
		    found = i;
        }
    }
    return found;
}


/* ------------------------------ Scheduling Policies ------------------------- */

/*
* pick_FCFS()
* 
* Selects the next process to run according to FCFS policy
* Among ready processes:
* - Earliest arrival time wins
* - Ties are broken by smaller PID
*/
static int pick_FCFS(double now)
{
	int best = -1;
	for(int i = 0; i <N; i++){
		if(!P[i].done && P[i].arrival <= now + 1e-9)
		{
			if(best == -1) best = i;
			else{
				if(P[i].arrival < P[best].arrival)best = i;
				else if(P[i].arrival == P[best].arrival && P[i].pid < P[best].pid) best = i;
			}
		}
	}
	return best;
}

/*
 * SJF selection:
 * Shortest total burst time among ready processes.
 */
static int pick_SJF(double now){
    int best = -1;
    for (int i=0;i<N;i++){
        if (!P[i].done && P[i].arrival <= now + 1e-9){
            if (best==-1 || P[i].burst < P[best].burst ||
               (P[i].burst==P[best].burst && (P[i].arrival<P[best].arrival ||
                 (P[i].arrival==P[best].arrival && P[i].pid<P[best].pid))))
                best=i;
        }
    }
    return best;
}

/*
 * SRTF selection:
 * Shortest remaining execution time among ready processes.
 */
static int pick_SRTF(double now){
    int best = -1;
    for (int i=0;i<N;i++){
        if (!P[i].done && P[i].arrival <= now + 1e-9){
            if (best==-1 || P[i].remain < P[best].remain ||
               (fabs(P[i].remain-P[best].remain)<1e-9 && (P[i].arrival<P[best].arrival ||
                (P[i].arrival==P[best].arrival && P[i].pid<P[best].pid))))
                best=i;
        }
    }
    return best;
}

/* ------------------------------ Gantt Chart -------------------------------- */
/*
 * print_gantt()
 * 
 * Prints a simplified Gantt chart using 1ms granularity.
 * Context switch intervals are skipped for clarity.
 * 
 * makespan : simulation total time
 */
static void print_gantt(double makespan)
{   
    printf("Gantt Chart:\n|");
    for (int t = 0; t < (int)ceil(makespan-1e-9); t++)
    {
        int pid = PID_IDLE;
        for (int i=0;i<segc;i++){
            if (segs[i].start <= t + 1e-9 && segs[i].end > t + 1e-9) 
	        {
                pid = segs[i].pid;
                break;
            }
        }
        if (pid == PID_CS) continue;    // Skip Context switching Overhead
        printf(" P%d |", pid);           
    }
    printf("\n");
}

/* ------------------------------ Statistics --------------------------------- */

/*
 * Waiting time    = Finish - arrival - burst
 * Response time   = FirstStart - arrival
 * Turnaround time = Finish - arrival
 * CPU utilization = (sum of bursts / makespan) * 100
 */
static void print_stats(){
    double sum_wait=0, sum_resp=0, sum_turn=0, sum_of_burst=0, finish_max=0;
    for (int i=0;i<N;i++)
    {
        double wait = P[i].finish - P[i].arrival - P[i].burst;
        double resp = P[i].first_start - P[i].arrival;
        double turn = P[i].finish - P[i].arrival;
        sum_wait += wait;
        sum_resp += resp;
        sum_turn += turn;
        sum_of_burst += P[i].burst;   
        if (P[i].finish > finish_max) finish_max = P[i].finish; 
    }
    double avgW = sum_wait/N, avgR = sum_resp/N, avgT = sum_turn/N;
    double util = (finish_max>0)? (sum_of_burst/finish_max)*100.0 : 0.0;

    printf("Average Waiting Time =  %.2f\n", avgW);
    printf("Average Turnaround Time = %.2f\n",avgT);
    printf("Average Response Time = %.2f\n", avgR);     
    printf("CPU Utilization = %.2f %%\n", util);
}


/*
 * FCFS
 *
 * Non-preemptive scheduling:
 * Once a process starts executing, it runs until completion.
 * Context switch overhead is applied only when switching
 * between different processes.
 */
static void simulate_FCFS(){
    double now = 0.0;           // current simulation time (ms)
    int prev_pid = PID_IDLE;    // PID that ran previously

    /*
     * Run until all processes are completed
     */
    while(!all_done()){
        /*
         * 1) If no process is ready, keep CPU idle
         *    until the next process arrives
         */
        if (!any_ready(now)){
            int nx = next_arrival_after(now); 
            if (nx<0) break;  
            add_seg(now, P[nx].arrival, PID_IDLE); 
            now = P[nx].arrival; 
        }

        /*
        * 2) Select a ready process according to FCFS rule
        */
        int k = pick_FCFS(now);
       
        /*
         * 3) Apply context switch overhead if execution entity changes
         */
        if (prev_pid != P[k].pid && prev_pid != PID_IDLE){
            add_seg(now, now+CS_OVERHEAD, PID_CS); 
            now += CS_OVERHEAD;
        }

        /*
         * 4) Record first start time (used for response time calculation)
         */
        if (!P[k].started){
	       	P[k].first_start = now;
	       	P[k].started=1;
       	}

        / /*
         * 5) FCFS is non-preemptive:
         *    run the selected process until completion
         */
        add_seg(now, now + P[k].remain, P[k].pid); 
        now += P[k].remain; 
        P[k].remain = 0;
        P[k].done = 1;
        P[k].finish = now;  // completion time

        /*
         * 6) Update previously executed PID
         */
        prev_pid = P[k].pid;
    }

    /*
     * Print Gantt chart and scheduling statistics
     */
    print_gantt(now);
    print_stats();
}


/*
 * SJF
 * 
 * Non-preemptive scheduling:
 *  - Among ready processes, pick the one with the shortest remaining time.
 *  - Once selected, the process runs until completion.
 */
static void simulate_SJF(){
    double now = 0.0;
    int prev_pid = PID_IDLE;

    while(!all_done())
    {   /* 1) If no process is ready, CPU stays IDLE until next arrival */
        if (!any_ready(now))
	    {
            int nx = next_arrival_after(now);
            if (nx<0) break;
            add_seg(now, P[nx].arrival, PID_IDLE);
            now = P[nx].arrival;
        }

	    /* 2) Pick a ready process using SJF rule */
        int k = pick_SJF(now);
       
	    /* 3) Apply context switch overhead if switching */
        if (prev_pid != P[k].pid && prev_pid != PID_IDLE) {
            add_seg(now, now+CS_OVERHEAD, PID_CS);
            now += CS_OVERHEAD;
        }
	    /* 4) Record first start time (only once) */
        if (!P[k].started){
	       	P[k].first_start = now;
	       	P[k].started=1;
       	}

	    /*5) Non-preemptive execution until completion*/
        add_seg(now, now + P[k].remain, P[k].pid);
        now += P[k].remain;

        P[k].remain = 0; 
	    P[k].done = 1;
       	P[k].finish = now;

        prev_pid = P[k].pid;
    }


    print_gantt(now);
    print_stats();
}

/*
 * SRTF
 * Preemptive version of SJF:
 *  - At any time, the process with the shortest remaining time runs.
 *  - A newly arrived process may preempt the running one.
 */
static void simulate_SRTF()
{
    double now = 0.0;
    int running_pid = PID_IDLE; // PID currently occupying the CPU

    while(!all_done())
    {
        /* 1) If nothing is ready, stay idle until next arrival */
        if (!any_ready(now)){
            int nx = next_arrival_after(now);
            if (nx<0) break; 
            add_seg(now, P[nx].arrival, PID_IDLE); 
            now = P[nx].arrival; 
        }

        /* 2) Select process with shortest remaining time */
        int k = pick_SRTF(now);

        /* 3) Context switch if process changes */
        if (running_pid != P[k].pid){
            if (running_pid != PID_IDLE) {
                add_seg(now, now + CS_OVERHEAD, PID_CS);
                now += CS_OVERHEAD;
            }   
            if (!P[k].started){
                P[k].first_start = now;
                P[k].started=1;
            }
        }

        /*
         * 4) Determine next event:
         *    - process finishes
         *    - or another process arrives
         */
        int nx = next_arrival_after(now);
        double next_event = (nx<0)? (now + P[k].remain)
                                  : fmin(now + P[k].remain, (double)P[nx].arrival);

        /*
        * 5) Execute until next event
        */
        add_seg(now, next_event, P[k].pid);

        double ran = next_event - now; 
        P[k].remain -= ran;
        now = next_event;

        /*
         * 6) If finished, mark completion
         */
        if (P[k].remain <= 1e-9){  
            P[k].remain = 0; 
            P[k].done=1;
            P[k].finish = now;
            running_pid = PID_IDLE; 
        }
        else {
            running_pid = P[k].pid;
        }
    }

    print_gantt(segs[segc-1].end);
    print_stats();
}

/*
 * Round Robin 
 * Preemptive scheduling with time quantum:
 *  - Each process runs for at most tq milliseconds.
 *  - Unfinished processes are pushed to the back of the queue.
 */

 /* Simple circular queue for RR */
typedef struct 
{
	int q[MAX_TASKS];
    int f; // front inext
	int r; // rear index
} Queue;

static void q_init(Queue*Q){Q->f=Q->r=0;}
static int  q_empty(Queue*Q){return Q->f==Q->r;}
static void q_push(Queue*Q,int x){Q->q[Q->r++]=x;}
static int  q_pop (Queue*Q){return Q->q[Q->f++];}

/*
 * Enqueue all processes that arrive during (from, to]
 */
static void enqueue_arrivals(Queue*Q, double from, double to)
{
    for (int i=0;i<N;i++)
    {    
        if ((!P[i].done) && (from + 1e-9 < P[i].arrival) && (P[i].arrival <= to + 1e-9)){
            q_push(Q, i);
        }
    }
}

/*
 * Round Robin simulation
 */
static void simulate_RR(int tq)
{ 
    double now = 0.0;
    Queue Q; 
    q_init(&Q); 

    /*
     *  1) Fill initial idle time if first arrival > 0
     */
    int nx = next_arrival_after(-1.0);
    if (nx>=0 && P[nx].arrival > 0){
        add_seg(0, P[nx].arrival, PID_IDLE);
        now = P[nx].arrival;
    }
    
    /*
     * 2) Push all processes that already arrived
     */
    for (int i=0;i<N;i++)
        if (P[i].arrival <= now + 1e-9) 
            q_push(&Q, i);

    int prev = PID_IDLE; 

    while(!all_done())
    {
        if (q_empty(&Q)){
            /*
             *  3) Queue empty → idle until next arrival
             */
            int j = next_arrival_after(now);
            if (j<0) break;

            add_seg(now, P[j].arrival, PID_IDLE);
            now = P[j].arrival; 
            prev = PID_IDLE; 

            for (int i=0;i<N;i++)
                if (!P[i].done && P[i].arrival <= now + 1e-9) q_push(&Q, i);
            continue;
        }

        /*
         *  4) Pop next process
         */
        int k = q_pop(&Q);
        if (P[k].done) continue; // Skip already done process

        /*
         *  5) Context switch handling
         */
        if (prev != P[k].pid)
        {	
            if(prev != PID_IDLE){
            	add_seg(now, now+CS_OVERHEAD, PID_CS);
            	now += CS_OVERHEAD;
            } 
        }

        /*
         * 6) Record first execution
         */
        
        if (!P[k].started){ 
            P[k].first_start = now;
	       	P[k].started=1;
       	}

        /*
         * 7) Execute for one time quantum or until completion
         */
        double run_for = fmin((double)tq, P[k].remain);
        double end_time = now + run_for;  

        add_seg(now, end_time, P[k].pid);
        enqueue_arrivals(&Q, now, end_time);

        now = end_time; 
        P[k].remain -= run_for; 

        /*
        * 8) Completion check
        */
        if (P[k].remain <= 1e-9) {
            P[k].remain=0;
            P[k].done=1;
            P[k].finish=now;
        }else{
            q_push(&Q, k);
        }
	
        prev = P[k].pid;
    }

    print_gantt(segs[segc-1].end);
    print_stats();
}

/*
 * main()
 *
 * Usage:
 *   ./scheduler input_file {FCFS|SJF|SRTF|RR} [time_quantum]
 *
 * Input format:
 *   pid arrival_time burst_time
 */
int main(int argc, char**argv){
   
    if (argc < 3){
        fprintf(stderr, "Usage: %s input_file {FCFS|RR|SJF|SRTF} [time_quantum]\n", argv[0]);
        return 1;
    }
    const char* in = argv[1];
    const char* alg = argv[2];
    int tq = 0;

    /* RR requires argv[3] */
    if (!strcmp(alg,"RR")){
        if (argc < 4){ fprintf(stderr,"RR requires time_quantum (ms)\n"); return 1; }
        tq = atoi(argv[3]);
        if (tq<=0){ fprintf(stderr,"time_quantum must be positive\n"); return 1; }
    }

    FILE* f = fopen(in, "r");
    if (!f){ 
        perror("Errpr : Open input file "); 
        return 1; 
    }
 
    while (true){
        int pid, arr, bur;
        int r = fscanf(f, "%d %d %d", &pid, &arr, &bur);
        if (r!=3) break;
        P[N].pid=pid; P[N].arrival=arr; P[N].burst=bur;
        P[N].remain=(double)bur; P[N].started=0; P[N].done=0;
        P[N].first_start=0.0; P[N].finish=0.0;
        N++;
        if (N>=MAX_TASKS) break;
    }

    fclose(f);
    if (N==0){
        fprintf(stderr,"No tasks found in %s\n", in); 
        return 1;
    }

    qsort(P, N, sizeof(Proc), cmp_arrival);

    if (!strcmp(alg,"FCFS")) simulate_FCFS();
    else if (!strcmp(alg,"SJF")) simulate_SJF();
    else if (!strcmp(alg,"SRTF")) simulate_SRTF();
    else if (!strcmp(alg,"RR")) simulate_RR(tq);
    else { 
        fprintf(stderr,"Unknown algorithm: %s\n", alg); 
        return 1; 
    }

    return 0;
}