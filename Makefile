# NAME Nevin Liang
# EMAIL: nliang868@g.ucla.edu
# ID: 705575353

lab4c: tcp tls

tcp: lab4c_tcp.c
	@if [ `uname -a | grep -c "armv7l"` -ne 0 ]; then\
		gcc -lmraa -Wall -Wextra -lm -o lab4c_tcp lab4c_tcp.c;\
	else\
		gcc -DDUMMY -Wall -Wextra -lm -o lab4c_tcp lab4c_tcp.c;\
	fi

tls: lab4c_tls.c
	@if [ `uname -a | grep -c "armv7l"` -ne 0 ]; then\
		gcc -lmraa -Wall -Wextra -lm -lcrypto -lssl -o lab4c_tls lab4c_tls.c;\
	else\
		gcc -DDUMMY -Wall -Wextra -lm -lcrypto -lssl -o lab4c_tls lab4c_tls.c;\
	fi

clean:
	rm -f lab4c_tcp lab4c_tls
	rm -f lab4c-705575353.tar.gz

dist: lab4c_tcp.c lab4c_tls.c README Makefile
	tar -czf lab4c-705575353.tar.gz lab4c_tls.c lab4c_tcp.c README Makefile
