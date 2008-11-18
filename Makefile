CC = gcc
CFLAGS = -ansi -pedantic -Wall -Werror

all: driver

driver: driver.o libbitconvert.a
	$(CC) $(CFLAGS) -o driver driver.o -L. -lbitconvert -lpcre

driver.o: driver.c bitconvert.h
	$(CC) $(CFLAGS) -c driver.c

bitconvert.o: bitconvert.c bitconvert.h
	$(CC) $(CFLAGS) -c bitconvert.c

libbitconvert.a: bitconvert.o
	ar rcs libbitconvert.a bitconvert.o

clean:
	rm -f *.a *.o driver
