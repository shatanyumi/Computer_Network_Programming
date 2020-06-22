echo_select:csapp.h csapp.c tcp_echo_cli.c tcp_echo_srv.c
	gcc -O2 csapp.h csapp.c tcp_echo_cli.c -o cli -lpthread
	gcc -O2 csapp.h csapp.c tcp_echo_srv.c -o srv -lpthread

.PONY:
	clean
clean:
	rm srv cli
