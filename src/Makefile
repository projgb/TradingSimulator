all: orderclient.exe ordertracker.exe exch.exe

CC = g++
CFLAGS = -std=c++11
LIBS = -lpthread

orderclient.exe:
	$(CC) $(CFLAGS) -o orderclient.exe OrderClientMain.cpp utils.cpp CommLink.cpp $(LIBS)

ordertracker.exe:
	$(CC) $(CFLAGS) -o ordertracker.exe OrderTracker.cpp OrdTrackerMain.cpp utils.cpp CommLink.cpp $(LIBS)

exch.exe:
	$(CC) $(CFLAGS) -o exch.exe ExchMain.cpp CommLink.cpp utils.cpp  $(LIBS)

clean:
	rm -f orderclient.exe ordertracker.exe exch.exe
