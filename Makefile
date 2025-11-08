
CCFLAGS= -g -Wall -Werror -O2
LDFLAGS=
CC="gcc"

all : json_min.o
%.c : %.o

commit : 
	