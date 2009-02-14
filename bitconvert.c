/*
 * bitconvert.c - bit decoding implementation
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

/* Assumptions:
 * - character set is ALPHA or BCD
 * - each character is succeeded by an odd parity bit
 * - libbitconvert is run on a system which uses the ASCII character set (this
 *   is required for the to_ascii function to work correctly)
 */

#include "bitconvert.h"
#include <string.h>	/* strspn, strlen */
#include <pcre.h>	/* pcre* */
#include <stdio.h>	/* FILE, fopen, fgets */
#include <ctype.h>	/* isspace */

/* TODO: add appropriate calls to pcre_free (probably just re variables) */

/* maximum length of a line in a format file */
#define FORMAT_LEN	1024

/* maximum number of captured substrings in a track's regular expression;
 * thus, this is also the maximum number of fields per track
 */
#define MAX_CAPTURED_SUBSTRINGS	64


/* return codes internal to the library; these MUST NOT overlap with BCERR_* */
#define BCINT_OFFSET	1024
#define BCINT_NO_MATCH	BCINT_OFFSET + 1


void (*send_error)(const char*);

char to_ascii(char bits, unsigned char value)
{
	if (5 == bits)
	{
		/* the BCD encoding is the subset of ASCII beginning at
		 * character 48 ('0'), ending at character 63; see
		 *	http://www.cyberd.co.uk/support/technotes/isocards.htm
		 *	http://en.wikipedia.org/wiki/ASCII#ASCII_printable_characters
		 */
		return '0' + value;
	}
	else if (7 == bits)
	{
		/* the ALPHA encoding is the subset of ASCII beginning at
		 * character 32 (' '), ending at character 95; see
		 *	http://www.cyberd.co.uk/support/technotes/isocards.htm
		 *	http://en.wikipedia.org/wiki/ASCII#ASCII_printable_characters
		 */
		return ' ' + value;
	}
	return '\0';
}

int bc_decode_format(char* bits, char* result, size_t result_len, unsigned char format_bits)
{
	int start_idx;
	int i;
	int j;
	unsigned char current_value;
	unsigned char parity;
	size_t result_idx;
	int retval = 0;

	int bits_len = strlen(bits);

	/* skip leading zeroes; assume 1st character in stream starts with 1 */
	start_idx = strspn(bits, "0");

	result_idx = 0;
	for (i = start_idx; (i + format_bits) <= bits_len; i += format_bits)
	{
		current_value = 0;
		parity = 1;

		for (j = 0; j < (format_bits - 1); j++)
		{
			if ('1' == bits[i + j])
			{
				/* push a 1 onto the front of our accumulator */
				current_value |= (1 << j);

				parity ^= 1; /* flip parity bit */
			}
			/* assimilating a 0 is a no-op */
			else if ('0' != bits[i + j])
			{
				retval = BCERR_INVALID_INPUT;
				break;
			}
		}

		/* since C doesn't have multi-level breaks */
		if (0 != retval)
		{
			break;
		}

		if ("01"[parity] != bits[i + j])
		{
			retval = BCERR_PARITY_MISMATCH;
			break;
		}

		if (result_idx < result_len)
		{
			result[result_idx] = to_ascii(format_bits, current_value);
			result_idx++;

			if ('?' == result[result_idx - 1])
			{
				/* found end sentinel; we're done */
				break;
			}
		}
		else
		{
			/* set result_idx so result is null-terminated */
			result_idx = result_len - 1;

			retval = BCERR_RESULT_FULL;
			break;
		}
	}

	result[result_idx] = '\0';
	/* no need to increment result_idx; we are done */

	return retval;
}

int bc_decode_track_fields(char* input, int encoding, int track, FILE* formats,
	struct bc_decoded* d)
{
	pcre* re;
	const char* error;
	int erroffset;
	int rc;
	int rv;
	int ovector[3 * MAX_CAPTURED_SUBSTRINGS];
	int i;
	int j;
	int k;
	int num_fields;
	char buf[FORMAT_LEN];
	const char* result;
	char* field;
	char* temp_ptr;

	rv = 0;

	fgets(buf, FORMAT_LEN, formats);
	buf[strlen(buf) - 1] = '\0';

	/* TODO: parse out string prefix */
	temp_ptr = strchr(buf, ':');
	if (NULL == temp_ptr) {
		if (strcmp(buf, "none") == 0) {
			if (BC_ENCODING_NONE == encoding) {
				return 0;
			} else {
				return BCINT_NO_MATCH;
			}
		} else if (strcmp(buf, "unknown") == 0) {
			return 0;
		}
		/* TODO: print name of encoding type to error method */
		return BCERR_BAD_FORMAT_ENCODING_TYPE;
	}

	/* replace ':' with '\0' so buf represents encoding type */
	temp_ptr[0] = '\0';

	/* increment past the '\0', which replaced ':' */
	temp_ptr++;

	/* skip whitespace between ':' and regular expression */
	while (isspace(temp_ptr[0]) && temp_ptr[0] != '\0') {
		temp_ptr++;
	}

