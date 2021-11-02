
#ifdef BINFILTER

struct bfiltervec filters;

#endif // BINFILTER



void filters_init(void)
{
	VEC_INIT(filters);
}



#include "filters_common.inc.h"



#ifdef BINFILTER

# ifndef BINSEARCH

#define MATCHFILTER(it,pk) ( \
	memcmp(pk,VEC_BUF(filters,it).f,VEC_BUF(filters,it).len) == 0 && \
	(pk[VEC_BUF(filters,it).len] & VEC_BUF(filters,it).mask) == VEC_BUF(filters,it).f[VEC_BUF(filters,it).len])

#define DOFILTER(it,pk,code) \
do { \
	for (it = 0;it < VEC_LENGTH(filters);++it) { \
		if (unlikely(MATCHFILTER(it,pk))) { \
			code; \
			break; \
		} \
	} \
} while (0)

# else // BINSEARCH

#define DOFILTER(it,pk,code) \
do { \
	for (size_t down = 0,up = VEC_LENGTH(filters);down < up;) { \
		it = (up + down) / 2; \
		{ \
			register int filterdiff = memcmp(pk,VEC_BUF(filters,it).f,VEC_BUF(filters,it).len); \
			if (filterdiff < 0) { \
				up = it; \
				continue; \
			} \
			if (filterdiff > 0) { \
				down = it + 1; \
				continue; \
			} \
		} \
		if ((pk[VEC_BUF(filters,it).len] & VEC_BUF(filters,it).mask) < \
			VEC_BUF(filters,it).f[VEC_BUF(filters,it).len]) \
		{ \
			up = it; \
			continue; \
		} \
		if ((pk[VEC_BUF(filters,it).len] & VEC_BUF(filters,it).mask) > \
			VEC_BUF(filters,it).f[VEC_BUF(filters,it).len]) \
		{ \
			down = it + 1; \
			continue; \
		} \
		{ \
			code; \
			break; \
		} \
	} \
} while (0)

# endif // BINSEARCH

#define PREFILTER
#define POSTFILTER

#endif // BINFILTER
