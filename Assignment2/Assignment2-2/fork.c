#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> // fork(), exit()
#include <sys/wait.h> // waitpid() 
#include <time.h>    // clock_gettime()

// 프로세스 개수 기본값 설정
#ifndef MAX_PROCESSES
#define MAX_PROCESSES 8
#endif

static int read_input(int* out)
{
	// 입력 파일 열기
	FILE*f = fopen("./temp.txt","r");
	if(!f){
		perror("can't open temp.txt");
		return -1;
	}

	// 파일에서 숫자들을 읽어서 out  배열에 저장
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

// timespec 구조체를 나노초 단위 정수로 변환 (시간차 계산 쉽게 하기 위해)
static inline long long ns(const struct timespec *ts){
	return (long long)ts->tv_sec *1000000000LL + ts->tv_nsec;
}


int main()
{
	int nums[2*MAX_PROCESSES];
	if(read_input(nums)<0)
	{
		fprintf(stderr,"input read error(pos : main() )\n");
		return 1;
	}

	struct timespec t0,t1; // 시간 측정용 구조체
	clock_gettime(CLOCK_MONOTONIC,&t0); // start time

	int total = 0; // 전체 합 저장 변수
	pid_t pids[MAX_PROCESSES]; //  각 자식에 대한 PID 저장 배열

	// STEP.1 : MAX_PROCESS 만큼 프로세스 생성
	for(int i = 0; i < MAX_PROCESSES; i++)
	{
		pid_t pid = fork(); // 자식 생성

		if(pid<0){
			perror("fork error");
			return 1;
		}

		// Child Process Area : 
		if(pid == 0)
		{
			// STEP 2. 최상단 프로세스마다 2개의 숫자를 읽음
			int a = nums[2*i]; // 첫번째 수
			int b = nums[2*i +1]; // 두번째 수

			// STEP 3. 각 프로세스는 두개의 숫자를 더한 후 부모 프로세스에게 값을 전달
			int part_sum = a+b; 
			// 자식 프로세스 종료 (part_sum 이 255 넘으면 1바이트로 잘림)
			_exit(part_sum & 0xFF); // exit 코드는 0~255 사이의 정수만 전달 가능함.
			
		}

		// Parent Process Area : 방금 만든 자식 PID 저장
		pids[i] = pid;

	}

	// =================================
	
	// 모든 자식 프로세스의 종료를 기다리고 결과 합산
	for(int i = 0; i <MAX_PROCESSES; i++)
	{
		// 특정 PID 자식 기다리기 
		int st = 0;
		if(waitpid(pids[i],&st,0)<0)
		{
			perror("waitpid");
			return 1;
		}

		// WIFEEXITED(정상종료) 가 되었으면
		if (WIFEXITED(st)) 
			total += WEXITSTATUS(st); // exit 코드 읽어서 합산
		else{
			fprintf(stderr,"child %d abnormal\n",i);
			return 1;
		}
	}

	clock_gettime(CLOCK_MONOTONIC,&t1); // end time

	double elapsed = (ns(&t1) - ns(&t0)) / 1e6 ;  // ns -> ms

	printf("value of fork : %d\n",total); // total 
	printf("%.6f\n",elapsed/1000.0); // 초 단위로 시간 출력

	return 0;
}