	/* if there is no regular expression after the data format specifier */
	if (temp_ptr[0] == '\0') {
		return BCERR_FORMAT_MISSING_RE;
	}

	/* temp_ptr now points at the regular expression */
	re = pcre_compile(temp_ptr, 0, &error, &erroffset, NULL);
	if (NULL == re) {
		/* TODO: find some way to pass back error and erroffset;
		 * these would be useful to the user
		 */
		return BCERR_PCRE_COMPILE_FAILED;
	}

	/* XXX: if we want to be really pedantic, check the return code;
	 * with the current code and the behavior of pcre_fullinfo specified
	 * in the documentation, about the only way we could get a non-zero
	 * return code is by cosmic rays
	 */
	pcre_fullinfo(re, NULL, PCRE_INFO_CAPTURECOUNT, &num_fields);

	/* now that we know how many fields to skip, we can validate the
	 * encoding type and then skip the fields and return if there is an
	 * error in the type
	 */
	if (strcmp(buf, "ALPHA") == 0) {
		if (BC_ENCODING_ALPHA != encoding) {
			rv = BCINT_NO_MATCH;
			goto skip_fields;
		}
	} else if (strcmp(buf, "BCD") == 0) {
		if (BC_ENCODING_BCD != encoding) {
			rv = BCINT_NO_MATCH;
			goto skip_fields;
		}
	} else if (strcmp(buf, "binary") == 0) {
		if (BC_ENCODING_BINARY != encoding) {
			rv = BCINT_NO_MATCH;
			goto skip_fields;
		}
	} else {
		/* TODO: print name of encoding type to error method */
		return BCERR_BAD_FORMAT_ENCODING_TYPE;
	}

	/* TODO: if positive, see if return code matches the number
	 * of strings we expect (see pcre.txt line 2113)
	 */
	/* TODO: on error (negative return code), see if it's a bad
	 * error (ie. invalid input) and return if it is; a list of
	 * errors is available starting at pcre.txt line 2155
	 */
	rc = pcre_exec(re, NULL, input, strlen(input), 0, 0,
		ovector, 3 * MAX_CAPTURED_SUBSTRINGS);
	if (rc < 0) {
		rv = BCINT_NO_MATCH;
		goto skip_fields;
	}

	/* find first entry not filled in by previous tracks */
	/* TODO: this could be eliminated by making j a parameter to
	 * bc_decode_track_fields
	 */
	for (j = 0; d->field_names[j][0] != '\0'; j++);

	/* read until we have read all the fields or we encounter end of file
	 * or an empty line
	 */
	for (k = 0; k < num_fields &&
		fgets(buf, FORMAT_LEN, formats) && buf[0] != '\n';
		k++) {

		/* find the first period */
		for (i = 0; buf[i] != '.' && buf[i] != '\0'; i++);

		if ('\0' == buf[i]) {
			/* TODO: find some way to return buf; would be
			 * useful to the user for debugging
			 */
			return BCERR_FORMAT_MISSING_PERIOD;
		}

		/* replace '.' with '\0' to make new string */
		buf[i] = '\0';
		pcre_get_named_substring(re, input, ovector, rc, buf, &result);

		/* need at least enough room for ". " plus at least one
		 * character for the name of the field plus "\n"
		 */
		if (i >= (FORMAT_LEN - 4) ||
			buf[i + 1] == '\0' || buf[i + 2] == '\0' ||
			buf[i + 3] == '\0') {
			/* TODO: find some way to return buf; would be
			 * useful to the user for debugging
			 */
			return BCERR_FORMAT_MISSING_NAME;
		}

		field = &(buf[i + 2]);
		field[strlen(field) - 1] = '\0'; /* remove '\n' */

		strcpy(d->field_names[j], field);
		strcpy(d->field_values[j], result);
		d->field_tracks[j] = track;
		j++;
	}

	d->field_names[j][0] = '\0';
	goto done;

skip_fields:
	/* if there was no match, skip the field descriptions */
	for (k = 0; k < num_fields &&
		fgets(buf, FORMAT_LEN, formats) && buf[0] != '\n';
		k++);

done:
	return rv;
}

/* TODO: fix bc_decode_fields so it checks if inputted lines are longer than or
 * as long as FORMAT_LEN
 */

/* TODO: fix bc_decode_fields so it has variable-length field list; requires
 * fixing the constants in the ovector and fields initialization
 */

/* TODO: fix bc_decode_fields so error handling from multiple
 * bc_decode_track_fields calls is cleaner
 */

