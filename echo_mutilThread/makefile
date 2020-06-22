echo:tcp_echo_srv.c tcp_echo_cli.c
	gcc -O2 tcp_echo_srv.c -o srv -lpthread
	gcc -O2 tcp_echo_cli.c -o cli -lpthread
.PHONY:clean

clean:
	rm cli srv
