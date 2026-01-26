#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_TASKS 1000 // 최대 큐 길이 1000
#define CS_OVERHEAD 0.1 // Context switch overhead 
#define PID_IDLE -1 // for Gantte chart idle
#define PID_CS -2 // for Gantte Chart Context switch interval

// ======  Process State ======
typedef struct 
{
	int pid; // 고유 PID
	int arrival; //도착 시간
	int burst; // 총  CPU 요청량
	double remain; // 남은 실행시간
	double first_start; // 첫 실행 시각
	double finish; // 종료 시각
	int started; // 처음 실행여부
	int done;  // 완료 여부
}Proc;

static Proc P[MAX_TASKS];
static int N =0; 


// ======= 실행 구간 기록 =======
typedef struct{
	double start;
	double end;
	int pid;
}Segment;

static Segment *segs = NULL;
static int segc = 0;
static int  segcap = 0;

// 구간 추가 (+바로 앞 구간과 동일 pid & 이어지면 병합)
// 시각 s부터  e까지 PID가  cpu를 점유한 구간 기록 
static void add_seg(double s, double e, int pid){
    if (e <= s) return; // 시간차 0 이하는 무시

    // 직전 구간과 pid 같고 시간 연속이면 병합
    if (segc && fabs(segs[segc-1].end - s) < 1e-9 && segs[segc-1].pid == pid)
    {
        segs[segc-1].end = e;
        return;
    }
    
   
    if (segc == segcap)
    {    
        segcap = segcap ? segcap * 2 : 128; // 꽉 찼으면, 용량 2배 확장 또는 처음 할당이면 128칸 할당
        segs = (Segment*)realloc(segs, sizeof(Segment)*segcap);
    }

    segs[segc++] = (Segment){s, e, pid}; // 새 구간 추가 
}


// ==========  유틸리티 함수 ===========


// 도착 시각 → PID 타이브레이커 정렬
static int cmp_arrival(const void* a, const void* b)
{
	const Proc *x = a;
	const Proc* y = b;
	if(x->arrival != y->arrival ) return x->arrival - y->arrival; // 도착 시각 기준으로 오름차순 
	else return x->pid - y->pid;  // 도착 시각이 같은 경우 PID순으로 정렬
}

// 현재 시각 now 에 도착했고 아직 끝나지 않은 작업이 존재하면 1 반환 
static int any_ready(double now)
{
	for(int i = 0;i<N;i++)
		if (!P[i].done && P[i].arrival <= now + 1e-9) return 1; // 도착 시각이 현재 시각보다 작거나 같은 경우 
	return 0;
}

// 모두 끝났는지 확인
static int all_done()
{
	for(int i = 0;i <N; i++) 
		if(!P[i].done) return 0;
		
	return 1;
}


// now 이후로 가장 먼저 도착하는 작업의 인덱스(없으면 -1)
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


// ===== non preemptive =====

// FCFS : 준비된 작업중 도착시각 오름차순
// 가장 먼저 도착한 준비 작업 고르기 , 동시 도착이면 PID 오름차순으로 결정 
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

// SJF: 준비된 작업 중 burst가 가장 짧음(타이: arrival → pid)
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

// SRTF: 준비된 작업 중 남은 시간이 가장 짧음(타이: arrival → pid)
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


// ------------------------------ 간트차트 출력 ------------------------------
// 내부는 double로 기록했지만, 과제 요구대로 1ms 스텝으로 직관적 출력
static void print_gantt(double makespan)
{   // makespan : 총 실행 시간 
	
    printf("Gantt Chart:\n|");
    // 1ms단위로 탐색 
    for (int t = 0; t < (int)ceil(makespan-1e-9); t++)
    {
        // 시각 t+ε가 포함되는 세그먼트를 찾아 pid 결정
        int pid = PID_IDLE;
        for (int i=0;i<segc;i++){
            if (segs[i].start <= t + 1e-9 && segs[i].end > t + 1e-9) // start <= t < end일시 -> pid가  cpu 사용중인 것
	    {
                pid = segs[i].pid;
                break;
            }
        }
        if (pid == PID_CS) continue;    // 컨텍스트 스위치
        //else if (pid == PID_IDLE) printf("ID |"); // 유휴 시간
        printf(" P%d |", pid);            // 실제 실행
    }
    printf("\n");
}


