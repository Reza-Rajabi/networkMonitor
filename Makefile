CC=g++
CFLAGS=-I
CFLAGS+=-Wall
FILE1=networkMonitor.cpp
FILE2=interfaceMonitor.cpp

netMonitor: $(FILE1)
	$(CC) $(CFLAGS) $^ -o $@

intfMonitor: $(FILE2)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f *.o netMonitor intfMonitor

all: netMonitor
