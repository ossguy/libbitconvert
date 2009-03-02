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

#define TRACK_INPUT_SIZE 4096


char* get_track(FILE* input, char* bits, int bits_len)
{
	int bits_end;

	if (NULL == fgets(bits, bits_len, input)) {
		return NULL;
	}
	bits_end = strlen(bits);

	/* strip trailing newline */
	if ('\n' == bits[bits_end - 1]) {
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
	char ft1[TRACK_INPUT_SIZE];
	char ft2[TRACK_INPUT_SIZE];
	char ft3[TRACK_INPUT_SIZE];
	char bt1[TRACK_INPUT_SIZE];
	char bt2[TRACK_INPUT_SIZE];
	char bt3[TRACK_INPUT_SIZE];
	struct bc_input forward;
	struct bc_input backward;
	struct bc_input combined;
	int rv;

	input = stdin;
	bc_init(print_error);

	while (1) {
		if (NULL == get_track(input, ft1, sizeof(ft1))) {
			break;
		}
		if (NULL == get_track(input, ft2, sizeof(ft2))) {
			break;
		}
		if (NULL == get_track(input, ft3, sizeof(ft3))) {
			break;
		}

		if (NULL == get_track(input, bt1, sizeof(bt1))) {
			break;
		}
		if (NULL == get_track(input, bt2, sizeof(bt2))) {
			break;
		}
		if (NULL == get_track(input, bt3, sizeof(bt3))) {
			break;
		}

		forward.t1 = ft1;
		forward.t2 = ft2;
		forward.t3 = ft3;
		backward.t1 = bt1;
		backward.t2 = bt2;
		backward.t3 = bt3;
		rv = bc_combine(&forward, &backward, &combined);

		printf("Result: %d (%s)\n", rv, bc_strerror(rv));
		printf("Track 1 - data_len: %lu, data:\n`%s`\n",
			(unsigned long)strlen(combined.t1), combined.t1);
		printf("Track 2 - data_len: %lu, data:\n`%s`\n",
			(unsigned long)strlen(combined.t2), combined.t2);
		printf("Track 3 - data_len: %lu, data:\n`%s`\n",
			(unsigned long)strlen(combined.t3), combined.t3);

		bc_input_free(&combined);
	}

	fclose(input);

	return 0;
}
