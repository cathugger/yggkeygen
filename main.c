#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <sodium/core.h>
#include <sodium/randombytes.h>
#include <sodium/utils.h>

#include "types.h"
#include "vec.h"
#include "base32.h"
#include "cpucount.h"
#include "ioutil.h"
#include "common.h"
#include "output.h"

#include "filters.h"

#include "worker.h"

#ifndef _WIN32
#define FSZ "%zu"
#else
#define FSZ "%Iu"
#endif

int quietflag = 0;
int verboseflag = 0;

static int wantdedup = 0;

pthread_mutex_t fout_mutex;
FILE *fout;

static void termhandler(int sig)
{
	switch (sig) {
	case SIGTERM:
	case SIGINT:
		endwork = 1;
		break;
	}
}

#ifdef STATISTICS
struct tstatstruct {
	u64 numcalc;
	u64 numsuccess;
	u64 numrestart;
	u32 oldnumcalc;
	u32 oldnumsuccess;
	u32 oldnumrestart;
} ;
VEC_STRUCT(tstatsvec,struct tstatstruct);
#endif

static void printhelp(FILE *out,const char *progname)
{
	fprintf(out,
		"Usage: %s filter [filter...] [options]\n"
		"       %s -f filterfile [options]\n"
		"Options:\n"
		"\t-h  - print help to stdout and quit\n"
		"\t-f  - specify filter file which contains filters separated by newlines\n"
		"\t-D  - deduplicate filters\n"
		"\t-q  - do not print diagnostic output to stderr\n"
		"\t-v  - print more diagnostic data\n"
		"\t-t numthreads  - specify number of threads to utilise (default - CPU core count or 1)\n"
		"\t-j numthreads  - same as -t\n"
		"\t-n numkeys  - specify number of keys (default - 0 - unlimited)\n"
		"\t-z  - use \"faster\" key generation method (later default)\n"
		"\t-B  - use batching key generation method (>10x faster than -z, current default)\n"
		"\t-s  - print statistics each 10 seconds\n"
		"\t-S t  - print statistics every specified ammount of seconds\n"
		"\t-T  - do not reset statistics counters when printing\n"
		,progname,progname);
	fflush(out);
}

static void e_additional(void)
{
	fprintf(stderr,"additional argument required\n");
	exit(1);
}

#ifndef STATISTICS
static void e_nostatistics(void)
{
	fprintf(stderr,"statistics support not compiled in\n");
	exit(1);
}
#endif

VEC_STRUCT(threadvec, pthread_t);

#include "filters_inc.inc.h"
#include "filters_main.inc.h"

enum worker_type {
	WT_FAST,
	WT_BATCH,
};

