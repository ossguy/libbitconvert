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
#define BCERR_RESULT_FULL		3
#define BCERR_INVALID_TRACK		4
#define BCERR_NO_FORMAT_FILE		5
#define BCERR_PCRE_COMPILE_FAILED	6
#define BCERR_FORMAT_MISSING_PERIOD	7
#define BCERR_FORMAT_MISSING_NAME	8
#define BCERR_NO_MATCHING_FORMAT	9
#define BCERR_BAD_FORMAT_ENCODING_TYPE	10
#define BCERR_FORMAT_MISSING_RE		11

#define BC_ENCODING_NONE  -1	/* track has no data; not the same as binary */
#define BC_ENCODING_BINARY 1
#define BC_ENCODING_BCD    4
#define BC_ENCODING_ALPHA  6
#define BC_ENCODING_ASCII  7

#define BC_TRACK_1	1
#define BC_TRACK_2	2
#define BC_TRACK_3	3

#define BC_INPUT_SIZE	4096
#define BC_T1_INPUT_SIZE BC_INPUT_SIZE
#define BC_T2_INPUT_SIZE BC_INPUT_SIZE
#define BC_T3_INPUT_SIZE BC_INPUT_SIZE

#define BC_DECODED_SIZE	1024
#define BC_T1_DECODED_SIZE BC_DECODED_SIZE
#define BC_T2_DECODED_SIZE BC_DECODED_SIZE
#define BC_T3_DECODED_SIZE BC_DECODED_SIZE

#define BC_NUM_FIELDS	32
#define BC_FIELD_SIZE	80

struct bc_input {
	char t1[BC_T1_INPUT_SIZE];
	char t2[BC_T2_INPUT_SIZE];
	char t3[BC_T3_INPUT_SIZE];
};

struct bc_decoded {
	char t1[BC_T1_DECODED_SIZE];
	char t2[BC_T2_DECODED_SIZE];
	char t3[BC_T3_DECODED_SIZE];

	/* one of BC_ENCODING_* */
	int t1_encoding;
	int t2_encoding;
	int t3_encoding;

	/* name of the card; based on the match in the formats file */
	char name[BC_FIELD_SIZE];

	/* empty string-terminated array of field names */
	char field_names[BC_NUM_FIELDS][BC_FIELD_SIZE];

	/* empty strings may be valid values; use field_names to find end */
	char field_values[BC_NUM_FIELDS][BC_FIELD_SIZE];

	/* one of BC_TRACK_* to represent the track the field is stored on */
	int field_tracks[BC_NUM_FIELDS];
};


/* user may provide a NULL error_callback to ignore error messages */
void bc_init(struct bc_input* in, void (*error_callback)(const char*));

int bc_decode(struct bc_input* in, struct bc_decoded* result);
int bc_find_fields(struct bc_decoded* result);
const char* bc_strerror(int err);

#ifdef __cplusplus
}
#endif

#endif /* H_BITCONVERT */
