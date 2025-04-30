CC = gcc
CFLAGS = -Wall -Wextra -O2

TARGET_1 = task1
TARGET_2 = task2

SRC_1 = task1.c
SRC_2 = task2.c

.PHONY: all clean setup run

all: $(TARGET_1) $(TARGET_2)

$(TARGET_1): $(SRC_1)
 $(CC) $(CFLAGS) -o $@ $<

$(TARGET_2): $(SRC_2)
 $(CC) $(CFLAGS) -o $@ $<

clean:
 rm -f $(TARGET_1) $(TARGET_2)
