/*
 * driver.c - test driver for libbitconvert
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

char* encoding_to_str(int track)
{
	switch (track) {
	case BC_ENCODING_NONE:		return "none";
	case BC_ENCODING_BINARY:	return "binary";
	case BC_ENCODING_BCD:		return "BCD";
	case BC_ENCODING_ALPHA:		return "ALPHA";
	case BC_ENCODING_ASCII:		return "ASCII";
	default:			return "unknown";
	}
}

void print_error(const char* error)
{
	printf("%s\n", error);
}

int main(void)
{
	FILE* input;
	struct bc_input in;
	struct bc_decoded result;
	int rv;
	int i;

	input = stdin;
	bc_init(&in, print_error);

	while (1)
	{
		if (NULL == get_track(input, in.t1, sizeof(in.t1)))
			break;
		if (NULL == get_track(input, in.t2, sizeof(in.t2)))
			break;
		if (NULL == get_track(input, in.t3, sizeof(in.t3)))
			break;

		rv = bc_decode(&in, &result);

		printf("Result: %d (%s)\n", rv, bc_strerror(rv));
		printf("Track 1 - data_len: %lu, encoding: %s, data:\n`%s`\n",
			(unsigned long)strlen(result.t1),
			encoding_to_str(result.t1_encoding), result.t1);
		printf("Track 2 - data_len: %lu, encoding: %s, data:\n`%s`\n",
			(unsigned long)strlen(result.t2),
			encoding_to_str(result.t2_encoding), result.t2);
		printf("Track 3 - data_len: %lu, encoding: %s, data:\n`%s`\n",
			(unsigned long)strlen(result.t3),
			encoding_to_str(result.t3_encoding), result.t3);

		if (0 != rv)
			/* if there was an error, bc_find_fields isn't useful */
			continue;

		rv = bc_find_fields(&result);
		if (0 != rv)
		{
			printf("Error %d (%s); no fields found for this card\n",
				rv, bc_strerror(rv));
			continue;
		}

		printf("\n=== Fields ===\n");
		printf("Card name: %s\n", result.name);
		for (i = 0; result.field_names[i][0] != '\0'; i++)
		{
			/* NOTE: you should verify that the BC_TRACK_* constants
			 * in the version of libbitconvert that you are using
			 * map cleanly onto integers if you wish to print the
			 * tracks using the method below; the library may change
			 * to allow tracks such as BC_TRACK_JIS_II, which would
			 * not print correctly using this method
			 */
			printf("Track %d - %s: %s\n", result.field_tracks[i],
				result.field_names[i], result.field_values[i]);
		}
	}

	fclose(input);

	return 0;
}
