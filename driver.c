#include "bitconvert.h"
#include <stdio.h>  /* FILE, fgets, printf */
#include <string.h> /* strlen */
#include <stdlib.h> /* malloc */

#define BITS_LEN	1024
#define RESULT_LEN	1024

int main(void)
{
	FILE* input;
	char bits[BITS_LEN];
	struct bc_result result;
	int rv;
	int i;
	unsigned char track = BC_TRACK_1;

	result.data_len = RESULT_LEN;
	result.data = malloc(sizeof(char) * result.data_len);

	input = stdin;

	while (fgets(bits, BITS_LEN, input))
	{
		int bits_end = strlen(bits);

		/* strip trailing newline */
		if ('\n' == bits[bits_end - 1])
		{
			bits[bits_end - 1] = '\0';
		}

		rv = bc_decode(bits, &result, track);

		printf("rv: %d, data_len: %d, data:\n`%s`\n", rv, strlen(result.data), result.data);

		if (0 == rv)
		{
			printf("=== Fields ===\n");
			for (i = 0; result.fields[i] != NULL; i += 2)
			{
				printf("%s: %s\n", result.fields[i], result.fields[i + 1]);
			}
		}
		/* add empty line to visually separate tracks */
		printf("\n");

		/* move to next track */
		if (BC_TRACK_3 == track)
		{
			track = BC_TRACK_1;
		}
		else
		{
			track++;
		}
	}

	fclose(input);
	free(result.data);

	return 0;
}