int bc_decode_fields(struct bc_decoded* d)
{
	int err;
	int rc;
	int rv;
	char name[FORMAT_LEN];
	FILE* formats;

	formats = fopen("formats", "r");

	if (NULL == formats) {
		return BCERR_NO_FORMAT_FILE;
	}

	rv = BCERR_NO_MATCHING_FORMAT;

	/* initialize the name and fields list */
	d->name[0] = '\0';
	d->field_names[0][0] = '\0';

	while (fgets(name, FORMAT_LEN, formats)) {
		/* TODO: check lengths before strcpy (in general, but here esp.)
		 */
		name[strlen(name) - 1] = '\0'; /* remove '\n' */
		strcpy(d->name, name);


		err = bc_decode_track_fields(d->t1, d->t1_encoding, BC_TRACK_1,
			formats, d);
		if (0 == err || BCINT_NO_MATCH == err) {
			rc = err;
		} else {
			/* not 0 (match) or BCINT_NO_MATCH; thus a formats file
			 * parsing error so we cannot continue
			 */
			rv = err;
			break;
		}

		err = bc_decode_track_fields(d->t2, d->t2_encoding, BC_TRACK_2,
			formats, d);
		/* if previous tracks were ok but this one returned an error,
		 * update the overall return code accordingly
		 */
		if (0 == err || BCINT_NO_MATCH == err) {
			if (0 == rc) {
				rc = err;
			}
		} else {
			/* not 0 (match) or BCINT_NO_MATCH; thus a formats file
			 * parsing error so we cannot continue
			 */
			rv = err;
			break;
		}

		err = bc_decode_track_fields(d->t3, d->t3_encoding, BC_TRACK_3,
			formats, d);
		/* if previous tracks were ok but this one returned an error,
		 * update the overall return code accordingly
		 */
		if (0 == err || BCINT_NO_MATCH == err) {
			if (0 == rc) {
				rc = err;
			}
		} else {
			/* not 0 (match) or BCINT_NO_MATCH; thus a formats file
			 * parsing error so we cannot continue
			 */
			rv = err;
			break;
		}

		/* if there are no errors, we found a match */
		if (0 == rc) {
			rv = 0;
			break;
		}

		/* skip to beginning of next card specification; each card is
		 * separated by an empty line
		 */
		while (fgets(name, FORMAT_LEN, formats) && name[0] != '\n');

		/* empty the name and fields list */
		d->name[0] = '\0';
		d->field_names[0][0] = '\0';
	}

	fclose(formats);

	return rv;
}

void bc_init(struct bc_input* in, void (*error_callback)(const char*))
{
	in->t1[0] = '\0';
	in->t2[0] = '\0';
	in->t3[0] = '\0';
	send_error = error_callback;
}

int bc_decode(struct bc_input* in, struct bc_decoded* result)
{
	int err;
	int rc;

	/* initialize the fields list */
	result->field_names[0][0] = '\0';

	/* TODO: find some way to specify which track an error occurred on */

	/* TODO: try reversing the input bits if these don't work */

	/* Track 1 */
	if ('\0' == in->t1[0]) {
		result->t1[0] = '\0';
		result->t1_encoding = BC_ENCODING_NONE;
		err = 0;
	} else {
		result->t1_encoding = BC_ENCODING_ALPHA;
		err = bc_decode_format(in->t1, result->t1, BC_T1_DECODED_SIZE, 7);
		/* TODO: try other encodings if this doesn't work */
	}

	rc = err;

	/* Track 2 */
	if ('\0' == in->t2[0]) {
		result->t2[0] = '\0';
		result->t2_encoding = BC_ENCODING_NONE;
		err = 0;
	} else {
		result->t2_encoding = BC_ENCODING_BCD;
		err = bc_decode_format(in->t2, result->t2, BC_T2_DECODED_SIZE, 5);
		/* TODO: try other encodings if this doesn't work */
	}

	/* if previous tracks were ok but this one returned an error, update
	 * the overall return code accordingly
	 */
	if (0 == rc) {
		rc = err;
	}

	/* Track 3 */
	if ('\0' == in->t3[0]) {
		result->t3[0] = '\0';
		result->t3_encoding = BC_ENCODING_NONE;
		err = 0;
	} else {
		result->t3_encoding = BC_ENCODING_ALPHA;
		err = bc_decode_format(in->t3, result->t3, BC_T1_DECODED_SIZE, 7);
		/* TODO: try other encodings if this doesn't work */
	}

	/* if previous tracks were ok but this one returned an error, update
	 * the overall return code accordingly
	 */
	if (0 == rc) {
		rc = err;
	}

	return rc;
}

int bc_find_fields(struct bc_decoded* result)
{
	return bc_decode_fields(result);
}

const char* bc_strerror(int err)
{
	switch (err)
	{
	case 0:					return "Success";
	case BCERR_INVALID_INPUT:		return "Invalid input";
	case BCERR_PARITY_MISMATCH:		return "Parity mismatch";
	case BCERR_RESULT_FULL:			return "Result full";
	case BCERR_INVALID_TRACK:		return "Invalid track";
	case BCERR_NO_FORMAT_FILE:		return "No format file";
	case BCERR_PCRE_COMPILE_FAILED:		return "PCRE compile failed";
	case BCERR_FORMAT_MISSING_PERIOD:	return "Format missing period";
	case BCERR_FORMAT_MISSING_NAME:		return "Format missing name";
	case BCERR_NO_MATCHING_FORMAT:		return "No matching format";
	case BCERR_BAD_FORMAT_ENCODING_TYPE:	return "Bad format encoding type";
	case BCERR_FORMAT_MISSING_RE:		return "Format missing regular expression";
	default:				return "Unknown error";
	}
}
