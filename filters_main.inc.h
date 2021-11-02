
#include "filters_common.inc.h"

#ifdef BINFILTER

static inline int filter_compare(const void *p1,const void *p2)
{
	const struct binfilter *b1 = (const struct binfilter *)p1;
	const struct binfilter *b2 = (const struct binfilter *)p2;

	size_t l = b1->len <= b2->len ? b1->len : b2->len;

	int cmp = memcmp(b1->f,b2->f,l);
	if (cmp != 0)
		return cmp;

	if (b1->len < b2->len)
		return -1;
	if (b1->len > b2->len)
		return +1;

	u8 cmask = b1->mask & b2->mask;
	if ((b1->f[l] & cmask) < (b2->f[l] & cmask))
		return -1;
	if ((b1->f[l] & cmask) > (b2->f[l] & cmask))
		return +1;

	if (b1->mask < b2->mask)
		return -1;
	if (b1->mask > b2->mask)
		return +1;

	return 0;
}

static void filter_sort(void)
{
	size_t len = VEC_LENGTH(filters);
	if (len > 0)
		qsort(&VEC_BUF(filters,0),len,sizeof(struct binfilter),&filter_compare);
}

#endif // BINFILTER



static inline int filters_a_includes_b(size_t a,size_t b)
{
	const struct binfilter *fa = &VEC_BUF(filters,a);
	const struct binfilter *fb = &VEC_BUF(filters,b);

	if (fa->len > fb->len)
		return 0;
	size_t l = fa->len;

	int cmp = memcmp(fa->f,fb->f,l);
	if (cmp != 0)
		return 0;

	if (fa->len < fb->len)
		return 1;

	if (fa->mask > fb->mask)
		return 0;

	return fa->f[l] == (fb->f[l] & fa->mask);
}

static void filters_dedup(void)
{
	size_t last = ~(size_t)0; // index after last matching element
	size_t chk;               // element to compare against
	size_t st;                // start of area to destroy

	size_t len = VEC_LENGTH(filters);
	for (size_t i = 1;i < len;++i) {
		if (last != i) {
			if (filters_a_includes_b(i - 1,i)) {
				if (last != ~(size_t)0) {
					memmove(&VEC_BUF(filters,st),
						&VEC_BUF(filters,last),
						(i - last) * VEC_ELSIZE(filters));
					st += i - last;
				}
				else
					st = i;
				chk = i - 1;
				last = i + 1;
			}
		}
		else {
			if (filters_a_includes_b(chk,i))
				last = i + 1;
		}
	}
	if (last != ~(size_t)0) {
		memmove(&VEC_BUF(filters,st),
			&VEC_BUF(filters,last),
			(len - last) * VEC_ELSIZE(filters));
		st += len - last;
		VEC_SETLENGTH(filters,st);
	}
}

static void filters_clean(void)
{
	VEC_FREE(filters);
}

size_t filters_count(void)
{
	return VEC_LENGTH(filters);
}


static void filters_print(void)
{
	if (quietflag)
		return;
	size_t i,l;
	l = VEC_LENGTH(filters);
	if (l)
		fprintf(stderr,"filters:\n");

	for (i = 0;i < l;++i) {
#ifdef NEEDBINFILTER
		char buf0[256],buf1[256];
		u8 bufx[128];
#endif

		if (!verboseflag && i >= 20) {
			size_t notshown = l - i;
			fprintf(stderr,"[another " FSZ " %s not shown]\n",
				notshown,notshown == 1 ? "filter" : "filters");
			break;
		}

#ifdef BINFILTER
		size_t len = VEC_BUF(filters,i).len + 1;
		u8 mask = VEC_BUF(filters,i).mask;
		u8 *ifraw = VEC_BUF(filters,i).f;
#endif // BINFILTER
#ifdef NEEDBINFILTER
		base32_to(buf0,ifraw,len);
		memcpy(bufx,ifraw,len);
		bufx[len - 1] |= ~mask;
		base32_to(buf1,bufx,len);
		char *a = buf0,*b = buf1;
		while (*a && *a == *b)
			++a, ++b;
		*a = 0;
		fprintf(stderr,"\t%s\n",buf0);
#endif // NEEDBINFILTER
	}
	fprintf(stderr,"in total, " FSZ " %s\n",l,l == 1 ? "filter" : "filters");
}

void filters_add(const char *filter)
{
#ifdef NEEDBINFILTER
	struct binfilter bf;
	size_t ret;

	// skip regex start symbol. we do not support regex tho
	if (*filter == '^')
		++filter;

	memset(&bf,0,sizeof(bf));

	if (!base32_valid(filter,&ret)) {
		fprintf(stderr,"filter \"%s\" is not valid base32 string\n",filter);
		fprintf(stderr,"        ");
		while (ret--)
			fputc(' ',stderr);
		fprintf(stderr,"^\n");
		return;
	}

	ret = BASE32_FROM_LEN(ret);
	if (!ret)
		return;
	size_t maxsz = sizeof(bf.f);
	if (ret > maxsz) {
		fprintf(stderr,"filter \"%s\" is too long\n",filter);
		fprintf(stderr,"        ");
		maxsz = (maxsz * 8) / 5;
		while (maxsz--)
			fputc(' ',stderr);
		fprintf(stderr,"^\n");
		return;
	}
	base32_from(bf.f,&bf.mask,filter);
	bf.len = ret - 1;

# ifdef BINFILTER
	VEC_ADD(filters,bf);
# endif // BINFILTER
#endif // NEEDBINFILTER
}

static void filters_prepare(void)
{
	if (!quietflag)
		fprintf(stderr,"sorting filters...");
	filter_sort();
	if (wantdedup) {
		if (!quietflag)
			fprintf(stderr," removing duplicates...");
		filters_dedup();
	}
	if (!quietflag)
		fprintf(stderr," done.\n");
}

static bool loadfilterfile(const char *fname)
{
	char buf[128];
	FILE *f = fopen(fname,"r");
	if (!f) {
		fprintf(stderr,"failed to load filter file \"%s\": %s\n",fname,strerror(errno));
		return false;
	}
	while (fgets(buf,sizeof(buf),f)) {
		for (char *p = buf;*p;++p) {
			if (*p == '\n') {
				*p = 0;
				break;
			}
		}
		if (*buf && *buf != '#' && memcmp(buf,"//",2) != 0)
			filters_add(buf);
	}
	int fe = ferror(f);
	fclose(f);
	if (fe != 0) {
		fprintf(stderr,"failure while reading filter file \"%s\": %s\n",fname,strerror(fe));
		return false;
	}
	return true;
}
