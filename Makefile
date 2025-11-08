
CCFLAGS= -g -Wall -Werror -O2
LDFLAGS=
CC="gcc"

all : json_min.o
%.c : %.o

commit : 
	@if [ "x$M" = 'x' ]; then \
		echo "Usage: M='your message here.' make commit"; fi
	@test "x$M" != 'x'
	git commit -a -m "$M"