CC = gcc
CFLAGS = -Wall -O2 -std=c11
LDLIBS = -lm              # ⬅ math 라이브러리 추가

TARGET = cpu_scheduler
SRC = cpu_scheduler.c

all: $(TARGET)


$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)

clean:
	rm -f $(TARGET)

