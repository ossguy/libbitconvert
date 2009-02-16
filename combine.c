/*
 * combine.c - test driver for bc_combine in libbitconvert
 * This file is part of libbitconvert.
 *
 * Copyright (c) 2008-2009, Denver Gingerich <denver@ossguy.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "bitconvert.h"
#include <stdio.h>  /* FILE, fgets, printf */
#include <string.h> /* strlen */


char* get_track(FILE* input, char* bits, int bits_len)
{
	int bits_end;

	if (NULL == fgets(bits, bits_len, input))
	{
		return NULL;
	}
	bits_end = strlen(bits);

	/* strip trailing newline */
	if ('\n' == bits[bits_end - 1])
	{
		bits[bits_end - 1] = '\0';
	}

	return bits;
}

void print_error(const char* error)
{
	printf("%s\n", error);
}

int main(void)
{
	FILE* input;
	struct bc_input forward;
	struct bc_input backward;
	struct bc_input combined;
	int rv;

	input = stdin;
	bc_init(&forward, print_error);
	bc_init(&backward, print_error);
	bc_init(&combined, print_error);

	while (1)
	{
		if (NULL == get_track(input, forward.t1, sizeof(forward.t1)))
			break;
		if (NULL == get_track(input, forward.t2, sizeof(forward.t2)))
			break;
		if (NULL == get_track(input, forward.t3, sizeof(forward.t3)))
			break;

		if (NULL == get_track(input, backward.t1, sizeof(backward.t1)))
			break;
		if (NULL == get_track(input, backward.t2, sizeof(backward.t2)))
			break;
		if (NULL == get_track(input, backward.t3, sizeof(backward.t3)))
			break;

		rv = bc_combine(&forward, &backward, &combined);

		printf("Result: %d (%s)\n", rv, bc_strerror(rv));
		printf("Track 1 - data_len: %lu, data:\n`%s`\n",
			(unsigned long)strlen(combined.t1), combined.t1);
		printf("Track 2 - data_len: %lu, data:\n`%s`\n",
			(unsigned long)strlen(combined.t2), combined.t2);
		printf("Track 3 - data_len: %lu, data:\n`%s`\n",
			(unsigned long)strlen(combined.t3), combined.t3);
	}

	fclose(input);

	return 0;
}
