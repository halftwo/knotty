/* $Id: ratcliff.c,v 1.5 2009/02/09 04:46:59 jiagui Exp $ */
#include "ratcliff.h"

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: ratcliff.c,v 1.5 2009/02/09 04:46:59 jiagui Exp $";
#endif


static int RatcliffObershelp(const char *st1, const char *end1, const char *st2, const char *end2)
{
	register const char *a1, *a2;
	const char *b1, *b2; 
	const char *s1 = st1, *s2 = st2;	/* initializations are just to pacify GCC */
	int max, i;

	if (end1 == st1 + 1 && end2 == st2 + 1)
		return 0;
		
	max = 0;
	b1 = end1; b2 = end2;
	
	for (a1 = st1; a1 < b1; a1++)
	{
		for (a2 = st2; a2 < b2; a2++)
		{
			if (*a1 == *a2)
			{
				/* determine length of common substring */
				for (i = 1; a1[i] && (a1[i] == a2[i]); i++)
					continue;

				if (i > max)
				{
					max = i; 
					s1 = a1;
					s2 = a2;
					b1 = end1 - max;
					b2 = end2 - max;
				}
			}
		}
	}

	if (!max)
		return 0;

	if (s1 + max < end1 && s2 + max < end2)
		max += RatcliffObershelp(s1 + max, end1, s2 + max, end2);	/* rhs */
	if (st1 < s1 && st2 < s2)
		max += RatcliffObershelp(st1, s1, st2, s2);			/* lhs */

	return max;
}

float ratcliff_similarity(const char *s1, size_t len1, const char *s2, size_t len2)
{
	if (len1 <= 0 || len2 <= 0)
		return 0.0;

	if (len1 == 1 && len2 == 1 && *s1 == *s2)
		return 1.0;
			
	return 2.0 * RatcliffObershelp(s1, s1 + len1, s2, s2 + len2) / (len1 + len2);
}


#ifdef TEST_RATCLIFF

#include <stdio.h>

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		fprintf(stderr, "usage: %s <str1> <str2>\n", argv[0]);
		exit(1);
	}

	char *s1 = argv[1];
	char *s2 = argv[2];
	float similarity;
	int i;

	for (i = 0; i < 1024 * 1024; ++i)
		similarity = ratcliff_similarity(s1, strlen(s1), s2, strlen(s2));

	printf("similarity=%g\n", similarity);
	return 0;
}

#endif
