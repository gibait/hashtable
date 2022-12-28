# variabile cc specifica il compilatore da utilizzare
CC=gcc
#parametro utilizzato dal compilatore C
CFLAG=-Wall -Wextra -Werror -pedantic
SRC = $(wildcard *.c)
TAR = $(SRC:.c=)

all: $(TAR)

%: %.c
	$(CC) $(CFLAG)  $< -o $@.o -lpthread -g

clean:
	rm -fr *.o