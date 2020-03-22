CC=g++
CFLAGS=-I
CFLAGS+=-Wall
FILE1=networkMonitor.cpp
FILE2=interfaceMonitor.cpp

networkMonitor: $(FILE1)
	$(CC) $(CFLAGS) $^ -o $@

intfMonitor: $(FILE2)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f *.o networkMonitor intfMonitor

all: networkMonitor intfMonitor
