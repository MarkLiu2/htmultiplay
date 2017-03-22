
TOPDIR=$(HOME)/bin
CLDFLAGS += -lpthread -lasound

LDFLAGS += -lavutil
LDFLAGS += -lavcodec
LDFLAGS += -lavformat
LDFLAGS += -lswresample
LDFLAGS += -lpthread

LDFLAGS += -L$(TOPDIR)/lib/
INCLUDE_H+=-I$(TOPDIR)/include/


SOBJ=server.o cmd.o stream.o queue.o
COBJ=client.o alsa.o mixer.o queue.o

all:server client 

server: $(SOBJ)
	$(CC)	$(SOBJ) $(CFLAGS) -o server $(LDFLAGS)

client: $(COBJ)
	$(CC)	$(COBJ) $(CFLAGS) -o client $(CLDFLAGS)

%.o:%.c
	$(CC) -c $(CFLAGS) $(INCLUDE_H) $(CPPFLAGS) $< -o $@ 

htmsg: htmsg.o
	$(CC)	htmsg.o  $(CFLAGS) -o htmsg $(LDFLAGS)

clean:
	rm -f *.o *.so htmsg server client 
