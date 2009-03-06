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
#include <stdlib.h>	/* malloc and friends */
#include <pcre.h>	/* pcre* */
#include <stdio.h>	/* FILE, fopen, fgets */
#include <ctype.h>	/* isspace */

/* TODO: add appropriate calls to pcre_free (probably just re variables) */


/* return codes internal to the library; these MUST NOT overlap with BCERR_* */
#define BCINT_OFFSET	1024
#define BCINT_NO_MATCH	BCINT_OFFSET + 1
#define BCINT_EOF_FOUND	BCINT_OFFSET + 2


void (*send_error)(const char*);

char to_ascii(char bits, unsigned char value)
{
	if (5 == bits) {
		/* the BCD encoding is the subset of ASCII beginning at
		 * character 48 ('0'), ending at character 63; see
		 *	http://www.cyberd.co.uk/support/technotes/isocards.htm
		 *	http://en.wikipedia.org/wiki/ASCII#ASCII_printable_characters
		 */
		return '0' + value;
	} else if (7 == bits) {
		/* the ALPHA encoding is the subset of ASCII beginning at
		 * character 32 (' '), ending at character 95; see
		 *	http://www.cyberd.co.uk/support/technotes/isocards.htm
		 *	http://en.wikipedia.org/wiki/ASCII#ASCII_printable_characters
		 */
		return ' ' + value;
	}
	return '\0';
}

int bc_decode_format(char* bits, char** result, unsigned char format_bits)
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

	/* Allocate space for the result: the length of the input minus the
	 * leading zeroes divided by the number of bits per output character.
	 * Note that we are not allocating space for a possible partial output
	 * character (ie. if we have 17/5, we malloc only 3 bytes) because a
	 * partial output character is not meaningful (though we do add 1 for
	 * the null terminator).  Also note that we are allocating space for
	 * trailing zeroes because it's hard to determine here how many
	 * trailing zeroes there will be.
	 */
	*result = malloc( ((bits_len - start_idx) / format_bits) + 1 );

	result_idx = 0;
	for (i = start_idx; (i + format_bits) <= bits_len; i += format_bits) {
		current_value = 0;
		parity = 1;

		for (j = 0; j < (format_bits - 1); j++) {
			if ('1' == bits[i + j]) {
				/* push a 1 onto the front of our accumulator */
				current_value |= (1 << j);

				parity ^= 1; /* flip parity bit */
			} else if ('0' != bits[i + j]) {
				retval = BCERR_INVALID_INPUT;
				break;
			}
			/* assimilating a 0 is a no-op */
		}

		/* since C doesn't have multi-level breaks */
		if (0 != retval) {
			break;
		}

		if ("01"[parity] != bits[i + j]) {
			retval = BCERR_PARITY_MISMATCH;
			break;
		}

		(*result)[result_idx] = to_ascii(format_bits, current_value);
		result_idx++;

		if ('?' == (*result)[result_idx - 1]) {
			/* found end sentinel; we're done */
			break;
		}
	}

	(*result)[result_idx] = '\0';
	/* no need to increment result_idx; we are done */

	return retval;
}

int dynamic_fgets(char** buf, size_t* size, FILE* file)
{
	char* offset;
	size_t old_size;

	if (!fgets(*buf, *size, file)) {
		return BCINT_EOF_FOUND;
	}

	if ((*buf)[strlen(*buf) - 1] == '\n') {
		return 0;
	}

	do {
		/* we haven't read the whole line so grow the buffer */
		void* t;
		old_size = *size;
		t = realloc(*buf, *size * 2);
		if (NULL == t) {
			/* TODO: add error string here */
			return BCERR_OUT_OF_MEMORY;
		}
		*size *= 2;
		*buf = t;
		offset = *buf + old_size - 1;
	} while ( fgets(offset, old_size + 1, file)
		&& offset[strlen(offset) - 1] != '\n' );

	return 0;
}

