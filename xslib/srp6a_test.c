#include "mp.h"
#include "hex.h"
#include "sha1.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: srp6a_test.c,v 1.7 2015/05/21 08:50:02 gremlin Exp $";
#endif

#define ND	MP_NDIGITS(1024)
#define BUFSIZE	MP_BUFSIZE(1024)


/**
  The routines comply with RFC 5054 (SRP for TLS), with the following exceptions:
  	1) The computation of the password key 'x' is modified to omit the user identity 'I' in order to allow for server-side user identity renaming as well as authentication with multiple alternate identities.
  	2) The evidence messages 'M1' and 'M2' are computed according to Tom Wu's paper "SRP-6: Improvements and refinements to the Secure Remote Password protocol", table 5, from 2002. 
 **/

/**
   The following group parameters are taken from
        http://srp.stanford.edu/demo/demo.html
 
   bits   g    N
   ----  ---  -----------------------------------------------------------------
   512  : 2 : d4c7f8a2b32c11b8fba9581ec4ba4f1b04215642ef7355e37c0fc0443ef756ea
              2c6b8eeb755a1c723027663caa265ef785b8ff6a9b35227a52d86633dbdfca43
   640  : 2 : c94d67eb5b1a2346e8ab422fc6a0edaeda8c7f894c9eeec42f9ed250fd7f0046
              e5af2cf73d6b2fa26bb08033da4de322e144e7a8e9b12a0e4637f6371f34a207
              1c4b3836cbeeab15034460faa7adf483
   768  : 2 : b344c7c4f8c495031bb4e04ff8f84ee95008163940b9558276744d91f7cc9f40
              2653be7147f00f576b93754bcddf71b636f2099e6fff90e79575f3d0de694aff
              737d9be9713cef8d837ada6380b1093e94b6a529a8c6c2be33e0867c60c3262b
   1024 : 2 : eeaf0ab9adb38dd69c33f80afa8fc5e86072618775ff3c0b9ea2314c9c256576
              d674df7496ea81d3383b4813d692c6e0e0d5d8e250b98be48e495c1d6089dad1
              5dc7d7b46154d6b6ce8ef4ad69b15d4982559b297bcf1885c529f566660e57ec
              68edbc3c05726cc02fd4cbf4976eaa9afd5138fe8376435b9fc61d2fc0eb06e3

   And from RFC5054
        http://www.ietf.org/rfc/rfc5054.txt

   1536 : 2 : 9DEF3CAF B939277A B1F12A86 17A47BBB DBA51DF4 99AC4C80 BEEEA961
              4B19CC4D 5F4F5F55 6E27CBDE 51C6A94B E4607A29 1558903B A0D0F843
              80B655BB 9A22E8DC DF028A7C EC67F0D0 8134B1C8 B9798914 9B609E0B
              E3BAB63D 47548381 DBC5B1FC 764E3F4B 53DD9DA1 158BFD3E 2B9C8CF5
              6EDF0195 39349627 DB2FD53D 24B7C486 65772E43 7D6C7F8C E442734A
              F7CCB7AE 837C264A E3A9BEB8 7F8A2FE9 B8B5292E 5A021FFF 5E91479E
              8CE7A28C 2442C6F3 15180F93 499A234D CF76E3FE D135F9BB

   2048 : 2 : AC6BDB41 324A9A9B F166DE5E 1389582F AF72B665 1987EE07 FC319294
              3DB56050 A37329CB B4A099ED 8193E075 7767A13D D52312AB 4B03310D
              CD7F48A9 DA04FD50 E8083969 EDB767B0 CF609517 9A163AB3 661A05FB
              D5FAAAE8 2918A996 2F0B93B8 55F97993 EC975EEA A80D740A DBF4FF74
              7359D041 D5C33EA7 1D281E44 6B14773B CA97B43A 23FB8016 76BD207A
              436C6481 F1D2B907 8717461A 5B9D32E6 88F87748 544523B5 24B0D57D
              5EA77A27 75D2ECFA 032CFBDB F52FB378 61602790 04E57AE6 AF874E73
              03CE5329 9CCC041C 7BC308D8 2A5698F3 A8D0C382 71AE35F8 E9DBFBB6
              94B5C803 D89F7AE4 35DE236D 525F5475 9B65E372 FCD68EF2 0FA7111F
              9E4AFF73
 **/

