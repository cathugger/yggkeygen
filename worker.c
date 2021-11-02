#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sodium/randombytes.h>
#include <sodium/utils.h>

#include <arpa/inet.h>

#include "types.h"
#include "likely.h"
#include "vec.h"
#include "base32.h"
#include "ed25519/ed25519.h"
#include "ioutil.h"
#include "common.h"
#include "output.h"

#include "worker.h"

#include "filters.h"

#ifndef _WIN32
#define FSZ "%zu"
#else
#define FSZ "%Iu"
#endif

pthread_mutex_t keysgenerated_mutex;
volatile size_t keysgenerated = 0;
volatile int endwork = 0;

int numwords = 1;
size_t numneedgenerate = 0;

// output directory
char *workdir = 0;
size_t workdirlen = 0;

void worker_init(void)
{
	ge_initeightpoint();
}

static void makeyggaddress(char *dst,size_t dstlen,u8 *counter,const u8 *pk)
{
	u8 ones = 0,bits = 0,done = 0,nbits = 0;
	u8 invpk[PUBLIC_LEN];
	u8 addr[16] = {0};
	// first byte is prefix, second is counter
	u8 *aptr = &addr[2];
	size_t alen = sizeof(addr) - 2;

	memcpy(invpk,pk,PUBLIC_LEN);
	for (size_t i; i < PUBLIC_LEN; i++)
		invpk[i] = ~invpk[i];

	for(int i = 0; i < 8 * PUBLIC_LEN; i++) {

		u8 bit = (invpk[i >> 3] & (0x80 >> (i & 7))) >> (7 - (i & 7));

		if (!done) {
			if (bit != 0)
				ones++;
			else
				done = 1;

			continue;
		}

		bits = (bits << 1) | bit;
		nbits++;
		if (nbits == 8) {
			nbits = 0;
			*aptr++ = bits;
			alen--;
			if (!alen)
				break;
		}
	}
	addr[0] = 0x02; // prefix
	addr[1] = ones; // counter
	*counter = ones;

	if (!inet_ntop(AF_INET6,addr,dst,dstlen))
		abort();
}

static void yggready(const u8 *secret,const u8 *public)
{
	if (endwork)
		return;

	if (numneedgenerate) {
		pthread_mutex_lock(&keysgenerated_mutex);
		if (keysgenerated >= numneedgenerate) {
			pthread_mutex_unlock(&keysgenerated_mutex);
			return;
		}
		++keysgenerated;
		if (keysgenerated == numneedgenerate)
			endwork = 1;
		pthread_mutex_unlock(&keysgenerated_mutex);
	}

	// disabled as this was never ever triggered as far as I'm aware
#if 1
	// Sanity check that the public key matches the private one.
	ge_p3 ALIGN(16) point;
	u8 testpk[PUBLIC_LEN];
	ge_scalarmult_base(&point,secret);
	ge_p3_tobytes(testpk,&point);
	if (memcmp(testpk,public,PUBLIC_LEN) != 0) {
		fprintf(stderr,"!!! secret key mismatch !!!\n");
		abort();
	}

#endif

	char addressbuf[38+1] = {0};
	u8 counter;
	makeyggaddress(addressbuf,sizeof(addressbuf),&counter,public);

	output_writekey(addressbuf,public,secret,counter);
}

#include "filters_inc.inc.h"
#include "filters_worker_i.inc.h"

#ifdef STATISTICS
#define ADDNUMSUCCESS ++st->numsuccess.v
#else
#define ADDNUMSUCCESS do ; while (0)
#endif


// in little-endian order, 32 bytes aka 256 bits
static void addsztoscalar32(u8 *dst,size_t v)
{
	int i;
	u32 c = 0;
	for (i = 0;i < 32;++i) {
		c += *dst + (v & 0xFF); *dst = c & 0xFF; c >>= 8;
		v >>= 8;
		++dst;
	}
}

#include "worker_fast.inc.h"


#if !defined(BATCHNUM)
	#define BATCHNUM 2048
#endif

size_t worker_batch_memuse(void)
{
	return (sizeof(ge_p3) + sizeof(fe) + sizeof(bytes32)) * BATCHNUM;
}

#include "worker_batch.inc.h"
