all:client.c server.c packet.h
	gcc client.c -o client -pthread
	gcc server.c -o server -pthread
client:client.c packet.h
	gcc client.c -o client -pthread
server:server.c packet.h
	gcc server.c -o server -pthread
clean:
	rm server client