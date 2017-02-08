#include "golay24.h"
#include <stdlib.h>
#include <stdint.h>

/*
 * This Golay Code implementation is from an article by Hank Wallace.
 * See:
 * 	http://www.aqdi.com/golay.htm
 */

/* generator polynomial 0x0AE3U, or use the bitwise reverse 0x0C75
 */
#define GEN_POLY       	0x000AE3U
#define DATA_MASK 	0x000FFFU
#define CODE_MASK    	0x7FFFFFU	/* Not include the parity bit */
#define PARITY_MASK 	0x800000U


static inline int parity(uint32_t x) 
{
	x ^= x >> 16;
	x ^= x >> 8;
	x ^= x >> 4;
	x ^= x >> 2;
	x ^= x >> 1;
	return (x & 1);
}

static inline int weight(uint32_t x)
{
	int n = 0;		/* count of bits */
	while (x)
	{
		x &= x - 1; 	/* clear the least significant bit set */
		++n;
	}
	return n;
}

/* This function calculates and returns the syndrome of a [23,12] Golay codeword. */
static uint32_t syndrome(uint32_t codeword) 
{
	int i = 12;

	codeword &= CODE_MASK;
	while (i-- > 0)
	{
		if (codeword & 0x01)
			codeword ^= GEN_POLY;
		codeword >>= 1;
	}

	return (codeword << 12);
}

/* This function corrects Golay [23,12] codeword cw, returning the 
 * corrected codeword. This function will produce the corrected codeword 
 * for three or fewer errors. It will produce some other valid Golay 
 * codeword for four or more errors, possibly not the intended 
 * one. *errs is set to the number of bit errors corrected. 
 */ 
static uint32_t correct(uint32_t codeword, int *errs)
{
	int w;    		/* current syndrome limit weight, 2 or 3 */
	uint32_t saved_codeword = codeword;
	int j;

	*errs = 0;
	w = 3;                /* initial syndrome weight threshold */

	/* j == -1: no trial bit flipping on first pass */
	for (j = -1; j < 23; ++j)
	{
		int i;
		uint32_t s; 		/* calculated syndrome */
		if (j != -1)
		{
			codeword = saved_codeword ^ (1 << j);
			w = 2;
		}

		s = syndrome(codeword);
		if (!s) 
			return codeword; /* return corrected codeword */

		/* errors exist */
		for (i = 0; i < 23; ++i) 
		{
			/* check syndrome of each cyclic shift */
			(*errs) = weight(s);
			if ((*errs) <= w)
			{
				/* syndrome matches error pattern */
				codeword ^= s;              /* remove errors */
				codeword = ((codeword >> i | codeword << (23 - i)) & CODE_MASK);  /* unrotate (right) data */

				if (j != -1) /* count toggled bit (per Steve Duncan) */
					++(*errs);

				return codeword;
			}
			codeword = ((codeword << 1 | codeword >> 22) & CODE_MASK);   /* rotate left to next pattern */
			s = syndrome(codeword);         /* calc new syndrome */
		}
	}

	return saved_codeword;
}


uint32_t golay24_data2code(uint32_t data)
{
	uint32_t codeword;

	data &= DATA_MASK;
	codeword = syndrome(data) | data;
	if (parity(codeword))
		codeword |= PARITY_MASK;
	return codeword;
}


int golay24_code2data(uint32_t codeword, uint32_t *data)
{
	int errs;
	uint32_t parity_bit = codeword & PARITY_MASK;

	codeword = correct(codeword & CODE_MASK, &errs);

	if (parity(codeword | parity_bit))
		return -1;

	*data = codeword & DATA_MASK;
	return errs;
}


#ifdef TEST_GOLAY24

#include <time.h>
#include <stdio.h>

int main(int argc, char **argv) 
{
	int i, k;

	srandom(time(NULL));
	for (i = 0; i < 1024*100; ++i)
	{
		uint32_t decoded;
		uint32_t data = random() % DATA_MASK;
		uint32_t codeword = golay24_data2code(data);
		uint32_t noise_codeword = codeword;

		for (k = 0; k < 3; ++k)
		{
			noise_codeword ^= (1 << (random() % 23));
		}

		int rc = golay24_code2data(noise_codeword, &decoded);
		if (rc < 0 || data != decoded)
		{
			fprintf(stderr, "ERROR: data=%#06x code=%#06x noise=%#06x decoded=%#06x\n",
					data, codeword, noise_codeword, decoded);
			exit(1);
		}
	}

	fprintf(stderr, "success\n");
	return 0;
}

#endif

