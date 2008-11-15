/* Assumptions:
 * - character set is ALPHA or BCD
 * - each character is succeeded by an odd parity bit
 */

#include "bitconvert.h"
#include <string.h>	/* strspn, strlen */
#include <pcre.h>	/* pcre* */
#include <stdio.h>	/* FILE, fopen, fgets */
#include <stdlib.h>	/* malloc */

/* TODO: create bc_init() functions like in md5 library; need to have two: one
 * for automatic result_len and another for user-specified result_len with
 * documentation stating that you can use user-specified result_len if you are
 * concerned about size
 */

/* TODO: create bc_finish()/bc_free() function to cleanup dynamically-allocated
 * memory
 */

/* maximum length of a line in a format file */
#define FORMAT_LEN	1024

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
			/* assimilating a 0 is a no-op (we've already shifted
			 * the previous bits up)
			 */
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

/* TODO: fix bc_decode_fields so it checks if inputted lines are longer than or
 * as long as FORMAT_LEN
 */

/* TODO: fix bc_decode_fields so it has variable-length field list; requires
 * fixing the constants in the ovector and fields initialization
 */

int bc_decode_fields(char* input, char*** fields)
{
	pcre* re;
	const char* error;
	int erroffset;
	int rc;
	int ovector[30];
	int i;
	int j;
	FILE* formats;
	char buf[FORMAT_LEN];
	char name[FORMAT_LEN];
	const char* result;
	char* field;

	/* give enough room for 10 fields */
	(*fields) = malloc(sizeof(char*) * 20);

	formats = fopen("formats", "r");

	if (NULL == formats) {
		return BCERR_NO_FORMAT_FILE;
	}

	while (fgets(name, FORMAT_LEN, formats)) {
		fgets(buf, FORMAT_LEN, formats);
		buf[strlen(buf)-1] = '\0';
		re = pcre_compile(buf, 0, &error, &erroffset, NULL);
		if (NULL == re) {
			/* TODO: find some way to pass back error and erroffset;
			 * these would be useful to the user
			 */
			return BCERR_PCRE_COMPILE_FAILED;
		}

		/* TODO: if positive, see if return code matches the number
		 * of strings we expect (see pcre.txt line 2113)
		 */
		/* TODO: on error (negative return code), see if it's a bad
		 * error (ie. invalid input) and return if it is; a list of
		 * errors is available starting at pcre.txt line 1878
		 */
		rc = pcre_exec(re, NULL, input, strlen(input), 0, 0, ovector, 30);
		if (rc < 0) {
			while (fgets(buf, FORMAT_LEN, formats) && buf[0] != '\n');
			continue;
		}

		j = 0;

		(*fields)[j] = malloc(sizeof(char) * strlen("Type of card") + 1);
		strcpy((*fields)[j], "Type of card");
		j++;

		name[strlen(name) - 1] = '\0'; /* remove '\n' */
		(*fields)[j] = malloc(sizeof(char) * strlen(name) + 1);
		strcpy((*fields)[j], name);
		j++;

		while (fgets(buf, FORMAT_LEN, formats) && buf[0] != '\n') {
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

			(*fields)[j] = malloc(sizeof(char) * strlen(field) + 1);
			strcpy((*fields)[j], field);
			j++;

			(*fields)[j] = malloc(sizeof(char) * strlen(result) + 1);
			strcpy((*fields)[j], result);
			j++;
		}

		(*fields)[j] = NULL;
		(*fields)[j + 1] = NULL;

		return 0;
	}

	return BCERR_NO_MATCHING_FORMAT;
}

int bc_decode(char* bits, struct bc_result* result, int track)
{
	int rc;

	/* TODO: try reversing the bits if these don't work */
	if (BC_TRACK_2 == track)
	{
		result->encoding = BC_ENCODING_BCD;
		rc = bc_decode_format(bits, result->data, result->data_len, 5);
		/* TODO: try other encodings if this doesn't work */

		if (0 == rc) {
			rc = bc_decode_fields(result->data, &(result->fields));
		}
		return rc;
	}
	else if (BC_TRACK_1 == track || BC_TRACK_3 == track)
	{
		result->encoding = BC_ENCODING_ALPHA;
		rc = bc_decode_format(bits, result->data, result->data_len, 7);
		/* TODO: try other encodings if this doesn't work */

		if (0 == rc) {
			rc = bc_decode_fields(result->data, &(result->fields));
		}
		return rc;
	}
	else if (BC_TRACK_UNKNOWN)
	{
		/* TODO: try BCD, ALPHA, and other encodings */
		return BCERR_INVALID_TRACK;
	}
	else
	{
		return BCERR_INVALID_TRACK;
	}
}
