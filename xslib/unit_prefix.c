#include "unit_prefix.h"
#include <string.h>

static const char *_multiplier = "kKMGTPEZY";
static const char *_divider =     "munpfazy";


intmax_t cstr_unit_multiplier(const char *str, char **end/*NULL*/)
{
	intmax_t r = 1;
	char *s = (char *)str;

	if (*s)
	{
		char *p = strchr(_multiplier, *s);
		if (p)
		{
			int n = p > _multiplier ? p - _multiplier : 1;
			intmax_t x = 1000;

			++s;
			if (*s == 'i')
			{
				++s;
				x = 1024;
			}

			switch (n)
			{
			case 8: r *= x;
			case 7: r *= x;
			case 6: r *= x;
			case 5: r *= x;
			case 4: r *= x;
			case 3: r *= x;
			case 2: r *= x;
			case 1: r *= x;
			}
		}
	}

	if (end)
	{
		*end = s;
	}

	return r;
}

intmax_t xstr_unit_multiplier(const xstr_t *xs, xstr_t *end/*NULL*/)
{
	intmax_t r = 1;
	int k = 0;

	if (xs->len)
	{
		char c = xs->data[0];
		char *p = strchr(_multiplier, c);

		if (p)
		{
			int n = p > _multiplier ? p - _multiplier : 1;
			intmax_t x = 1000;

			++k;
			if (xs->len > k && xs->data[k] == 'i')
			{
				++k;
				x = 1024;
			}

			switch (n)
			{
			case 8: r *= x;
			case 7: r *= x;
			case 6: r *= x;
			case 5: r *= x;
			case 4: r *= x;
			case 3: r *= x;
			case 2: r *= x;
			case 1: r *= x;
			}
		}
	}

	if (end)
	{
		end->data = xs->data + k;
		end->len = xs->len - k;
	}

	return r;
}


intmax_t cstr_unit_divider(const char *str, char **end/*NULL*/)
{
	intmax_t r = 1;
	char *s = (char *)str;

	if (*s)
	{
		char *p = strchr(_divider, *s);
		if (p)
		{
			int n = p - _divider + 1;
			intmax_t x = 1000;

			++s;
			switch (n)
			{
			case 8: r *= x;
			case 7: r *= x;
			case 6: r *= x;
			case 5: r *= x;
			case 4: r *= x;
			case 3: r *= x;
			case 2: r *= x;
			case 1: r *= x;
			}
		}
	}

	if (end)
	{
		*end = s;
	}

	return r;
}

intmax_t xstr_unit_divider(const xstr_t *xs, xstr_t *end/*NULL*/)
{
	intmax_t r = 1;
	int k = 0;

	if (xs->len)
	{
		char c = xs->data[0];
		char *p = strchr(_divider, c);

		if (p)
		{
			int n = p - _divider + 1;
			intmax_t x = 1000;

			++k;
			switch (n)
			{
			case 8: r *= x;
			case 7: r *= x;
			case 6: r *= x;
			case 5: r *= x;
			case 4: r *= x;
			case 3: r *= x;
			case 2: r *= x;
			case 1: r *= x;
			}
		}
	}

	if (end)
	{
		end->data = xs->data + k;
		end->len = xs->len - k;
	}

	return r;
}

