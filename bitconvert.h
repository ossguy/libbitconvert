/*
 * bitconvert.h - bit decoding declarations
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

/*********************************************************************/
/***** THIS API IS NOT YET STABLE; FREQUENT CHANGES WILL BE MADE *****/
/*********************************************************************/

#ifndef H_BITCONVERT
#define H_BITCONVERT

#include <stddef.h> /* size_t */

#ifdef __cplusplus
extern "C" {
#endif

/* when adding to this list, also add a case to the switch in bc_strerror */
#define BCERR_INVALID_INPUT		1
#define BCERR_PARITY_MISMATCH		2
/* 3 was used for BCERR_RESULT_FULL; we're using dynamic allocation now */
/* 4 was used for BCERR_INVALID_TRACK; now we accept all tracks at once */
#define BCERR_NO_FORMAT_FILE		5
#define BCERR_PCRE_COMPILE_FAILED	6
#define BCERR_FORMAT_MISSING_PERIOD	7
#define BCERR_FORMAT_MISSING_NAME	8
#define BCERR_NO_MATCHING_FORMAT	9
#define BCERR_BAD_FORMAT_ENCODING_TYPE	10
#define BCERR_FORMAT_MISSING_RE		11
#define BCERR_UNIMPLEMENTED		12
#define BCERR_OUT_OF_MEMORY		13
#define BCERR_FORMAT_MISSING_TRACK	14
#define BCERR_FORMAT_MISSING_SPACE	15
#define BCERR_FORMAT_NAMED_SUBSTRING	16

#define BC_ENCODING_NONE  -1	/* track has no data; not the same as binary */
#define BC_ENCODING_BINARY 1
#define BC_ENCODING_BCD    4
#define BC_ENCODING_ALPHA  6
#define BC_ENCODING_ASCII  7

#define BC_TRACK_1	1
#define BC_TRACK_2	2
#define BC_TRACK_3	3

struct bc_input {
	char* t1;
	char* t2;
	char* t3;
};

struct bc_decoded {
	char* t1;
	char* t2;
	char* t3;

	/* one of BC_ENCODING_* */
	int t1_encoding;
	int t2_encoding;
	int t3_encoding;

	/* name of the card; based on the match in the formats file */
	char* name;

	/* NULL-terminated array of field names */
	char** field_names;

	/* empty strings may be valid values; use field_names to find end */
	const char** field_values;

	/* one of BC_TRACK_* to represent the track the field is stored on */
	int* field_tracks;
};


/* user may provide a null error_callback to ignore error messages */
void bc_init(void (*error_callback)(const char*));

int bc_decode(struct bc_input* in, struct bc_decoded* result);
int bc_find_fields(struct bc_decoded* result);
int bc_combine(struct bc_input* forward, struct bc_input* backward,
	struct bc_input* combined);
const char* bc_strerror(int err);

void bc_input_free(struct bc_input* in);
void bc_decoded_free(struct bc_decoded* result);

#ifdef __cplusplus
}
#endif

#endif /* H_BITCONVERT */
