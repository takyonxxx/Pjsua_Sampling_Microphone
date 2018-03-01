CC = gcc
CFLAGS = -Wall -g -O0 `pkg-config --cflags libpjproject`
CPPFLAGS =
LD_FLAGS = 
LD_LIBS = `pkg-config --libs libpjproject`
SRCS = main_read.c
OBJS = $(SRCS:.c=.o)
TARGET = run


all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $^ $(LD_FLAGS) $(LD_LIBS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< 

.PHONY: clean

clean:
	$(RM) $(TARGET) $(OBJS)
