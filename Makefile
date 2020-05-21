CC      = g++
CFLAGS  = -std=c++11

INC     = -I includes/
LDFLAGS = -pthread $(INC) 

all: reliable_sender reliable_receiver

.PHONY: reliable_sender
reliable_sender:
	$(CC) -o reliable_sender src/reliable_sender.cpp $(LDFLAGS) $(CFLAGS) 

.PHONY: reliable_receiver
reliable_receiver:
	$(CC) -o reliable_receiver src/reliable_receiver.cpp $(LDFLAGS) $(CFLAGS) 

.PHONY: clean
clean:
	@rm -f reliable_sender reliable_receiver *.o
