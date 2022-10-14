CC = gcc 
LD = gcc

SRCS = $(wildcard *.cpp)
OBJS = $(patsubst %c, %o, $(SRCS))

TARGET = rtpserver_h265

.PHONY:all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) -o $@ $^

%o:%c
	$(CC) -cpp $^

clean:
	rm -f $(OBJS) $(TARGET)