/*
  This program read lines from stdin which consist of
	a b s p
  And write lines to stdout that is
	M2

  The bits of private keys (a and b) should be no more longer than bits(N)/2.
 */

int main()
{
	/* All arithmetic is done modulo N.
	 */
	const char *N_str = "EEAF0AB9 ADB38DD6 9C33F80A FA8FC5E8 60726187 75FF3C0B 9EA2314C \
		9C256576 D674DF74 96EA81D3 383B4813 D692C6E0 E0D5D8E2 50B98BE4 \
		8E495C1D 6089DAD1 5DC7D7B4 6154D6B6 CE8EF4AD 69B15D49 82559B29 \
		7BCF1885 C529F566 660E57EC 68EDBC3C 05726CC0 2FD4CBF4 976EAA9A \
		FD5138FE 8376435B 9FC61D2F C0EB06E3";
	int generator = 2;
	const char *k_str = "7556AA04 5AEF2CDD 07ABAF0F 665C3E81 8913186F";
	MP_DIGIT N[ND], g[ND], k[ND], k1[ND];

	unsigned char buf[BUFSIZE];
	unsigned char sha1[20];
	char sha1hex[41];
	sha1_context sha1ctx;
	char *line = NULL;
	size_t size = 0;
	ssize_t len;
	ostk_t *ostk = ostk_create(4096*2);

	mp_from_digit(g, ND, generator);
	mp_from_hex(N, ND, N_str, -1);

	/* compute: k = SHA1(N | PAD(g))
	 */
	sha1_start(&sha1ctx);
	mp_to_padbuf(N, ND, buf, sizeof(buf));
	sha1_update(&sha1ctx, buf, sizeof(buf));
	mp_to_padbuf(g, ND, buf, sizeof(buf));
	sha1_update(&sha1ctx, buf, sizeof(buf));
	sha1_finish(&sha1ctx, sha1);
	mp_from_buf(k, ND, sha1, sizeof(sha1));

	mp_from_hex(k1, ND, k_str, -1);
	assert(mp_equal(k, k1, ND));

	while ((len = getline(&line, &size, stdin)) > 0)
	{
		unsigned char M1[20], M2[20], K[20];
		MP_DIGIT a[ND], b[ND], x[ND], v[ND];
		MP_DIGIT A[ND], B[ND], u[ND];
		MP_DIGIT S_user[ND], S_host[ND];
		MP_DIGIT TT[2*ND], T1[ND], T2[ND]; /* temporary */

		/* INPUT from stdin: a   b   s   p
		          in format: hex hex raw raw
		 */
		xstr_t a_xs, b_xs, s_xs, p_xs;
		xstr_t xs = XSTR_INIT((unsigned char *)line, len);
		xstr_delimit_in_cstr(&xs, " \t\r\n", &a_xs);
		xstr_delimit_in_cstr(&xs, " \t\r\n", &b_xs);
		xstr_delimit_in_cstr(&xs, " \t\r\n", &s_xs);
		xstr_delimit_in_cstr(&xs, " \t\r\n", &p_xs);
		if (p_xs.len == 0)
			continue;

		mp_from_hex(a, ND, (char *)a_xs.data, a_xs.len);
		mp_from_hex(b, ND, (char *)b_xs.data, b_xs.len);

		/* In RFC5054: x = SHA1(s | SHA1(I | ":" | p))
 		 * Here, we omit `I`
		 * compute: x = SHA1(s | SHA1(p))
		 */
		sha1_checksum(sha1, p_xs.data, p_xs.len); /* SHA1(p) */
		sha1_start(&sha1ctx);
		sha1_update(&sha1ctx, s_xs.data, s_xs.len);
		sha1_update(&sha1ctx, sha1, sizeof(sha1));
		sha1_finish(&sha1ctx, sha1);
		mp_from_buf(x, ND, sha1, sizeof(sha1));

		/* compute: v = g^x % N
		 */
		mp_modexp(v, g, x, N, ND, ostk);

		/* compute: A = g^a % N
		 */
		mp_modexp(A, g, a, N, ND, ostk);

		/* compute: B = k*v + g^b % N
		 */
		mp_modexp(B, g, b, N, ND, ostk);
		mp_modmul(T1, k, v, N, ND, ostk);
		mp_modadd(B, B, T1, N, ND, ostk);

		/* compute: u = SHA1(PAD(A) | PAD(B))
		 */
		sha1_start(&sha1ctx);
		mp_to_padbuf(A, ND, buf, sizeof(buf));
		sha1_update(&sha1ctx, buf, sizeof(buf));
		mp_to_padbuf(B, ND, buf, sizeof(buf));
		sha1_update(&sha1ctx, buf, sizeof(buf));
		sha1_finish(&sha1ctx, sha1);
		mp_from_buf(u, ND, sha1, sizeof(sha1));

		/* compute: S_user = (B - (k * g^x)) ^ (a + (u * x)) % N
		 */
		mp_modexp(T1, g, x, N, ND, ostk);
		mp_modmul(T1, k, T1, N, ND, ostk);
		mp_modsub(T2, B, T1, N, ND, ostk);

		/* NB: Because u and x have less than ND/2 digits, (u * x)
		       has no more than ND digits.
		       TT has 2*ND digits, but the higher ND digits are all zeros.
		 */
		mp_mul(TT, u, x, ND);

		if (mp_add(T1, TT, a, ND))
		{
			/* T ^ (P + Q) == (T ^ P) * (T ^ Q)
			 */
			MP_DIGIT T3[ND], T4[ND];

			mp_add_digit(T1, T1, 1, ND);
			mp_modexp(T3, T2, T1, N, ND, ostk);

			mp_zero(T1, ND);
			mp_sub_digit(T1, T1, 1, ND);
			mp_modexp(T4, T2, T1, N, ND, ostk);

			mp_modmul(S_user, T3, T4, N, ND, ostk);
		}
		else
		{
			mp_modexp(S_user, T2, T1, N, ND, ostk);
		}

		/* compute: S_host = (A * v^u) ^ b % N
		 */
		mp_modexp(T1, v, u, N, ND, ostk);
		mp_modmul(T1, T1, A, N, ND, ostk);
		mp_modexp(S_host, T1, b, N, ND, ostk);

		assert(mp_equal(S_user, S_host, ND));

		/* compute: M1 = SHA1(PAD(A) | PAD(B) | PAD(S))
		 */
		sha1_start(&sha1ctx);
		mp_to_padbuf(A, ND, buf, sizeof(buf));
		sha1_update(&sha1ctx, buf, sizeof(buf));
		mp_to_padbuf(B, ND, buf, sizeof(buf));
		sha1_update(&sha1ctx, buf, sizeof(buf));
		mp_to_padbuf(S_user, ND, buf, sizeof(buf));
		sha1_update(&sha1ctx, buf, sizeof(buf));
		sha1_finish(&sha1ctx, M1);

		/* compute: M2 = SHA1(PAD(A) | M1 | PAD(S))
		 */
		sha1_start(&sha1ctx);
		mp_to_padbuf(A, ND, buf, sizeof(buf));
		sha1_update(&sha1ctx, buf, sizeof(buf));
		sha1_update(&sha1ctx, M1, sizeof(M1));
		mp_to_padbuf(S_host, ND, buf, sizeof(buf));
		sha1_update(&sha1ctx, buf, sizeof(buf));
		sha1_finish(&sha1ctx, M2);

		/* compute: K = SHA1(PAD(S))
		 */
		mp_to_padbuf(S_user, ND, buf, sizeof(buf));
		sha1_checksum(K, buf, sizeof(buf));

		/* OUTPUT to stdout in hex format: M2
		 */
		hexlify(sha1hex, M2, sizeof(M2));
		printf("%s\n", sha1hex);
	}
	free(line);
	ostk_destroy(ostk);

	return 0;
}

