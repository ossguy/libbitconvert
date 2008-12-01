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

int main(void)
{
	FILE* input;
	struct bc_input in;
	struct bc_decoded result;
	int rv;
	int i;

	input = stdin;
	bc_init(&in);

	while (1)
	{
		if (NULL == get_track(input, in.t1, sizeof(in.t1)))
			break;
		if (NULL == get_track(input, in.t2, sizeof(in.t2)))
			break;
		if (NULL == get_track(input, in.t3, sizeof(in.t3)))
			break;

		rv = bc_decode(&in, &result);

		printf("rv: %d\n", rv);
		printf("Track 1 - data_len: %lu, data:\n`%s`\n",
			(unsigned long)strlen(result.t1), result.t1);
		printf("Track 2 - data_len: %lu, data:\n`%s`\n",
			(unsigned long)strlen(result.t2), result.t2);
		printf("Track 3 - data_len: %lu, data:\n`%s`\n",
			(unsigned long)strlen(result.t3), result.t3);

		printf("\n=== Fields ===\n");
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
