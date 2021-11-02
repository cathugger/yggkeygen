
#define BINFILTER

// whether binfilter struct is needed
#ifdef BINFILTER
# define NEEDBINFILTER
#endif


#ifdef NEEDBINFILTER

# ifndef BINFILTERLEN
#  define BINFILTERLEN PUBLIC_LEN
# endif

struct binfilter {
	u8 f[BINFILTERLEN];
	size_t len; // real len minus one
	u8 mask;
} ;

VEC_STRUCT(bfiltervec,struct binfilter);

#ifdef BINFILTER
extern struct bfiltervec filters;
#endif

#endif // NEEDBINFILTER


extern void filters_init(void);
extern void filters_add(const char *filter);
extern size_t filters_count(void);
