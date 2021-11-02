
void *worker_fast(void *task)
{
	u8 pk[PUBLIC_LEN];
	u8 sk[SECRET_LEN];
	u8 seed[SEED_LEN];
	ge_p3 ALIGN(16) ge_public;

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
	ge_p3_tobytes(pk,&ge_public);

	for (counter = 0;counter < SIZE_MAX-8;counter += 8) {
		ge_p1p1 ALIGN(16) sum;

		if (unlikely(endwork))
			goto end;

		DOFILTER(i,pk,{
			// found!
			// update secret key with counter
			addsztoscalar32(sk,counter);
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
		ge_add(&sum,&ge_public,&ge_eightpoint);
		ge_p1p1_to_p3(&ge_public,&sum);
		ge_p3_tobytes(pk,&ge_public);
#ifdef STATISTICS
		++st->numcalc.v;
#endif
	}
	goto initseed;

end:

	POSTFILTER

	sodium_memzero(sk,sizeof(sk));
	sodium_memzero(seed,sizeof(seed));

	return 0;
}