// ------------------------------ 통계 계산/출력 -----------------------------
// waiting  = finish - arrival - burst // 대기 queue에서 기다린 총 시간
// response = first_start - arrival // response time
// turnaround = finish - arrival // turnaround time 
// utilization = (Σ burst / makespan) * 100
static void print_stats(){
    double sum_wait=0, sum_resp=0, sum_turn=0, work_sum=0, finish_max=0;
    for (int i=0;i<N;i++)
    {
        double wait = P[i].finish - P[i].arrival - P[i].burst;
        double resp = P[i].first_start - P[i].arrival;
        double turn = P[i].finish - P[i].arrival;
        sum_wait += wait;
        sum_resp += resp;
        sum_turn += turn;
        work_sum += P[i].burst;   // 총 cpu 이용시간 
        if (P[i].finish > finish_max) finish_max = P[i].finish; // 가장 늦게 끝난 시각 -> 전체 스케줄 길이
    }
    double avgW = sum_wait/N, avgR = sum_resp/N, avgT = sum_turn/N;
    double util = (finish_max>0)? (work_sum/finish_max)*100.0 : 0.0;

    printf("Average Waiting Time =  %.2f\n", avgW);
    printf("Average Turnaround Time = %.2f\n",avgT);
    printf("Average Response Time = %.2f\n", avgR);     
    printf("CPU Utilization = %.2f %%\n", util);
}


// ------------------------------ FCFS 시뮬레이터 ----------------------------
static void simulate_FCFS(){
    double now = 0.0;      // 시뮬레이션 현재 시각 (ms) 
    int prev_pid = PID_IDLE; // 직전에 실행한 주체

    // 모든 작업이 끝날 때까지 반복
    while(!all_done()){
        // 1) 준비된 것이 없다면 다음 도착까지 idle
        if (!any_ready(now)){
            int nx = next_arrival_after(now); // now 이후 가장 빠른 도착
            if (nx<0) break;  // 더이상 도착이 없으면 종료
            add_seg(now, P[nx].arrival, PID_IDLE); // IDLE 구간을 간트에 추가
            now = P[nx].arrival; // 시각을 다음 도착 시각으로 점프
        }

	// 2) 준비된 작업 중에서 FCFS 룰에 따라 선택
        int k = pick_FCFS(now);
       
        // 3)  실행 주체가 바뀌면 오버헤드 부과
        if (prev_pid != P[k].pid && prev_pid != PID_IDLE){
            add_seg(now, now+CS_OVERHEAD, PID_CS); //  Context Switch 구간 기록
            now += CS_OVERHEAD;
        }

        // 4)  첫 실행이면, Response time 측정용 first_start 기록
        if (!P[k].started){
	       	P[k].first_start = now;
	       	P[k].started=1;
       	}

        // 5) FCFS는  비선점: 남은 시간 전체를 한번에  실행
        add_seg(now, now + P[k].remain, P[k].pid); // 실행 구간 기록
        now += P[k].remain; // 끝 시각으로 이동
        P[k].remain = 0;
        P[k].done = 1;
        P[k].finish = now; // 종료 시각 확정

	// 6) 직전실행  PID 갱신 
        prev_pid = P[k].pid;
    }

    // 간트차트/ 통계 출력 
    print_gantt(now);
    print_stats();
}


