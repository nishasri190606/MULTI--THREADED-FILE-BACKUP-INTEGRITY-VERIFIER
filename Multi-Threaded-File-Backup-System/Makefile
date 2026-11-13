CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
TARGET = backup_tool

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) main.c -o $(TARGET)

clean:
	rm -f $(TARGET) backup.log

.PHONY: all clean
