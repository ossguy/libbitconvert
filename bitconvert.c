/* Assumptions:
 * - character set is ALPHA or BCD
 * - each character is succeeded by an odd parity bit
 */

#include "bitconvert.h"
#include <string.h>	/* strspn, strlen */
#include <pcre.h>	/* pcre* */
#include <stdio.h>	/* FILE, fopen, fgets */

/* TODO: create bc_init() functions like in md5 library; need to have two: one
 * for automatic result_len and another for user-specified result_len with
 * documentation stating that you can use user-specified result_len if you are
 * concerned about size
 */

/* maximum length of a line in a format file */
#define FORMAT_LEN	1024

/* maximum number of captured substrings in a track's regular expression;
 * thus, this is also the maximum number of fields per track
 */
#define MAX_CAPTURED_SUBSTRINGS	64


/* return codes internal to the library; these MUST NOT overlap with BCERR_* */
#define BCINT_OFFSET	-1024
#define BCINT_NO_MATCH	BCINT_OFFSET -1


char to_character(char bits, unsigned char value)
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
			result[result_idx] = to_character(format_bits, current_value);
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

int bc_decode_track_fields(char* input, FILE* formats,
	char field_names[BC_NUM_FIELDS][BC_FIELD_SIZE],
	char field_values[BC_NUM_FIELDS][BC_FIELD_SIZE])
{
	pcre* re;
	const char* error;
	int erroffset;
	int rc;
	int ovector[3 * MAX_CAPTURED_SUBSTRINGS];
	int i;
	int j;
	int k;
	int num_fields;
	char buf[FORMAT_LEN];
	const char* result;
	char* field;

	fgets(buf, FORMAT_LEN, formats);
	buf[strlen(buf) - 1] = '\0';

	/* TODO: parse out string prefix */

	re = pcre_compile(buf, 0, &error, &erroffset, NULL);
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
		/* if there was no match, skip the field descriptions */
		for (k = 0; k < num_fields &&
			fgets(buf, FORMAT_LEN, formats) && buf[0] != '\n';
			k++);
		return BCINT_NO_MATCH;
	}

	/* find first entry not filled in by previous tracks */
	/* TODO: this could be eliminated by making j a parameter to
	 * bc_decode_track_fields
	 */
	for (j = 0; field_names[j][0] != '\0'; j++);

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

		strcpy(field_names[j], field);
		strcpy(field_values[j], result);
		j++;
	}

	field_names[j][0] = '\0';

	return 0;
}

/* TODO: fix bc_decode_fields so it checks if inputted lines are longer than or
 * as long as FORMAT_LEN
 */

/* TODO: fix bc_decode_fields so it has variable-length field list; requires
 * fixing the constants in the ovector and fields initialization
 */

int bc_decode_fields(char* t1_input, char* t2_input, char* t3_input,
	char field_names[BC_NUM_FIELDS][BC_FIELD_SIZE],
	char field_values[BC_NUM_FIELDS][BC_FIELD_SIZE])
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

	while (fgets(name, FORMAT_LEN, formats)) {
		/* TODO: check lengths before strcpy (in general, but here esp.)
		 */
		strcpy(field_names[0], "Type of card");

		name[strlen(name) - 1] = '\0'; /* remove '\n' */
		strcpy(field_values[0], name);

		/* empty the rest of the fields list */
		field_names[1][0] = '\0';


		err = bc_decode_track_fields(t1_input, formats,
			field_names, field_values);
		rc = err;

		err = bc_decode_track_fields(t2_input, formats,
			field_names, field_values);
		/* if previous tracks were ok but this one returned an error,
		 * update the overall return code accordingly
		 */
		if (0 == rc) {
			rc = err;
		}

		rc = bc_decode_track_fields(t3_input, formats,
			field_names, field_values);
		/* if previous tracks were ok but this one returned an error,
		 * update the overall return code accordingly
		 */
		if (0 == rc) {
			rc = err;
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
	}

	fclose(formats);

	return rv;
}

void bc_init(struct bc_input* in)
{
	in->t1[0] = '\0';
	in->t2[0] = '\0';
	in->t3[0] = '\0';
}

int bc_decode(struct bc_input* in, struct bc_decoded* result)
{
	int err;
	int rc;

	/* initialize the fields list */
	result->field_names[0][0] = '\0';

	/* TODO: find some way to specify which track and error occurred on */

	/* TODO: try reversing the input bits if these don't work */

	/* Track 1 */
	result->t1_encoding = BC_ENCODING_ALPHA;
	err = bc_decode_format(in->t1, result->t1, BC_T1_DECODED_SIZE, 7);
	/* TODO: try other encodings if this doesn't work */

	rc = err;

	/* Track 2 */
	result->t2_encoding = BC_ENCODING_BCD;
	err = bc_decode_format(in->t2, result->t2, BC_T2_DECODED_SIZE, 5);
	/* TODO: try other encodings if this doesn't work */

	/* if previous tracks were ok but this one returned an error, update
	 * the overall return code accordingly
	 */
	if (0 == rc) {
		rc = err;
	}

	/* Track 3 */
	result->t3_encoding = BC_ENCODING_ALPHA;
	err = bc_decode_format(in->t3, result->t3, BC_T1_DECODED_SIZE, 7);
	/* TODO: try other encodings if this doesn't work */

	/* if previous tracks were ok but this one returned an error, update
	 * the overall return code accordingly
	 */
	if (0 == rc) {
		rc = err;
	}

	/* only do field lookup if there were no errors during parsing */
	if (0 == rc) {
		rc = bc_decode_fields(result->t1, result->t2, result->t3,
			result->field_names, result->field_values);
	}

	return rc;
}
