
CCFLAGS= -g -Wall -Werror -O2
LDFLAGS=
CC="gcc"

%.c : %.o
all : json_min.o
clean:
	rm -f *.o
commit : 
	@if [ "x$M" = 'x' ]; then \
		echo "Usage: M='your message here.' make commit"; fi
	@test "x$M" != 'x'
	git commit -a -m "$M"