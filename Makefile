
CFLAGS= -g -Wall -Werror
LDFLAGS=
CC=gcc
SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)
BIN=app

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@
	
all : $(BIN)

$(BIN) : $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $@

clean:
	rm -f $(OBJ) $(BIN)
commit : 
	@if [ "x$M" = 'x' ]; then \
		echo "Usage: M='your message here.' make commit"; fi
	@test "x$M" != 'x'
	git commit -a -m "$M"