int bc_decode_track_fields(char* input, int encoding, int track, FILE* formats,
	struct bc_decoded* d, size_t* fields_size)
{
	pcre* re;
	const char* error;
	int erroffset;
	int exec_rc;
	int rc;
	int rv;
	int* ovector;
	int ovector_size;
	size_t j;
	int k;
	int num_fields;
	int fgets_rc;
	char* buf;
	size_t buf_size;
	const char* result;
	char* temp_ptr;
	int field_name_len;
	void* t;

	rv = 0;
	fgets_rc = 0;
	ovector = NULL;
	buf = NULL;
	k = 0;
	num_fields = -1;

	buf_size = 2;
	buf = malloc(buf_size);
	if (NULL == buf) {
		/* TODO: add to error string */
		rv = BCERR_OUT_OF_MEMORY;
		goto skip_fields;
	}

	if ( (fgets_rc = dynamic_fgets(&buf, &buf_size, formats)) ) {
		if (BCINT_EOF_FOUND == fgets_rc) {
			/* TODO: add line number information to error string */
			rv = BCERR_FORMAT_MISSING_TRACK;
			goto skip_fields;
		} else {
			rv = fgets_rc;
			goto skip_fields;
		}
	}
	buf[strlen(buf) - 1] = '\0';

	/* TODO: parse out string prefix */
	temp_ptr = strchr(buf, ':');
	if (NULL == temp_ptr) {
		if (strcmp(buf, "none") == 0) {
			if (BC_ENCODING_NONE == encoding) {
				goto skip_fields;
			} else {
				rv = BCINT_NO_MATCH;
				goto skip_fields;
			}
		} else if (strcmp(buf, "unknown") == 0) {
			goto skip_fields;
		}
		/* TODO: print name of encoding type to error method */
		rv = BCERR_BAD_FORMAT_ENCODING_TYPE;
		goto skip_fields;
	}

	/* replace ':' with '\0' so buf represents encoding type */
	temp_ptr[0] = '\0';

	/* increment past the '\0', which replaced ':' */
	temp_ptr++;

	/* skip whitespace between ':' and regular expression */
	while (isspace((int)temp_ptr[0]) && temp_ptr[0] != '\0') {
		temp_ptr++;
	}

	/* if there is no regular expression after the data format specifier */
	if (temp_ptr[0] == '\0') {
		rv = BCERR_FORMAT_MISSING_RE;
		goto skip_fields;
	}

	/* temp_ptr now points at the regular expression */
	re = pcre_compile(temp_ptr, 0, &error, &erroffset, NULL);
	if (NULL == re) {
		/* TODO: find some way to pass back error and erroffset;
		 * these would be useful to the user
		 */
		rv = BCERR_PCRE_COMPILE_FAILED;
		goto skip_fields;
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
		rv = BCERR_BAD_FORMAT_ENCODING_TYPE;
		goto skip_fields;
	}

	/* try to match the regular expression */
	ovector_size = 2 * 3; /* each captured substring needs 3 elements */
	ovector = malloc(ovector_size * sizeof(*ovector));
	if (NULL == ovector) {
		/* TODO: add to error string */
		rv = BCERR_OUT_OF_MEMORY;
		goto skip_fields;
	}
	while (1) {
		/* TODO: if positive, see if return code matches the number
		 * of strings we expect (see pcre.txt line 2113)
		 */
		/* TODO: on error (negative return code), see if it's a bad
		 * error (ie. invalid input) and return if it is; a list of
		 * errors is available starting at pcre.txt line 2155
		 */
		exec_rc = pcre_exec(re, NULL, input, strlen(input), 0, 0,
			ovector, ovector_size);
		if (exec_rc < 0) {
			rv = BCINT_NO_MATCH;
			goto skip_fields;
		}
		if (exec_rc > 0) {
			/* we found a match so break to fill in the fields */
			break;
		}

		/* exec_rc == 0, which means ovector is too small (see pcre.txt
		 * lines 2123-2125) so we need to grow it
		 */
		t = realloc(ovector, 2 * ovector_size * sizeof(*ovector));
		if (NULL == t) {
			/* TODO: add to error string */
			rv = BCERR_OUT_OF_MEMORY;
			goto skip_fields;
		}
		ovector_size *= 2;
		ovector = t;
	}

