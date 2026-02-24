🐧 Operating System (2025-2)

- 리눅스 커널 수준의 시스템 이해와 C를 활용한 운영체제 프로젝트

---

📂 과제 및 레포트 요약 (Assignments)각 과제 제목을 클릭하면 상세 구현 방법과 결과 분석이 담긴 개별 레포트로 이동합니다. 

| 단계 | 주요 주제 | 핵심 구현 내용 | 레포트 |
| :--- | :--- | :--- | :---: |
| **AS1** | **System Call & Hooking** | `os_ftrace` 시스템 콜 등록, 커널 모듈 적재, syscall hijacking (openat, read 등) | [📄 Report](./Assignment1/README.MD) |
| **AS2** | **Process & Scheduling** | `Process_tracer` 커널 모듈, 다중 프로세스/스레드 성능 비교, CPU 스케줄링(FCFS, SJF, SRTF, RR) 시뮬레이터 | [📄 Report](./Assignment2/Assignment2.pdf) |
| **AS3** | **Memory Management** | 가상 메모리 주소(VMA) 추적 시스템 콜, 페이지 교체 알고리즘(OPT, FIFO, LRU, Clock) 시뮬레이터 | [📄 Report](./Assignment3/Assignment3.pdf) |
| **AS4** | **File System** | `/proc` 파일 시스템 기반 커널 정보 출력, FAT 구조의 User-level 파일 시스템 구현 및 데이터 지속성 관리 | [📄 Report](./Assignment4/Assignment4.pdf) |


---

🛠️ 주요 기술 스택 
- Language: C 
- Environment: Linux Kernel (Ubuntu), GDB, Makefile
- Knowledge: Kernel Module (insmod/rmmod), System Call Table, Process/Thread, Virtual Memory, FAT File System

---

🚀 과제별 핵심 요약

[Assignment 1] 커널 시스템 콜 및 후킹
- 커널 소스 수정을 통한 신규 시스템 콜(os_ftrace) 등록 및 커널 재컴파일.
- 커널 모듈을 통한 시스템 콜 테이블 하이재킹 기술 습득.

[Assignment 2] 프로세스 추적 및 스케줄링
-  task_struct 탐색을 통한 프로세스 상태, Context Switch, 가족 관계 정보 커널 로그 출력.
- fork()와 pthread 기반의 연산 성능 비교 및 4가지 CPU 스케줄링 알고리즘 성능 수치 분석.

[Assignment 3] 메모리 영역 추적 및 페이지 교체
- 프로세스 PID를 입력받아 코드/데이터/힙 영역 및 매핑된 파일 경로를 출력하는 커널 모듈 구현.
- 페이지 결함률(Page Fault Rate)을 기반으로 한 4가지 페이지 교체 알고리즘 비교 시뮬레이션.

[Assignment 4] Proc 파일 시스템 및 FAT 구현
- 커널 내부 정보를 User-space에서 확인할 수 있는 가상 파일 시스템(proc) 인터페이스 구축.
- FAT 테이블 기반의 블록 연결 구조를 갖춘 사용자 수준 파일 시스템 설계 및 디스크 저장 구현.