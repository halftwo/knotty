#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include "setting.h"
#include "ostk.h"
#include "rbtree.h"
#include "binary_prefix.h"
#include <ctype.h>
#include <stdio.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: setting.c,v 1.2 2015/07/14 09:42:44 gremlin Exp $";
#endif

struct setting_t
{
	ostk_t *ostk;
	rbtree_t tree;
};


typedef struct item_t item_t;
struct item_t
{
	xstr_t key;	/* '\0' terminated */
	xstr_t value;	/* '\0' terminated */
};

static int item_cmp(const void *a, const void *b)
{
	item_t *x = (item_t *)a;
	item_t *y = (item_t *)b;
	return xstr_compare(&x->key, &y->key);
}

setting_t *setting_create()
{
	setting_t *st = NULL;
	ostk_t *ostk = ostk_create(0);
	if (!ostk)
		return NULL;

	st = (setting_t *)ostk_alloc(ostk, sizeof(setting_t));
	st->ostk = ostk;
	rbtree_init(&st->tree, sizeof(item_t), item_cmp, &ostk_xmem, ostk);
	return st;
}


void setting_destroy(setting_t *st)
{
	if (st)
	{
		ostk_destroy(st->ostk);
	}
}


inline xstr_t setting_get_xstr(setting_t *st, const char *key)
{
	item_t *item, hint;

	xstr_c(&hint.key, key);
	item = rbtree_find(&st->tree, &hint);
	return item ? item->value : xstr_null;
}

const char *setting_get_cstr(setting_t *st, const char *key)
{
	xstr_t v = setting_get_xstr(st, key);
	return v.len ? (const char *)v.data : "";
}

intmax_t setting_get_integer(setting_t *st, const char *key, intmax_t dft)
{
	const char *str = setting_get_cstr(st, key);
	if (str)
	{
		char *end;
		intmax_t i = strtoll(str, &end, 0);
		if (end > str)
		{
			if (end[0])
			{
				intmax_t x = binary_prefix(end, &end);
				if (end[0] == 0)
				{
					i *= x;
				}
				else
				{
					double r = strtod(str, &end);
					if (end > str && end[0] == 0)
					{
						i = r > INTMAX_MAX ? INTMAX_MAX
							 : r < INTMAX_MIN ? INTMAX_MIN
							 : (intmax_t)r;
					}
					else
						goto invalid;
				}
			}

			return i;
		}
	}
invalid:
	return dft;
}

bool setting_get_bool(setting_t *st, const char *key, bool dft)
{
	const char *str = setting_get_cstr(st, key);
	if (str)
	{
		if (isdigit(str[0]) || str[0] == '-' || str[0] == '+')
			return atoi(str);

		if (strcasecmp(str, "true") == 0 || strcasecmp(str, "yes") == 0
			|| strcasecmp(str, "on") == 0
			|| strcasecmp(str, "t") == 0 || strcasecmp(str, "y") == 0)
			return true;

		if (strcasecmp(str, "false") == 0 || strcasecmp(str, "no") == 0
			|| strcasecmp(str, "off") == 0
			|| strcasecmp(str, "f") == 0 || strcasecmp(str, "n") == 0)
			return false;
	}
	return dft;
}

double setting_get_floating(setting_t *st, const char *key, double dft)
{
	const char *str = setting_get_cstr(st, key);
	if (str)
	{
		char *end;
		double r = strtod(str, &end);
		if (end > str && end[0] == 0)
			return r;
	}
	return dft;
}

static bool st_insert(setting_t *st, const xstr_t *key, const char *value)
{
	item_t *item, hint;

	hint.key = *key;
	item = rbtree_insert(&st->tree, &hint);
	if (item)
	{
		int vlen = strlen(value);
		item->key.data = ostk_copyz(st->ostk, key->data, key->len);
		item->key.len = key->len;
		item->value.data = ostk_copyz(st->ostk, value, vlen);
		item->value.len = vlen;
		return true;
	}
	return false;
}

bool setting_insert(setting_t *st, const char *key, const char *value)
{
	xstr_t k = XSTR_C(key);
	return st_insert(st, &k, value);
}

static bool st_update(setting_t *st, const xstr_t *key, const char *value)
{
	item_t *item, hint;

	item = rbtree_find(&st->tree, &hint);
	if (item)
	{
		int vlen = strlen(value);
		if (vlen <= item->value.len)
		{
			memcpy(item->value.data, value, vlen + 1);
			item->value.len = vlen;
		}
		else
		{
			item->value.data = ostk_copyz(st->ostk, value, vlen);
			item->value.len = vlen;
		}
		return true;
	}
	return false;
}

bool setting_update(setting_t *st, const char *key, const char *value)
{
	xstr_t k = XSTR_C(key);
	return st_update(st, &k, value);
}

void setting_set(setting_t *st, const char *key, const char *value)
{
	xstr_t k = XSTR_C(key);

	if (!st_insert(st, &k, value))
	{
		st_update(st, &k, value);
	}
}

bool setting_load(setting_t *st, const char *filename, bool replace)
{
	char *line = NULL;
	size_t size = 0;
	ssize_t n;
	FILE *fp = fopen(filename, "rb");
	if (!fp)
		return false;

	while ((n = getline(&line, &size, fp)) > 0)
	{
		item_t *item, hint;
		xstr_t k;
		xstr_t v = XSTR_INIT((unsigned char*)line, n);
		xstr_trim(&v);

		if (v.len == 0 || v.data[0] == '#')
			continue;

		if (!xstr_delimit_char(&v, '=', &k))
			continue;

		xstr_rtrim(&k);
		xstr_ltrim(&v);
		if (k.len == 0 || v.len == 0)
			continue;

		hint.key = k;
		item = rbtree_insert(&st->tree, &hint);
		if (item)
		{
			item->key.data = ostk_copyz(st->ostk, k.data, k.len);
			item->key.len = k.len;
			item->value.data = ostk_copyz(st->ostk, v.data, v.len);
			item->value.len = v.len;
		}
		else if (replace)
		{
			item = rbtree_find(&st->tree, &hint);
			if (v.len <= item->value.len)
			{
				memcpy(item->value.data, v.data, v.len);
				item->value.data[v.len] = 0;
				item->value.len = v.len;
			}
			else
			{
				item->value.data = ostk_copyz(st->ostk, v.data, v.len);
				item->value.len = v.len;
			}
		}
	}

	free(line);
	fclose(fp);
	return true;
}