	/* find first entry not filled in by previous tracks */
	/* TODO: this could be eliminated by making j a parameter to
	 * bc_decode_track_fields
	 */
	for (j = 0; d->field_names[j] != NULL; j++);

	/* read until we have read all the fields or we encounter end of file
	 * or an empty line
	 */
	for (k = 0; k < num_fields
		&& !(fgets_rc = dynamic_fgets(&buf, &buf_size, formats))
		&& buf[0] != '\n'; k++) {

		/* find the first period */
		temp_ptr = strchr(buf, '.');
		if (NULL == temp_ptr) {
			/* TODO: find some way to return buf; would be
			 * useful to the user for debugging
			 */
			rv = BCERR_FORMAT_MISSING_PERIOD;
			goto skip_fields;
		}

		/* replace '.' with '\0' to make new string */
		temp_ptr[0] = '\0';

		/* find a named substring using the new string as the name */
		rc = pcre_get_named_substring(re, input, ovector, exec_rc, buf,
			&result);
		if (rc <= 0) {
			/* TODO: add information about type of error; see
			 * pcre.txt line 2368 for some details, can probably
			 * also return memory errors
			 */
			rv = BCERR_FORMAT_NAMED_SUBSTRING;
			goto skip_fields;
		}

		/* verify '.' is followed by a space and at least one other
		 * character (for the field name); as a side effect, the pointer
		 * will advance to the beginning of the field name
		 */
		temp_ptr++;
		if (temp_ptr[0] != ' ') {
			/* TODO: find some way to return buf; would be
			 * useful to the user for debugging
			 */
			rv = BCERR_FORMAT_MISSING_SPACE;
			goto skip_fields;
		}
		temp_ptr++;
		if (temp_ptr[0] == '\0') {
			/* TODO: find some way to return buf; would be
			 * useful to the user for debugging
			 */
			rv = BCERR_FORMAT_MISSING_NAME;
			goto skip_fields;
		}

		/* if we've reached the end of the arrays, grow the arrays */
		if (j == *fields_size) {
			t = realloc(d->field_names,
				2 * *fields_size * sizeof(*d->field_names));
			if (NULL == t) {
				/* TODO: add to error string */
				rv = BCERR_OUT_OF_MEMORY;
				goto skip_fields;
			}
			d->field_names = t;

			t = realloc(d->field_values,
				2 * *fields_size * sizeof(*d->field_values));
			if (NULL == t) {
				/* TODO: add to error string */
				rv = BCERR_OUT_OF_MEMORY;
				goto skip_fields;
			}
			d->field_values = t;

			t = realloc(d->field_tracks,
				2 * *fields_size * sizeof(*d->field_tracks));
			if (NULL == t) {
				/* TODO: add to error string */
				rv = BCERR_OUT_OF_MEMORY;
				goto skip_fields;
			}
			d->field_tracks = t;

			*fields_size *= 2;
		}

		field_name_len = strlen(temp_ptr) - 1;
		temp_ptr[field_name_len] = '\0'; /* remove '\n' */

		d->field_names[j] = malloc(field_name_len + 1);
		if (NULL == d->field_names[j]) {
			/* TODO: add information to the error string */
			rv = BCERR_OUT_OF_MEMORY;
			goto skip_fields;
		}
		strcpy(d->field_names[j], temp_ptr);

		d->field_values[j] = result;
		d->field_tracks[j] = track;
		j++;
	}

	/* if there was an error during dynamic_fgets, return that error */
	if (0 != fgets_rc && BCINT_EOF_FOUND != fgets_rc) {
		rv = fgets_rc;
	}

