default: server client

server.o: server.c
	cc -c server.c -o server.o
server: server.o
	cc server.o -o server
client.o: client.c
	cc -c client.c -o client.o
client: client.o
	cc client.o -o client

clean:
	rm -rf server client *.o core
