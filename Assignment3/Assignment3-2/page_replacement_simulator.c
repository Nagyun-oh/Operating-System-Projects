#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_FRAMES 1000 

typedef struct {
    int *refs; // 실제 Page Number가 들어있는 배열
    int n; // 참조열 길이
} RefSeq;

/*
- 입력 파일을 읽기: 
	Page frame size 과 Reference string을 읽는다.
	path : 입력 파일 경로, frames : 프레임 개수를 저장할 포인터
*/
static int read_input(const char *path, int *frames, RefSeq *seq) {

    // 파일 열기
    FILE *fp = fopen(path, "r");
     
    // 프레임 개수 입력받기
    if (fscanf(fp, "%d", frames) != 1) {
        fclose(fp);
	fprintf(stderr,"wrong input-1\n");
        return -1;
    }

    // 프레임 개수 유효성 검사
    if (*frames <= 0 || *frames > MAX_FRAMES) {
        fclose(fp);
	fprintf(stderr,"Wrong input-2\n");
        return -1;
    }

    // Reference string array 동적할당
    int cap = 128;
    seq->refs = (int*)malloc(sizeof(int)*cap);
    seq->n = 0;

    // 파일 끝까지 페이지 번호 읽기
    int x;
    while (fscanf(fp, "%d", &x) == 1) 
    {
	    // 만약 초기 용량보다 커지면, 배열 크기 재할당
        if (seq->n == cap) 
	{
            cap *= 2;
            seq->refs = (int*)realloc(seq->refs, sizeof(int)*cap);
        }

        seq->refs[seq->n++] = x; // 페이지 번호 참조열 배열에 저장
    }


    fclose(fp);
    return 0;
}


// 현재 frame 배열에 특정 페이지가 환있는지 탐색
// frames : 프레임 배열, f : 프레임 개수, page : 찾으려는 페이지 번호
static int find_in_frames(const int *frames, int f, int page) 
{
    for (int i = 0; i < f; i++) if (frames[i] == page) return i; // 해당 페이지가 들어있는 인덱스 발견시 반환
    return -1; // 없을경우에는 -1 반
}

/*
  Optimal Page Replacement
  - F : Frame 개수, seq : page reference stirng

  Logic :
 	- HIT일 경우: continue
        - Miss일 경우:
		(1) 아직 프레임이 덜 찬 경우에는 빈 프레임 사용
		(2) 다 찬 경우에는 가장 나중에 다시 쓰이거나, 또는 아예 안쓰이는 페이지를 found로 설정
  반환값:
	총 Page Fault 횟수  
*/
static int simulate_opt(int F, const RefSeq *seq) 
{
    // HIT/MISS 판정을 위한 배열 설정
    int *frames = (int*)malloc(sizeof(int)*F);
    for (int i = 0; i < F; ++i) frames[i] = -1; // 비어있는 경우 == -1 

    int faults = 0; // Page faluts
    int filled = 0; // 현재까지 채워진 frames


    for (int k = 0; k < seq->n; k++) 
    {
        int p = seq->refs[k]; // 현재 참조하는 페이지

        int hit_idx = find_in_frames(frames, F, p);
        if (hit_idx != -1) continue;            // hit인 경우, 아무 작업 없이 다음 참조로 넘기기

        faults++; // MISS 

	// 1) 프레임에 아직 빈 자리가 있는 경우, 그 자리에 넣기
        if (filled < F) {                      
            frames[filled++] = p;
            continue;
        }

        // 2) 프레임이 다 찬 경우, 교체할 대상 찾기
	// 	-  앞으로 아예 안나오는 페이지인  경우 교체
	// 	-  페이지가  모두 나오는 경우 가장 나중에 나오는 페이지를 교체
        int found = 0;
	int furthest = -1; // 가장 먼 미래에 대한 인덱스 변수
        for (int i = 0; i < F; i++) {
            int next = -1;
		
	    // search
            for (int j = k+1; j < seq->n; j++) {
                if (seq->refs[j] == frames[i]) {
		       	next = j; 
			break;
	       	}
            }

	    // -  앞으로 아예 안나오는 경우 바로 교체 대상 설정
            if (next == -1) 
	    {
		    found = i;
		    break; 
	    }   

	    // - 페이지가 모두 나오는 경우에 대해서, 가장 나오는 페이지를 찾기 위해서 계속 설정
            if (next > furthest) {
		    furthest = next; 
		    found = i; 
	    }
        }

        frames[found] = p;
    }

    free(frames);
    return faults;
}

/*
	FIFO : First in Fist Out
	idx : 가장 오래된 페이지를 가리키는 인덱스 ( ~ queue.front() )
 */
static int simulate_fifo(int F, const RefSeq *seq) {

    int *frames = (int*)malloc(sizeof(int)*F);
    for (int i = 0; i < F; i++) frames[i] = -1;

    int faults = 0; 
    int idx = 0; // 어느 프레임을 다음에 교체할지 가리키는 변수
    int  filled = 0; // 채워진 프레임 수

    for (int t = 0; t < seq->n; t++) 
    {
        int p = seq->refs[t];  //현재 참조하는 페이지

        if (find_in_frames(frames, F, p) != -1) continue; // hit인 경우 아무작업도 하지 않음

        faults++; // MISS

	// 아직 프레임이 비어있으면, 그 자리에 채우기
        if (filled < F) {
            frames[filled++] = p;
        } else {
            frames[idx] = p; // 다 찬 경우 가장 오래된 프레임 교체
            idx = (idx + 1) % F; // idx를 한칸 이동
        }
    }

    free(frames);
    return faults;
}

