
extern pthread_mutex_t keysgenerated_mutex;
extern volatile size_t keysgenerated;
extern volatile int endwork;

extern size_t numneedgenerate;

// statistics, if enabled
#ifdef STATISTICS
struct statstruct {
	union {
		u32 v;
		size_t align;
	} numcalc;
	union {
		u32 v;
		size_t align;
	} numsuccess;
	union {
		u32 v;
		size_t align;
	} numrestart;
} ;
VEC_STRUCT(statsvec,struct statstruct);
#endif

extern void worker_init(void);

extern size_t worker_batch_memuse(void);

extern void *worker_fast(void *task);
extern void *worker_batch(void *task);
