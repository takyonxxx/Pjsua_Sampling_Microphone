
CC = gcc
CFLAGS = -Wall -g -O0 `pkg-config --cflags libpjproject`
CPPFLAGS =
LD_FLAGS = 
LD_LIBS = `pkg-config --libs libpjproject`
SRCS = main.c
OBJS = $(SRCS:.c=.o)
TARGET = crypto_read


all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $^ $(LD_FLAGS) $(LD_LIBS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< 

.PHONY: clean

clean:
	$(RM) $(TARGET) $(OBJS)
