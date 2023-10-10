LINK_TARGET = client server

CLIENT_OBJS = client.o common.o
SERVER_OBJS = server.o common.o

REBUILDABLES = $(CLIENT_OBJS) $(SERVER_OBJS) $(LINK_TARGET)

all: $(LINK_TARGET)

clean:
	rm -f $(REBUILDABLES)

client: $(CLIENT_OBJS)
	cc -g3 -lpthread -o $@ $^

server: $(SERVER_OBJS)
	cc -g3 -lpthread -o $@ $^

%.o: %.c
	cc -g3 -Wall -o $@ -c $<