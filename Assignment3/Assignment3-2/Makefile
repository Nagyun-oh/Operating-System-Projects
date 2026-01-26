CC = gcc
CFLAGS = -Wall -O2 -std=c11

TARGET = page_replacement_simulator
SRC = page_replacement_simulator.c

all: $(TARGET)


$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)

clean:
	rm -f $(TARGET)