int main(int argc,char **argv)
{
	const char *outfile = 0;
	int outfileoverwrite = 0;
	const char *arg;
	int ignoreargs = 0;
	int numthreads = 0;
	enum worker_type wt = WT_BATCH;
	struct threadvec threads;
#ifdef STATISTICS
	struct statsvec stats;
	struct tstatsvec tstats;
	u64 reportdelay = 0;
	int realtimestats = 1;
#endif
	int tret;

	if (sodium_init() < 0) {
		fprintf(stderr,"sodium_init() failed\n");
		return 1;
	}
	worker_init();
	filters_init();

	setvbuf(stderr,0,_IONBF,0);
	fout = stdout;

	const char *progname = argv[0];
	/*
	if (argc <= 1) {
		printhelp(stderr,progname);
		exit(1);
	}
	*/
	argc--; argv++;

	while (argc--) {
		arg = *argv++;
		if (!ignoreargs && *arg == '-') {
			int numargit = 0;
		nextarg:
			++arg;
			++numargit;
			if (*arg == '-') {
				if (numargit > 1) {
					fprintf(stderr,"unrecognised argument: -\n");
					exit(1);
				}
				++arg;
				if (!*arg)
					ignoreargs = 1;
				else if (!strcmp(arg,"help") || !strcmp(arg,"usage")) {
					printhelp(stdout,progname);
					exit(0);
				}
				else {
					fprintf(stderr,"unrecognised argument: --%s\n",arg);
					exit(1);
				}
				numargit = 0;
			}
			else if (*arg == 0) {
				if (numargit == 1)
					ignoreargs = 1;
				continue;
			}
			else if (*arg == 'h') {
				printhelp(stdout,progname);
				exit(0);
			}
			else if (*arg == 'f') {
				if (argc--) {
					if (!loadfilterfile(*argv++))
						exit(1);
				}
				else
					e_additional();
			}
			else if (*arg == 'D') {
				wantdedup = 1;
			}
			else if (*arg == 'q')
				++quietflag;
			else if (*arg == 'x')
				fout = 0;
			else if (*arg == 'v')
				verboseflag = 1;
			else if (*arg == 'o') {
				outfileoverwrite = 0;
				if (argc--)
					outfile = *argv++;
				else
					e_additional();
			}
			else if (*arg == 'O') {
				outfileoverwrite = 1;
				if (argc--)
					outfile = *argv++;
				else
					e_additional();
			}
			else if (*arg == 't' || *arg == 'j') {
				if (argc--)
					numthreads = atoi(*argv++);
				else
					e_additional();
			}
			else if (*arg == 'n') {
				if (argc--)
					numneedgenerate = (size_t)atoll(*argv++);
				else
					e_additional();
			}
			else if (*arg == 'z')
				wt = WT_FAST;
			else if (*arg == 'B')
				wt = WT_BATCH;
			else if (*arg == 's') {
#ifdef STATISTICS
				reportdelay = 10000000;
#else
				e_nostatistics();
#endif
			}
			else if (*arg == 'S') {
#ifdef STATISTICS
				if (argc--)
					reportdelay = (u64)atoll(*argv++) * 1000000;
				else
					e_additional();
#else
				e_nostatistics();
#endif
			}
			else if (*arg == 'T') {
#ifdef STATISTICS
				realtimestats = 0;
#else
				e_nostatistics();
#endif
			}
			else {
				fprintf(stderr,"unrecognised argument: -%c\n",*arg);
				exit(1);
			}
			if (numargit)
				goto nextarg;
		}
		else
			filters_add(arg);
	}

	if (outfile) {
		fout = fopen(outfile,!outfileoverwrite ? "a" : "w");
		if (!fout) {
			perror("failed to open output file");
			exit(1);
		}
	}

	filters_prepare();

	filters_print();

/*
#ifdef STATISTICS
	if (!filters_count() && !reportdelay)
#else
	if (!filters_count())
#endif
		return 0;
*/

	pthread_mutex_init(&keysgenerated_mutex,0);
	pthread_mutex_init(&fout_mutex,0);

	if (numthreads <= 0) {
		numthreads = cpucount();
		if (numthreads <= 0)
			numthreads = 1;
	}
	if (!quietflag)
		fprintf(stderr,"using %d %s\n",
			numthreads,numthreads == 1 ? "thread" : "threads");

	signal(SIGTERM,termhandler);
	signal(SIGINT,termhandler);

	VEC_INIT(threads);
	VEC_ADDN(threads,numthreads);
#ifdef STATISTICS
	VEC_INIT(stats);
	VEC_ADDN(stats,numthreads);
	VEC_ZERO(stats);
	VEC_INIT(tstats);
	VEC_ADDN(tstats,numthreads);
	VEC_ZERO(tstats);
#endif

	pthread_attr_t tattr,*tattrp = &tattr;
	tret = pthread_attr_init(tattrp);
	if (tret) {
		perror("pthread_attr_init");
		tattrp = 0;
	}
	else {
		// 256KiB plus whatever batch stuff uses if in batch mode
		size_t ss = 256 << 10;
		if (wt == WT_BATCH)
			ss += worker_batch_memuse();
		// align to 64KiB
		ss = (ss + (64 << 10) - 1) & ~((64 << 10) - 1);
		//printf("stack size: " FSZ "\n",ss);
		tret = pthread_attr_setstacksize(tattrp,ss);
		if (tret)
			perror("pthread_attr_setstacksize");
	}

	for (size_t i = 0;i < VEC_LENGTH(threads);++i) {
		void *tp = 0;
#ifdef STATISTICS
		tp = &VEC_BUF(stats,i);
#endif
		tret = pthread_create(
			&VEC_BUF(threads,i),
			tattrp,
			wt == WT_BATCH
				? worker_batch
				: worker_fast,
			tp
		);
		if (tret) {
			fprintf(stderr,"error while making " FSZ "th thread: %s\n",i,strerror(tret));
			exit(1);
		}
	}

	if (tattrp) {
		tret = pthread_attr_destroy(tattrp);
		if (tret)
			perror("pthread_attr_destroy");
	}

#ifdef STATISTICS
	struct timespec nowtime;
	u64 istarttime,inowtime,ireporttime = 0,elapsedoffset = 0;
	if (clock_gettime(CLOCK_MONOTONIC,&nowtime) < 0) {
		perror("failed to get time");
		exit(1);
	}
	istarttime = (1000000 * (u64)nowtime.tv_sec) + ((u64)nowtime.tv_nsec / 1000);
#endif
	struct timespec ts;
	memset(&ts,0,sizeof(ts));
	ts.tv_nsec = 100000000;
	while (!endwork) {
		if (numneedgenerate && keysgenerated >= numneedgenerate) {
			endwork = 1;
			break;
		}
		nanosleep(&ts,0);

#ifdef STATISTICS
		clock_gettime(CLOCK_MONOTONIC,&nowtime);
		inowtime = (1000000 * (u64)nowtime.tv_sec) + ((u64)nowtime.tv_nsec / 1000);
		u64 sumcalc = 0,sumsuccess = 0,sumrestart = 0;
		for (int i = 0;i < numthreads;++i) {
			u32 newt,tdiff;
			// numcalc
			newt = VEC_BUF(stats,i).numcalc.v;
			tdiff = newt - VEC_BUF(tstats,i).oldnumcalc;
			VEC_BUF(tstats,i).oldnumcalc = newt;
			VEC_BUF(tstats,i).numcalc += (u64)tdiff;
			sumcalc += VEC_BUF(tstats,i).numcalc;
			// numsuccess
			newt = VEC_BUF(stats,i).numsuccess.v;
			tdiff = newt - VEC_BUF(tstats,i).oldnumsuccess;
			VEC_BUF(tstats,i).oldnumsuccess = newt;
			VEC_BUF(tstats,i).numsuccess += (u64)tdiff;
			sumsuccess += VEC_BUF(tstats,i).numsuccess;
			// numrestart
			newt = VEC_BUF(stats,i).numrestart.v;
			tdiff = newt - VEC_BUF(tstats,i).oldnumrestart;
			VEC_BUF(tstats,i).oldnumrestart = newt;
			VEC_BUF(tstats,i).numrestart += (u64)tdiff;
			sumrestart += VEC_BUF(tstats,i).numrestart;
		}
		if (reportdelay && (!ireporttime || inowtime - ireporttime >= reportdelay)) {
			if (ireporttime)
				ireporttime += reportdelay;
			else
				ireporttime = inowtime;
			if (!ireporttime)
				ireporttime = 1;

			double calcpersec = (1000000.0 * sumcalc) / (inowtime - istarttime);
			double succpersec = (1000000.0 * sumsuccess) / (inowtime - istarttime);
			double restpersec = (1000000.0 * sumrestart) / (inowtime - istarttime);
			fprintf(stderr,">calc/sec:%8lf, succ/sec:%8lf, rest/sec:%8lf, elapsed:%5.6lfsec\n",
				calcpersec,succpersec,restpersec,
				(inowtime - istarttime + elapsedoffset) / 1000000.0);

			if (realtimestats) {
				for (int i = 0;i < numthreads;++i) {
					VEC_BUF(tstats,i).numcalc = 0;
					VEC_BUF(tstats,i).numsuccess = 0;
					VEC_BUF(tstats,i).numrestart = 0;
				}
				elapsedoffset += inowtime - istarttime;
				istarttime = inowtime;
			}
		}
		if (sumcalc > U64_MAX / 2) {
			for (int i = 0;i < numthreads;++i) {
				VEC_BUF(tstats,i).numcalc /= 2;
				VEC_BUF(tstats,i).numsuccess /= 2;
				VEC_BUF(tstats,i).numrestart /= 2;
			}
			u64 timediff = (inowtime - istarttime + 1) / 2;
			elapsedoffset += timediff;
			istarttime += timediff;
		}
#endif
	}

	if (!quietflag)
		fprintf(stderr,"waiting for threads to finish...");
	for (size_t i = 0;i < VEC_LENGTH(threads);++i)
		pthread_join(VEC_BUF(threads,i),0);
	if (!quietflag)
		fprintf(stderr," done.\n");

	pthread_mutex_destroy(&fout_mutex);
	pthread_mutex_destroy(&keysgenerated_mutex);

/* done: */
	filters_clean();

	if (outfile)
		fclose(fout);

	return 0;
}
