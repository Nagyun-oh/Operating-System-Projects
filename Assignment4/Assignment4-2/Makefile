CC = gcc
CFLAGS = -Wall -Wextra -std=c11
TARGET = fat
SRCS = fat.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)


