echo_epoll: tcp_echo_cli.c tcp_echo_srv.c csapp.h csapp.c
	gcc -O2 tcp_echo_cli.c csapp.c csapp.h -o cli -lpthread 
	gcc -O2 tcp_echo_srv.c -o srv

.PONY:
	clean

clean:
	rm cli srv
