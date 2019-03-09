## Swarm version 3.2  -- June 2017
## http://www.spinroot.com/swarm
##
## leveraging parallelism with search diversity
## within user-defined time-bounds and memory-limits

linux: swarm.c
	cc -std=c99 -Wall -pedantic -o swarm swarm.c -lm

cygwin: swarm.c
	cc -Wall -pedantic -o swarm swarm.c -lm

uno: swarm.c
	uno -w swarm.c	# use uno version 2.11 or later
	@# http://www.spinroot.com/uno

install_linux: linux
	# linux:
	sudo cp swarm /usr/local/bin
	sudo cp swarm.1 /usr/local/share/man/man1

install_cygwin: cygwin
	cp swarm.exe /bin
	cp swarm.1 /usr/man/man1

tar:
	# cp ../index.html swarm.html
	tar -cf - swarm.c swarm_seeds.h makefile swarm.1 swarm.html > swarm3.2.tar
	rm -f swarm.html

clean:
	rm -f swarm swarm.exe swarm.o swarm.obj model.swarm
