
#ifdef BINFILTER

static inline size_t S(filter_len)(size_t i)
{
	size_t c = VEC_BUF(filters,i).len * 8;
	u8 v = VEC_BUF(filters,i).mask;
	for (size_t k = 0;;) {
		if (!v)
			return c;
		++c;
		if (++k >= 8)
			return c;
		v <<= 1;
	}
}
#define filter_len S(filter_len)

#endif // BINFILTER
