
void *worker_batch(void *task)
{
	u8 pk[PUBLIC_LEN];
	u8 sk[SECRET_LEN];
	u8 seed[SEED_LEN];
	ge_p3 ALIGN(16) ge_public;

	// state to keep batch data
	ge_p3   ALIGN(16) ge_batch [BATCHNUM];
	fe      ALIGN(16) tmp_batch[BATCHNUM];
	bytes32 ALIGN(16) pk_batch [BATCHNUM];

	size_t counter;
	size_t i;

	(void) i;

#ifdef STATISTICS
	struct statstruct *st = (struct statstruct *)task;
#else
	(void) task;
#endif

	PREFILTER

initseed:

#ifdef STATISTICS
	++st->numrestart.v;
#endif

	randombytes(seed,sizeof(seed));

	ed25519_seckey_expand(sk,seed);

	ge_scalarmult_base(&ge_public,sk);

	for (counter = 0;counter < SIZE_MAX-(8*BATCHNUM);counter += 8*BATCHNUM) {
		ge_p1p1 ALIGN(16) sum;

		if (unlikely(endwork))
			goto end;

		for (size_t b = 0;b < BATCHNUM;++b) {
			ge_batch[b] = ge_public;
			ge_add(&sum,&ge_public,&ge_eightpoint);
			ge_p1p1_to_p3(&ge_public,&sum);
		}
		// NOTE: leaves unfinished one bit at the very end
		ge_p3_batchtobytes_destructive_1(pk_batch,ge_batch,tmp_batch,BATCHNUM);

#ifdef STATISTICS
		st->numcalc.v += BATCHNUM;
#endif

		for (size_t b = 0;b < BATCHNUM;++b) {
			DOFILTER(i,pk_batch[b],{
				// found!
				// finish it up
				ge_p3_batchtobytes_destructive_finish(pk_batch[b],&ge_batch[b]);
				// copy public key
				memcpy(pk,pk_batch[b],PUBLIC_LEN);
				// update secret key with counter
				addsztoscalar32(sk,counter + (b * 8));
				// sanity check
				if ((sk[0] & 248) != sk[0] || ((sk[31] & 63) | 64) != sk[31])
					goto initseed;

				FILTERSUCCESS(pk)

				ADDNUMSUCCESS;

				yggready(sk,pk);

				// don't reuse same seed
				goto initseed;
			});
		/* next: */
			;
		}
	}
	goto initseed;

end:

	POSTFILTER

	sodium_memzero(sk,sizeof(sk));
	sodium_memzero(seed,sizeof(seed));

	return 0;
}
