#ifndef H_BITCONVERT
#define H_BITCONVERT

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h> /* size_t */

#define BCERR_INVALID_INPUT		-1
#define BCERR_PARITY_MISMATCH		-2
#define BCERR_RESULT_FULL		-3
#define BCERR_INVALID_TRACK		-4
#define BCERR_NO_FORMAT_FILE		-5
#define BCERR_PCRE_COMPILE_FAILED	-6
#define BCERR_FORMAT_MISSING_PERIOD	-7
#define BCERR_FORMAT_MISSING_NAME	-8
#define BCERR_NO_MATCHING_FORMAT	-9

#define BC_TRACK_UNKNOWN 0
#define BC_TRACK_1	 1
#define BC_TRACK_2	 2
#define BC_TRACK_3	 3

#define BC_ENCODING_BINARY 1
#define BC_ENCODING_BCD    4
#define BC_ENCODING_ALPHA  6
#define BC_ENCODING_ASCII  7

struct bc_result {
	char* data;
	size_t data_len;
	int encoding;
	char** fields;
};


int bc_decode(char* bits, struct bc_result* result, int track);

#ifdef __cplusplus
}
#endif

#endif /* H_BITCONVERT */