/*
 LRU(Least Recentyl USed)

	Logic:
		- frames[i] : i번째 프레임에 어떤 페이지가 들어있는지
		- last[i] : 해당 프레임이 마지막으로 사용된 시점
		- 교체 시 last[i] 값이 가장 작은(가장 오래전에 사용된) 프레임을 교체


 */
static int simulate_lru(int F, const RefSeq *seq) 
{
    // 동적 할당
    int *frames = (int*)malloc(sizeof(int)*F);
    int *last   = (int*)malloc(sizeof(int)*F);

    // 비어있음을 -1로 설정
    for (int i = 0; i < F; i++) {
	    frames[i] = -1;
	    last[i] = -1; 
    }

    int faults = 0;
    int  filled = 0;

    for (int t = 0; t < seq->n; t++) {

        int p = seq->refs[t]; // 현재 참조하는 페이지
        int idx = find_in_frames(frames, F, p); // 현제 프레임에서 페이지 p 탐색

	// HIT : last 사용 시점  갱신
        if (idx != -1) {   
            last[idx] = t;
            continue;
        }

	// MISS
        faults++;

	// 빈 프레임 채우기
        if (filled < F) {
            frames[filled] = p;
            last[filled] = t;
	    filled++;
            
        } else 
	// 프레임이 모두 찬 경우
	{
            // 프레임이 모두 찬 상태일 경우, LRU victim 설정
            int victim = 0;
            for (int i = 1; i < F; i++)
                if (last[i] < last[victim]) victim = i; // 계속해서 갱신해서, 가장 오래전에 사용된 프레임 설정

	    // 설정
            frames[victim] = p;
            last[victim]   = t;
        }
    }

    free(frames);
    free(last);
    return faults;
}

/*
 Clock Algorithm
	- frames[i] : ith page frame number
	- rebf[i] : reference bit(0 or 1)
		- 페이지가 참조될 때, refb[idx] = 1
		- 교체 시 hand가 가리키는 프레임의 refb가 1이면, 0으로 만들고, 한 번더 기회 주고 다음칸으로 이동
		- refb == 0 인 프레임을 처음 만나면 그 자리를 교체
	- hand : 원형 배열에서 현재 가리키고 있는 인덱스	
 */
static int simulate_clock(int F, const RefSeq *seq) {

    // Initialzie
    int *frames = (int*)malloc(sizeof(int)*F);
    char *refb  = (char*)malloc(sizeof(char)*F);
    for (int i = 0; i < F; i++) {
	    frames[i] = -1; 
	    refb[i] = 0; 
    }

    int faults = 0; // 페이지 폴트수
    int  hand = 0; // 시계 바늘 역할
    int  filled = 0; // 채워진 프레임수

    for (int t = 0; t < seq->n; t++) {

        int p = seq->refs[t]; // 현재 참조중인 페이지

	// HIT 여부부터 확인
        int idx = find_in_frames(frames, F, p); 
	// HIT인 경우, reference bit를 1로설정
        if (idx != -1) {
	       	refb[idx] = 1;
	       	continue;
       	} 

	// MISS
        faults++;

	// 1) 아직 비어있는 프레임이 있으면, 채우기
        if (filled < F) {
            frames[filled] = p;
            refb[filled] = 1;
	    filled++;
            // hand는 빈 칸을 채울 때는 굳이 움직이지 않아도 된다.
        }
	// 2) 프레임이 다 찬 경우 : Clcok Algorithm  진행
       	else 
	{
            // hand가 가리키는 프레임부터 reference bit ==  0 을 찾을 때까지 회전
            while(1) {
		// reference bit 가 0 인 경우, 해당 프레임 바로 교체
                if (refb[hand] == 0) {
                    frames[hand] = p;
                    refb[hand] = 1; // 새로 들어온 페이지의 경우는 참조되었으므로 1로 설정
                    hand = (hand + 1) % F; // hand 한 칸 이동
                    break;
                } else {
		   // reference bit 가 1인경우에는, 한번더 기회를 주고 0으로 설정한  뒤 다음칸으로 이동
                    refb[hand] = 0;
                    hand = (hand + 1) % F;
                }
            }
        }
    }


    free(frames);
    free(refb);
    return faults;
}

// 결과 출력 
static void print_result(const char *title, int faults, int total_refs) 
{
    // Page Falut rate 계산
    double rate = 0;
    if(total_refs ==0) rate = 0;
    else {
	    rate = (faults * 100.0) / (double)total_refs;
    }
    printf("%s\n", title); // 알고리즘 이름 출력
    printf("Number of Page Faults: %d\n", faults); // 페이지 폴트 수 출력
    printf("Page Fault Rate: %.2f%%\n\n", rate); // 페이티 폴트 비율 출력
}


// main()
int main(int argc, char **argv) 
{
    if (argc != 2) {
        fprintf(stderr, "Wrong input\n");
        return 1;
    }

    int frames; // frames
    RefSeq sequence = {0}; // reference string 구조체 초기화
	
    read_input(argv[1],&frames,&sequence);

    // 4가지 알고리즘 각각 시뮬레이션 돌리기
    int opt_faults   = simulate_opt(frames, &sequence);
    int fifo_faults  = simulate_fifo(frames, &sequence);
    int lru_faults   = simulate_lru(frames, &sequence);
    int clock_faults = simulate_clock(frames, &sequence);

    // 결과 출력
    print_result("Optimal Algorithm:", opt_faults, sequence.n);
    print_result("FIFO Algorithm:",    fifo_faults, sequence.n);
    print_result("LRU Algorithm:",     lru_faults, sequence.n);
    print_result("Clock Algorithm:",   clock_faults, sequence.n);

    // 동적할당 해제
    free(sequence.refs);
    return 0;
}

