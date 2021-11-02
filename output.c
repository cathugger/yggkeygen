#define _POSIX_C_SOURCE 200112L

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "types.h"
#include "output.h"
#include "ioutil.h"
#include "base16.h"
#include "common.h"

#define LINEFEED_LEN       (sizeof(char))
#define NULLTERM_LEN       (sizeof(char))

static const char keys_field_generated[] = "---";
static const char keys_field_address[]   = "// Address: ";
static const char keys_field_secretkey[] = "PrivateKey: ";
static const char keys_field_publickey[] = "PublicKey:  ";
static const char keys_field_compress[]  = " / Compression: ";


#define KEYS_FIELD_GENERATED_LEN (sizeof(keys_field_generated) - NULLTERM_LEN)
#define KEYS_FIELD_ADDRESS_LEN   (sizeof(keys_field_address)   - NULLTERM_LEN)
#define KEYS_FIELD_SECRETKEY_LEN (sizeof(keys_field_secretkey) - NULLTERM_LEN)
#define KEYS_FIELD_PUBLICKEY_LEN (sizeof(keys_field_publickey) - NULLTERM_LEN)
#define KEYS_FIELD_COMPRESS_LEN  (sizeof(keys_field_compress)  - NULLTERM_LEN)

#define B16_PUBKEY_LEN (BASE16_TO_LEN(PUBLIC_LEN))
#define B16_SECKEY_LEN (BASE16_TO_LEN(SECRET_LEN))
#define ADDRESS_LEN  38 // max address len
#define COMPRESS_LEN 3  // max 3 places

#define KEYS_LEN ( \
	KEYS_FIELD_GENERATED_LEN                  + LINEFEED_LEN + \
	KEYS_FIELD_ADDRESS_LEN   + ADDRESS_LEN    + \
		KEYS_FIELD_COMPRESS_LEN + COMPRESS_LEN + LINEFEED_LEN + \
	KEYS_FIELD_SECRETKEY_LEN + B16_SECKEY_LEN + LINEFEED_LEN + \
	KEYS_FIELD_PUBLICKEY_LEN + B16_PUBKEY_LEN + LINEFEED_LEN \
)


#define BUF_APPEND(buf,offset,src,srclen) \
do { \
	memcpy(&buf[offset],(src),(srclen)); \
	offset += (srclen); \
} while (0)
#define BUF_APPEND_CSTR(buf,offset,src) BUF_APPEND(buf,offset,src,strlen(src))
#define BUF_APPEND_CHAR(buf,offset,c) buf[offset++] = (c)


void output_writekey(
	const char *address,const u8 *publickey,const u8 *secretkey,u8 compress)
{
	char keysbuf[KEYS_LEN];
	char pubkeybuf[B16_PUBKEY_LEN + NULLTERM_LEN];
	char seckeybuf[B16_SECKEY_LEN + NULLTERM_LEN];
	char compressbuf[COMPRESS_LEN + NULLTERM_LEN];
	size_t offset = 0;


	BUF_APPEND(keysbuf,offset,keys_field_address,KEYS_FIELD_ADDRESS_LEN);
	BUF_APPEND(keysbuf,offset,address,ADDRESS_LEN);

	BUF_APPEND(keysbuf,offset,keys_field_compress,KEYS_FIELD_COMPRESS_LEN);
	snprintf(compressbuf,sizeof(compressbuf),"%d",(int)compress);
	BUF_APPEND(keysbuf,offset,compressbuf,strlen(compressbuf));

	BUF_APPEND_CHAR(keysbuf,offset,'\n');


	BUF_APPEND(keysbuf,offset,keys_field_secretkey,KEYS_FIELD_SECRETKEY_LEN);

	base16_to(seckeybuf,secretkey,SECRET_LEN);

	BUF_APPEND_CSTR(keysbuf,offset,seckeybuf);
	BUF_APPEND_CHAR(keysbuf,offset,'\n');


	BUF_APPEND(keysbuf,offset,keys_field_publickey,KEYS_FIELD_PUBLICKEY_LEN);

	base16_to(pubkeybuf,publickey,PUBLIC_LEN);

	BUF_APPEND_CSTR(keysbuf,offset,pubkeybuf);
	BUF_APPEND_CHAR(keysbuf,offset,'\n');


	BUF_APPEND(keysbuf,offset,keys_field_generated,KEYS_FIELD_GENERATED_LEN);
	BUF_APPEND_CHAR(keysbuf,offset,'\n');


	assert(offset <= KEYS_LEN);


	pthread_mutex_lock(&fout_mutex);

	fwrite(keysbuf,offset,1,fout);
	fflush(fout);

	if (!quietflag && fout != stdout) {
		fwrite(keysbuf,offset,1,stdout);
		fflush(stdout);
	}

	pthread_mutex_unlock(&fout_mutex);
}

#undef BUF_APPEND_CHAR
#undef BUF_APPEND_CSTR
#undef BUF_APPEND
