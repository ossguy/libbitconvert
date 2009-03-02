# Makefile
# This file is part of libbitconvert.
#
# Copyright (c) 2008-2009, Denver Gingerich <denver@ossguy.com>
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

CC = gcc
CFLAGS = -ansi -pedantic -Wall -Wextra -Werror

all: driver combine

driver: driver.o libbitconvert.a
	$(CC) $(CFLAGS) -o driver driver.o -L. -lbitconvert -lpcre

driver.o: driver.c bitconvert.h
	$(CC) $(CFLAGS) -c driver.c

combine: combine.o libbitconvert.a
	$(CC) $(CFLAGS) -o combine combine.o -L. -lbitconvert -lpcre

combine.o: combine.c bitconvert.h
	$(CC) $(CFLAGS) -c combine.c

bitconvert.o: bitconvert.c bitconvert.h
	$(CC) $(CFLAGS) -c bitconvert.c

libbitconvert.a: bitconvert.o
	ar rcs libbitconvert.a bitconvert.o

clean:
	rm -f *.a *.o driver