	d->field_names[j] = NULL;
	goto done;

skip_fields:
	/* if there was no match, or if we found an error before we
	 * finished reading the fields, skip the field descriptions
	 */
	if (num_fields == -1 && fgets_rc == 0) {
		int c = getc(formats);
		if (c != EOF) {
			ungetc(c, formats);
		}
		if (!isdigit(c)) {
			goto done;
		}
	}
	if (k + 1 < num_fields) {
		int c = getc(formats);
		int seen_newline;
		int at_end;
		do {
			seen_newline = (c == '\n');
			c = getc(formats);
			at_end = seen_newline && !isdigit(c);
		} while (c != EOF && !at_end);

		if (c != EOF) {
			ungetc(c, formats);
		}
	}

	/* if there was an error during dynamic_fgets, return that error */
	if (0 != fgets_rc && BCINT_EOF_FOUND != fgets_rc) {
		rv = fgets_rc;
	}

done:
	free(buf);
	free(ovector);

	return rv;
}

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
	int fgets_rc;
	int rv;
	char* name;
	size_t name_size;
	FILE* formats;
	size_t fields_size;

	formats = fopen("formats", "r");

	if (NULL == formats) {
		return BCERR_NO_FORMAT_FILE;
	}

	rv = BCERR_NO_MATCHING_FORMAT;

	/* initialize the names and fields lists */
	d->name = NULL;

	name_size = 2;
	name = malloc(name_size);
	if (NULL == name) {
		fclose(formats);
		/* TODO: add to error string */
		return BCERR_OUT_OF_MEMORY;
	}

	fields_size = 2;
	d->field_names = malloc(fields_size * sizeof(*d->field_names));
	if (NULL == d->field_names) {
		fclose(formats);
		/* TODO: add to error string */
		return BCERR_OUT_OF_MEMORY;
	}
	d->field_values = malloc(fields_size * sizeof(*d->field_values));
	if (NULL == d->field_values) {
		fclose(formats);
		/* TODO: add to error string */
		return BCERR_OUT_OF_MEMORY;
	}
	d->field_tracks = malloc(fields_size * sizeof(*d->field_tracks));
	if (NULL == d->field_tracks) {
		fclose(formats);
		/* TODO: add to error string */
		return BCERR_OUT_OF_MEMORY;
	}
	d->field_names[0] = NULL;

	while ( !(fgets_rc = dynamic_fgets(&name, &name_size, formats)) ) {

		err = bc_decode_track_fields(d->t1, d->t1_encoding, BC_TRACK_1,
			formats, d, &fields_size);
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
			formats, d, &fields_size);
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
			formats, d, &fields_size);
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
			name[strlen(name) - 1] = '\0'; /* remove '\n' */
			d->name = name;
			rv = 0;
			break;
		}

		/* skip to beginning of next card specification; each card is
		 * separated by an empty line
		 */
		while ( !(fgets_rc = dynamic_fgets(&name, &name_size, formats))
			&& name[0] != '\n' );
		if (0 != fgets_rc && BCINT_EOF_FOUND != fgets_rc) {
			break;
		}
	}

	/* don't free name if we found a match; the allocated memory is used by
	 * d->name, which will be passed to the user
	 */
	if (0 != rv) {
		free(name);
	}

	/* if there was an error during dynamic_fgets, return that error */
	if (0 != fgets_rc && BCINT_EOF_FOUND != fgets_rc) {
		rv = fgets_rc;
	}

	fclose(formats);

	return rv;
}

