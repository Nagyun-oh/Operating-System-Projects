#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>    // clock_gettime()

// 프로세스 개수 기본값 설정
#ifndef MAX_PROCESSES
#define MAX_PROCESSES 8
#endif

typedef struct{
	int a,b;
}Task;

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

static void* worker(void*arg)
{
	Task* t = (Task*)arg; // thread마다 전달받은 인자 -> Task 로 캐스팅

	int* ret = (int*)malloc(sizeof(int)); // 결과값을 저장할 공간 동적으로 할당
	*ret = t->a + t->b;  // 두 숫자 더하기
	return (void*)ret; // 결과값에 대한 주소 반환
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

        pthread_t th[MAX_PROCESSES]; //    Thread 저장 배열

	Task tasks[MAX_PROCESSES];  // 각 스레드의 작업 구조체

        // STEP.1 : MAX_PROCESS 만큼 Thread 생성
        for(int i = 0; i < MAX_PROCESSES; i++)
        {
 		// 각 스레드에 두 숫자 할당
		tasks[i].a = nums[2*i];
		tasks[i].b = nums[2*i + 1];

		if(pthread_create(&th[i],NULL,worker,&tasks[i]) != 0)
		{
			perror("pthread create");
			return 1;
		}
        }

        // =================================

        // 모든 스레드 종료를 기다리고 결과 합산
	int total = 0;
        for(int i = 0; i <MAX_PROCESSES; i++)
        {
		void *res = NULL;

		// 해당 스레드 종료까지 대기
		if(pthread_join(th[i],&res) != 0)
		{
			perror("pthread_join");
			return 1;
		}

		// 반환된  결과값 더하기
		total += *(int*)res;
		free(res); // heap memory 해제
        }

        clock_gettime(CLOCK_MONOTONIC,&t1); // end time

        double elapsed = (ns(&t1) - ns(&t0)) / 1e6 ;  // ns -> ms

        printf("value of thread : %d\n",total); // total 
        printf("%.6f\n",elapsed/1000.0); // 초 단위로 시간 출력

        return 0;
}