// ------------------------------ SJF 시뮬레이터 -----------------------------
static void simulate_SJF(){
    double now = 0.0;
    int prev_pid = PID_IDLE;

    while(!all_done())
    {  // 1) 준비된 작업이 없을 시, 다음 도착까지 IDLE 
        if (!any_ready(now))
	{
            int nx = next_arrival_after(now);
            if (nx<0) break;
            add_seg(now, P[nx].arrival, PID_IDLE);
            now = P[nx].arrival;
        }

	// 2) 준비된 작업 중에 SJF 규칙으로 하나 선택 
        int k = pick_SJF(now);
       
	// 3) 실행 주체가 바뀔 시 Context Switch Overhead 부과 
        if (prev_pid != P[k].pid && prev_pid != PID_IDLE)
	{
            add_seg(now, now+CS_OVERHEAD, PID_CS);
            now += CS_OVERHEAD;
        }
	// 4) 첫 실행일시 first_Start time 기록 
        if (!P[k].started){
	       	P[k].first_start = now;
	       	P[k].started=1;
       	}

	// 5) SJF : non - preemtive
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

// ------------------------------ SRTF 시뮬레이터 ----------------------------
// preemptive : 현재 실행중이더라도, 더 짧은 남은 시간의 작업이 오면 교체 
// 더 짧은 남은 시간이 생기면 선점(교체 시 오버헤드).
static void simulate_SRTF()
{
    double now = 0.0;
    int running_pid = PID_IDLE; // 현재 CPU를 점유 중인 PID(없으면 IDLE)

    while(!all_done())
    {
        // 1)  준비된 게 없으면 다음 도착까지 idle
        if (!any_ready(now))
	{
            int nx = next_arrival_after(now);
            if (nx<0) break; // 더이상 도착 되는 작업이 없을시 종료 
            add_seg(now, P[nx].arrival, PID_IDLE); // 잉여 구간 간트에 추가 
            now = P[nx].arrival; 
        }

        // 2) 현재 시점에서 남은 시간이 가장 짧은 작업 선택
        int k = pick_SRTF(now);

        // 3)  다른 작업으로 교체되는 순간 → 오버헤드
        if (running_pid != P[k].pid)
	{
	    // Context switch 추가 
		if(running_pid !=PID_IDLE){	
		add_seg(now,now+CS_OVERHEAD,PID_CS);
		now += CS_OVERHEAD;
		}

            //CS 여부와 관계없이 갱신 및 기록 수행
            
	    // first start time  기록 
            if (!P[k].started){
		    P[k].first_start = now;
		    P[k].started=1;
	    }

	    
        }

	

        // 다음 이벤트 시각 결정(preemptive):
        //   1) 현재 작업이 끝나는 시각(now + remain)
        //   2) 다음 작업이 새로 도착하는 경우
        int nx = next_arrival_after(now);
        double next_event = (nx<0)? (now + P[k].remain)
                                  : fmin(now + P[k].remain, (double)P[nx].arrival);

        // 해당 구간 실행
        add_seg(now, next_event, P[k].pid);
        double ran = next_event - now; // 실제 실행량
        P[k].remain -= ran;
        now = next_event;

        // 종료했으면 마킹하고 running_pid 비움
        if (P[k].remain <= 1e-9)
	{
            P[k].remain = 0;
	    P[k].done=1;
	    P[k].finish = now;
            running_pid = PID_IDLE; // IDLE로 설정하여 CPU가 비었음을 표시 
        }
        
	running_pid = P[k].pid;
    }

  

    print_gantt(segs[segc-1].end);
    print_stats();
}

// ------------------------------ Round Robin -------------------------------
// time_quantum(ms)만큼씩 실행 후 큐 뒤로. 실행 중 도착은 즉시 큐에 삽입.

// queue 구조체 정의 및 함수 정의 
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

// 새로 도착한 작업들을 모두 큐에 추가 
static void enqueue_arrivals(Queue*Q, double from, double to)
{
    for (int i=0;i<N;i++)
    {    // ( from ,to] 사이에 있는 작업들을 큐에 삽입 (실행 중에도 도착한 작업은 즉시 큐에 합류)
        if (!P[i].done && P[i].arrival > from + 1e-9 && P[i].arrival <= to + 1e-9){
            q_push(Q, i);
        }
    }
}

// Round Robin 
static void simulate_RR(int tq){ // tq: time_quantum(ms)

    // 1) 초기 설정 : 실행 대기 큐 초기화 
    double now = 0.0;
    Queue Q; q_init(&Q); 

    // 2) 첫 도착이 0보다 뒤면 idle로 채움
    int nx = next_arrival_after(-1.0);
    if (nx>=0 && P[nx].arrival > 0){
        add_seg(0, P[nx].arrival, PID_IDLE);
        now = P[nx].arrival;
    }
    // 3) 초기 큐 채우기 : now까지 도착한 모든 프로세스들을 큐에 삽입 
    for (int i=0;i<N;i++) if (P[i].arrival <= now + 1e-9) q_push(&Q, i);

    int prev = PID_IDLE; // 직전 실행 PID

    // 4) 메인 루프 
    while(!all_done())
    {
        if (q_empty(&Q)){
            //  a)  큐가 비면 다음 도착까지 idle
            int j = next_arrival_after(now);
            if (j<0) break;
            add_seg(now, P[j].arrival, PID_IDLE);
            now = P[j].arrival;
	    prev = PID_IDLE; // IDLE 직후엔 CS 생략 
            for (int i=0;i<N;i++)
                if (!P[i].done && P[i].arrival <= now + 1e-9) q_push(&Q, i);
            continue;
        }

	// b) 큐에서 프로세스 꺼내기 
        int k = q_pop(&Q);
        if (P[k].done) continue; // 이미 끝난 프로세스면  스킵(중복 방어)

        // c) 컨텍스트 스위치 기록 
        if (prev != P[k].pid)
	{
           //  첫 프로세스 실행시에는 컨텍스트 스위치 부과 x 
	    
	   	if(prev != PID_IDLE){
            	add_seg(now, now+CS_OVERHEAD, PID_CS);
            	now += CS_OVERHEAD;
		}
        }
	// d) first start time 기록 
        if (!P[k].started){ 
		P[k].first_start = now;
	       	P[k].started=1;
       	}

        //e)  이번 퀀텀 동안 실행할 시간 결정 
        double run_for = fmin((double)tq, P[k].remain);
        double end_time = now + run_for; // 남은시간이 적으면 끝까지 실행하고 아니면 tq만큼 실행 

	// f) 이번 time quantum동안 cpu를 점유한 구간 기록 
        add_seg(now, end_time, P[k].pid);

        // g) 실행 중 도착한 프로세스는 즉시 큐 뒤에 삽입
        enqueue_arrivals(&Q, now, end_time);

	// h) 실행 시간 반영 
        now = end_time; // 시뮬레이션 시각 진행시키고
        P[k].remain -= run_for; // 실행된 만큼 남은 시간 줄이기 

	// i) 프로세스 종료 체크 
        if (P[k].remain <= 1e-9)
	{
            // 작업 종료
            P[k].remain=0;
	    P[k].done=1;
	    P[k].finish=now;
        }
	else
	{
            // 잔여가 있으면 뒤로 보냄
            q_push(&Q, k);
        }
	// j) 이전 실행 PID 갱신 -> for next Context Switch 
        prev = P[k].pid;
    }

    print_gantt(segs[segc-1].end);
    print_stats();
}

// ------------------------------ main --------------------------------------
int main(int argc, char**argv){
    // 명령행 인자 파싱
    if (argc < 3){
        fprintf(stderr, "Usage: %s input_file {FCFS|RR|SJF|SRTF} [time_quantum]\n", argv[0]);
        return 1;
    }
    const char* in = argv[1];
    const char* alg = argv[2];
    int tq = 0;
    if (!strcmp(alg,"RR")){
        if (argc < 4){ fprintf(stderr,"RR requires time_quantum (ms)\n"); return 1; }
        tq = atoi(argv[3]);
        if (tq<=0){ fprintf(stderr,"time_quantum must be positive\n"); return 1; }
    }

    // 입력 파일 읽기
    FILE* f = fopen(in, "r");
    if (!f){ perror("open input"); return 1; }

    // 각 줄: pid arrival burst
    while (1){
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
    if (N==0){ fprintf(stderr,"No tasks found in %s\n", in); return 1; }

    // 도착 시각(동률 시 pid) 오름차순 정렬
    qsort(P, N, sizeof(Proc), cmp_arrival);

    /* 머리말 출력
    printf("Loaded %d tasks. Algorithm=%s", N, alg);
    if (!strcmp(alg,"RR")) printf(" (q=%dms)", tq);
    printf("\nContext switch overhead: %.1f ms\n", CS_OVERHEAD);
*/
    // 알고리즘 분기
    if (!strcmp(alg,"FCFS")) simulate_FCFS();
    else if (!strcmp(alg,"SJF")) simulate_SJF();
    else if (!strcmp(alg,"SRTF")) simulate_SRTF();
    else if (!strcmp(alg,"RR")) simulate_RR(tq);
    else { fprintf(stderr,"Unknown algorithm: %s\n", alg); return 1; }

    return 0;
}