int bc_combine_track(char* forward, char* backward, char** combined)
{
	size_t forward_len;
	size_t backward_len;
	char* forward_pos;
	char* backward_pos;
	char* forward_start;
	size_t end_zeroes;

	/* ensure the input consists of exclusively 0s and 1s and find length */

	forward_len = strspn(forward, "01");
	backward_len = strspn(backward, "01");

	if (forward[forward_len] != '\0') {
		/* TODO: set the error string to specify forward track */
		/* TODO: find way to also print track number; another param? */
		return BCERR_INVALID_INPUT;
	}
	if (backward[backward_len] != '\0') {
		/* TODO: set the error string to specify backward track */
		return BCERR_INVALID_INPUT;
	}


	/* TODO: this is slightly larger than we need, but it may be hard to
	 * determine exactly how much to allocate
	 */
	*combined = malloc(forward_len + backward_len + 1);
	if (NULL == *combined) {
		return BCERR_OUT_OF_MEMORY;
	}

	forward_pos = strchr(forward, (int)'1');
	backward_pos = strrchr(backward, (int)'1');

	/* don't bother handling empty strings; too much work for now */
	/* TODO: should handle this or make it unnecessary */
	if (NULL == forward_pos || NULL == backward_pos) {
		return BCERR_UNIMPLEMENTED;
	}

	end_zeroes = &(backward[backward_len]) - backward_pos;
	forward_start = forward_pos;

	while (forward_pos != &(forward[forward_len]) &&
		backward_pos != backward) {

		if (backward_pos[0] != forward_pos[0]) {
			return -1;
		}

		forward_pos++;
		backward_pos--;
	}

	memset(*combined, '0', end_zeroes);
	memcpy(&((*combined)[end_zeroes]), forward_start,
		&(forward[forward_len]) - forward_start);

	return 0;
}

void bc_init(void (*error_callback)(const char*))
{
	send_error = error_callback;
}

int bc_decode(struct bc_input* in, struct bc_decoded* result)
{
	int err;
	int rc;

	/* initialize name and fields list */
	result->name = NULL;
	result->field_names = NULL;

	/* TODO: find some way to specify which track an error occurred on */

	/* TODO: try reversing the input bits if these don't work */

	/* Track 1 */
	if (NULL == in->t1 || '\0' == in->t1[0]) {
		result->t1 = NULL;
		result->t1_encoding = BC_ENCODING_NONE;
		err = 0;
	} else {
		result->t1_encoding = BC_ENCODING_ALPHA;
		err = bc_decode_format(in->t1, &result->t1, 7);
		/* TODO: try other encodings if this doesn't work */
	}

	rc = err;

	/* Track 2 */
	if (NULL == in->t2 || '\0' == in->t2[0]) {
		result->t2 = NULL;
		result->t2_encoding = BC_ENCODING_NONE;
		err = 0;
	} else {
		result->t2_encoding = BC_ENCODING_BCD;
		err = bc_decode_format(in->t2, &result->t2, 5);
		/* TODO: try other encodings if this doesn't work */
	}

	/* if previous tracks were ok but this one returned an error, update
	 * the overall return code accordingly
	 */
	if (0 == rc) {
		rc = err;
	}

	/* Track 3 */
	if (NULL == in->t3 || '\0' == in->t3[0]) {
		result->t3 = NULL;
		result->t3_encoding = BC_ENCODING_NONE;
		err = 0;
	} else {
		result->t3_encoding = BC_ENCODING_ALPHA;
		err = bc_decode_format(in->t3, &result->t3, 7);
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

int bc_combine(struct bc_input* forward, struct bc_input* backward,
	struct bc_input* combined)
{
	combined->t1 = "";
	combined->t3 = "";
	return bc_combine_track(forward->t2, backward->t2, &combined->t2);
}

const char* bc_strerror(int err)
{
	switch (err) {
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
	case BCERR_UNIMPLEMENTED:		return "Unimplemented";
	case BCERR_OUT_OF_MEMORY:		return "Out of memory";
	case BCERR_FORMAT_MISSING_TRACK:	return "Format missing track description";
	case BCERR_FORMAT_MISSING_SPACE:	return "Format missing space";
	case BCERR_FORMAT_NAMED_SUBSTRING:	return "Format named substring";
	default:				return "Unknown error";
	}
}

void bc_input_free(struct bc_input* in)
{
	free(in->t2);
}

void bc_decoded_free(struct bc_decoded* result)
{
	int i;

	free(result->t1);
	free(result->t2);
	free(result->t3);
	free(result->name);

	for (i = 0; result->field_names != NULL
		&& result->field_names[i] != NULL; i++) {

		free((char*)result->field_names[i]);
		free((char*)result->field_values[i]);
		result->field_names[i] = NULL;
		result->field_values[i] = NULL;
	}

	free(result->field_names);
	free(result->field_values);
	free(result->field_tracks);
}
