echo:tcp_echo_srv.c tcp_echo_cli.c
	gcc -O2 tcp_echo_srv.c -o srv
	gcc -O2 tcp_echo_cli.c -o cli
.PHONY:clean

clean:
	rm cli srv
