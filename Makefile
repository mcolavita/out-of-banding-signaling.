CC = gcc
CFLAGS  = -g -Wall -w



.PHONY: all clean cleanall test

TARGET = supervisor \
	 client

all: $(TARGET)

supervisor: supervisor.c
	
	$(CC) $(CFLAGS) -o supervisor supervisor.c -lpthread

client: client.c
	
	$(CC) $(CFLAGS) -o client client.c

clean:
	rm -rf $(TARGET)

cleanall: clean

	\rm -f OOB* *.o *.log supervisor_*


test:
	bash ./test.sh
