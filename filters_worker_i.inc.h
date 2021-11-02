// XXX how to rid of these
#ifdef BINFILTER
struct bfiltervec filters;
#endif // BINFILTER

void filters_init(void)
{
	VEC_INIT(filters);
}



#include "filters_common.inc.h"


#define PREFILTER \
	u8 lastmatch[PUBLIC_LEN]; \
	memset(lastmatch,0xFF,PUBLIC_LEN);

#define POSTFILTER

#define FILTERSUCCESS(pk) \
	memcpy(lastmatch,pk,PUBLIC_LEN);

#define DOFILTER(it,pk,code) \
do { \
	if (unlikely(memcmp(pk,lastmatch,PUBLIC_LEN) < 0)) { \
		code; \
	} \
} while (0)
