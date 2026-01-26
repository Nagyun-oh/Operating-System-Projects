obj-m += file_varea.o # 커널 모듈로 컴파일할 오브젝트 파일

KDIR := /lib/modules/$(shell uname -r)/build # 현재 시스템의 커널 빌드 시스템 위치
PWD := $(shell pwd) # 현재 모듈을 컴파일하는 디렉토리 위치 

# 빌드
all:
	$(MAKE) -C$(KDIR) M=$(PWD) modules
# 빌드 생성물 제거
clean:
	$(MAKE) -C$(KDIR) M=$(PWD) clean

