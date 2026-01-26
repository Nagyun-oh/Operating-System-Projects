CC = gcc
CFLAGS = -O2 -Wall -Wextra
THREAD_FLAGS = -pthread

# 기본 프로세스 개수 = 8
MAX ?=8

# 실행 파일 이름
TARGETS = numgen fork thread

# 8개버전 
all: $(TARGETS)

numgen: numgen.c
	$(CC) $(CFLAGS) -DMAX_PROCESSES=$(MAX) -o $@ $<

fork: fork.c
	$(CC) $(CFLAGS) -DMAX_PROCESSES=$(MAX) -o $@ $<

thread: thread.c
	$(CC) $(CFLAGS) $(THREAD_FLAGS) -DMAX_PROCESSES=$(MAX) -o $@ $<


# 64개 버전
all64:
	$(MAKE) clean
	$(MAKE) all MAX=64

# 실행 도우미
run:
	./numgen
	./fork
	./thread

# 캐시 및 파일 정리
clean:
	rm -f $(TARGETS) temp.txt
