/*
   Author: XIONG Jiagui
   Date: 2006-08-09
 */
#define _GNU_SOURCE
#include "wdict.h"
#include "ostk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdbool.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: wdict.c,v 1.25 2015/05/18 07:12:50 gremlin Exp $";
#endif


struct node
{
	int value;
	int left;
	int next;
	int right;
};

struct node_pool
{
	struct node *nodes;
	int used;
	int allocated;
};

struct wdict_t
{
	int num_word;
	int max_word_length;
	int num_node;
	struct node *nodes;
};

struct strbuf
{
	char *str;
	int len;
};


static int pool_alloc(struct node_pool *pl)
{
	int size;
	if (pl->used >= pl->allocated)
	{
		if (pl->allocated == 0)
			pl->allocated = 4096;
		else
			pl->allocated *= 2;
		size = pl->allocated * sizeof(pl->nodes[0]);
		pl->nodes = (struct node *)realloc(pl->nodes, size);
	}
	memset(&pl->nodes[pl->used], 0, sizeof(struct node));
	return pl->used++;
}

static int compare(const void *v1, const void *v2)
{
	char *s1 = ((struct strbuf *)v1)->str;
	char *s2 = ((struct strbuf *)v2)->str;
	return strcmp(s1, s2);
}

static inline int find(register struct node *nodes, register int p, register int value)
{
	if (p == 0)
		return 0;

	do
	{
		register struct node *nptr = &nodes[p];
		if (value < nptr->value)
			p = nptr->left;
		else if (value == nptr->value)
			return p;
		else
			p = nptr->right;
	} while (p);

	return 0;
}

static int make_tree(struct node_pool *pl, int prefix_len, const struct strbuf *strv, int strv_num)
{
	int charnum, middle;
	int start, end;
	int value;
	int p, ptr;
	int i;
	int has_prefix_string;

	for (has_prefix_string = 0; strv_num > 0 && strv->len <= prefix_len; )
	{
		++strv;
		--strv_num;
		++has_prefix_string;
	}
	
	if (strv_num == 0)
		return 0;

	charnum = 0;
	value = -1;
	for (i = 0; i < strv_num; ++i)
	{
		int new_value = (unsigned char)strv[i].str[prefix_len];
		if (value != new_value)
		{
			value = new_value;
			++charnum;
		}
	}
	middle = charnum / 2;
	
	charnum = 0;
	value = -1;
	for (i = 0; i < strv_num; ++i)
	{
		int new_value = (unsigned char)strv[i].str[prefix_len];
		if (value != new_value)
		{
			value = new_value;
			if (charnum == middle)
			{
				break;
			}
			++charnum;
		}
	}
	start = i;
	assert(start < strv_num);
	for (++i; i < strv_num; ++i)
	{
		if (value != (unsigned char)strv[i].str[prefix_len])
		{
			break;
		}
	}
	end = i;

	p = pool_alloc(pl); 
	pl->nodes[p].value = value;

	ptr = make_tree(pl, prefix_len + 1, &strv[start], end - start);
	pl->nodes[p].next = ptr;

	if (start > 0)
	{
		ptr = make_tree(pl, prefix_len, strv, start);
		pl->nodes[p].left = ptr;
	}
	if (strv_num - end > 0)
	{
		ptr = make_tree(pl, prefix_len, &strv[end], strv_num - end);
		pl->nodes[p].right = ptr;
	}

	return (has_prefix_string) ? -p : p;
}

static void output(struct node *nodes, int p, int level, char *word, FILE *fp)
{
	if (nodes[p].left)
		output(nodes, nodes[p].left, level, word, fp);

	word[level] = nodes[p].value;

	if (nodes[p].next > 0)
		output(nodes, nodes[p].next, level + 1, word, fp);
	else if (nodes[p].next == 0)
	{
		fwrite(word, 1, level + 1, fp);
		putc('\n', fp);
	}
	else
	{
		fwrite(word, 1, level + 1, fp);
		putc('\n', fp);
		output(nodes, -nodes[p].next, level + 1, word, fp);
	}

	if (nodes[p].right)
		output(nodes, nodes[p].right, level, word, fp);
}

wdict_t *wdict_compile(wdict_generator_t *gen, void *source)
{
	struct node_pool pool;
	struct strbuf *strv = NULL;
	int strv_used = 0;
	int strv_allocated = 0;
	int p, ptr;
	int max_word_length = 0;
	char *word;
	int len;
	wdict_t *wd = NULL;
	ostk_t *mem = NULL;

	strv_allocated = 4096;
	strv_used = 0;
	strv = (struct strbuf *)malloc(strv_allocated * sizeof(strv[0]));

	mem = ostk_create(0);
	while ((len = gen(&word, source)) >= 0)
	{
		if (len == 0)
			continue;
		if (strv_used >= strv_allocated)
		{
			strv_allocated *= 2;
			strv = (struct strbuf *)realloc(strv, strv_allocated * sizeof(strv[0]));
		}
		if (len >= WDICT_WORD_MAX_SIZE)
			len = WDICT_WORD_MAX_SIZE - 1;
		strv[strv_used].str = ostk_copyz(mem, word, len);
		strv[strv_used].len = len;
		++strv_used;
		if (max_word_length < len)
			max_word_length = len;
	}
	qsort(strv, strv_used, sizeof(strv[0]), compare);

	memset(&pool, 0, sizeof(pool));
	p = pool_alloc(&pool);
	assert(p == 0);

	/* XXX: the pointer pool.nodes will be changed when make_tree()ing.
	   So we should NOT write like this:
		pool.nodes[p].next = make_tree(&pool, 0, strv, strv_used);
	   It's nasty, but I have not find an elegant solution.
	 */
	ptr = make_tree(&pool, 0, strv, strv_used);
	pool.nodes[p].next = ptr;


	ostk_destroy(mem);
	free(strv);
	
	wd = (struct wdict_t *)calloc(1, sizeof(wd[0]));
	wd->num_word = strv_used;
	wd->nodes = pool.nodes;
	wd->num_node = pool.used;
	wd->max_word_length = max_word_length;
	return wd;
}

struct file_source
{
	FILE *fp;
	char *line;
	size_t line_size;
};

static int _gen(char **p_word, struct file_source *src)
{
	int space_len, len;
	char *word;
	
	len = getline(&src->line, &src->line_size, src->fp);
	if (len <= 0)
		return len;

	space_len = strspn(src->line, " \t\r\n");
	word = src->line + space_len;
	len -= space_len;
	if (len)
	{
		--len;
		while (len >= 0 && strchr(" \t\r\n", word[len]))
			--len;
		++len;
	}
	word[len] = 0;
	*p_word = word;
	return len;
}


wdict_t *wdict_load_file(const char *filename)
{
	wdict_t *wd = NULL;
	struct file_source src = {0};
	src.fp = fopen(filename, "rb");
	if (!src.fp)
		return NULL;
	wd = wdict_compile((wdict_generator_t *)_gen, &src);
	free(src.line);
	fclose(src.fp);
	return wd;
}

struct multi_file_source
{
	char **filenames;
	int num;
	int cur;
	struct file_source src;
};

static int _multigen(char **p_word, struct multi_file_source *s)
{
	int rc;

again:
	if (s->cur >= s->num)
		return -1;

	if (s->src.fp == NULL)
	{
		while (s->cur < s->num)
		{
			s->src.fp = fopen(s->filenames[s->cur], "rb");
			if (s->src.fp)
				break;
			else
				s->cur++;
		}

		if (s->src.fp == NULL)
			return -1;
	}

	rc = _gen(p_word, &s->src);
	if (rc < 0)
	{
		fclose(s->src.fp);
		s->src.fp = NULL;
		s->cur++;
		goto again;
	}
	return rc;
}

wdict_t *wdict_load_multi_file(char *filenames[], int num)
{
	wdict_t *wd = NULL;
	struct multi_file_source multisrc = {0};
	multisrc.filenames = filenames;
	multisrc.num = num;
	wd = wdict_compile((wdict_generator_t *)_multigen, &multisrc);
	free(multisrc.src.line);
	return wd;
}

int wdict_max_word_length(wdict_t *wd)
{
	return wd->max_word_length;
}

int wdict_num_word(wdict_t *wd)
{
	return wd->num_word;
}

void wdict_dump(wdict_t *wd, FILE *out)
{
	char word[WDICT_WORD_MAX_SIZE];
	struct node *nodes = wd->nodes;
	int p = nodes[0].next;
	output(nodes, p, 0, word, out);
}

/* Return the length of the matched word (the word in the tree, not that in the string).
   The length of the matched string is returned in *plen.
   If match failed, return -1.
 */
static int match(struct node *nodes, int p, const unsigned char *str, const unsigned char *over,
		int flag, char **p_end, char *word)
{
	const unsigned char *sp = str;
	const unsigned char *saved_sp = NULL;
	int saved_wlen = 0;
	bool ignore_case = false;
	bool ignore_space = false;
	int wlen = 0;

	if (flag & WDICT_ICASE)
		ignore_case = true;
	if (flag & WDICT_ISPACE)
		ignore_space = true;

	while ((!over && *sp) || (sp < over))
	{
		int c = *sp++;
		int pp2 = 0;
		int p2 = find(nodes, p, c);
		if (ignore_case && isalpha(c))
		{
			pp2 = find(nodes, p, islower(c) ? toupper(c) : tolower(c));
			if (!p2 && pp2)
			{
				p2 = pp2;
				pp2 = 0;
			}
		}

		if (p2 == 0)
		{
			if (ignore_space && isspace(c))
				continue;
			break;
		}
	
		if (pp2)
		{
			char xword[WDICT_WORD_MAX_SIZE];
			int wlen1, wlen2;
			char *end1, *end2;

			if (word)
				word[wlen] = nodes[pp2].value;
			++wlen;
			p = nodes[pp2].next;
			if (p <= 0)
			{
				saved_sp = sp;
				saved_wlen = wlen;
				p = -p;
			}
			wlen2 = p ? match(nodes, p, sp, over, flag, &end2, word ? xword : 0) : -1;


			--wlen;

			if (word)
				word[wlen] = nodes[p2].value;
			++wlen;
			p = nodes[p2].next;
			if (p <= 0)
			{
				saved_sp = sp;
				saved_wlen = wlen;
				p = -p;
			}
			wlen1 = p ? match(nodes, p, sp, over, flag, &end1, word ? &word[wlen] : 0) : -1;


			if (wlen1 < 0 && wlen2 < 0)
				break;

			if (wlen2 > wlen1)
			{
				if (word)
				{
					word[wlen-1] = nodes[pp2].value;
					memcpy(&word[wlen], xword, wlen2);
				}
				wlen += wlen2;
				saved_sp = (unsigned char *)end2;
				saved_wlen = wlen;
			}
			else
			{
				wlen += wlen1;
				saved_sp = (unsigned char *)end1;
				saved_wlen = wlen;
			}

			break;
		}
		else 
		{
			if (word)
				word[wlen] = nodes[p2].value;
			++wlen;
			p = nodes[p2].next;

			if (p == 0)
			{
				saved_sp = sp;
				saved_wlen = wlen;
				break;
			}
			else if (p < 0)
			{
				saved_sp = sp;
				saved_wlen = wlen;
				p = -p;
			}
		}
	}

	if (saved_sp)
	{
		if (p_end)
			*p_end = (char *)saved_sp;
		if (word)
			word[saved_wlen] = '\0';
		return saved_wlen;
	}

	return -1;
}

static char *_wdict_match(wdict_t *wd, const char *str, const char *over, int flag, char **p_end, char *theword)
{
	struct node *nodes = wd->nodes;
	const unsigned char *sb = (const unsigned char *)str;
	int len;

	if (flag & WDICT_ISPACE)
	{
		while ((!over && *sb) || (sb < (const unsigned char *)over))
		{
			if (!isspace(*sb))
				break;
			++sb;
		}
	}

	len = match(nodes, nodes[0].next, sb, (unsigned char *)over, flag, p_end, theword);
	if (len >= 0)
		return (char *)sb;

	if (p_end)
		*p_end = (char *)sb;
	if (theword)
		theword[0] = 0;
	return NULL;
}

bool wdict_match_xstr(wdict_t *wd, const xstr_t *str, int flag, xstr_t *found, char *theword/*NULL*/)
{
	char *start, *end;
	start = _wdict_match(wd, (char *)str->data, (char *)str->data + str->len, flag, &end, theword);
	if (start)
	{
		found->data = (unsigned char *)start;
		found->len = end - start;
		return true;
	}
	*found = xstr_null;
	return false;
}

char *wdict_match(wdict_t *wd, const char *str, int flag, char **p_end, char *theword)
{
	return _wdict_match(wd, str, NULL, flag, p_end, theword);
}

static char *_wdict_search(wdict_t *wd, const char *str, const char *over, int flag, char **p_end, char *theword)
{
	struct node *nodes = wd->nodes;
	const unsigned char *sb;
	bool odd = false;
	bool ignore_space = false;
	bool not_gbk = false;

	if (flag & WDICT_ISPACE)
		ignore_space = true;

	if (flag & WDICT_NOTGBK)
		not_gbk = true;

	for (sb = (const unsigned char *)str; ((!over && *sb) || sb < (const unsigned char *)over); ++sb)
	{
		if (not_gbk || !odd)
		{
			int len;
			if (ignore_space && isspace(*sb))
				continue;
			
			len = match(nodes, nodes[0].next, sb, (unsigned char *)over, flag, p_end, theword);
			if (len >= 0)
				return (char *)sb;
		}

		if (!not_gbk)
			odd = (*sb < 0x80) ? false : !odd;
	}
	
	if (p_end)
		*p_end = (char *)sb;
	if (theword)
		theword[0] = 0;
	return NULL;
}

bool wdict_search_xstr(wdict_t *wd, const xstr_t *str, int flag, xstr_t *found, char *theword/*NULL*/)
{
	char *start, *end;
	start = _wdict_search(wd, (char *)str->data, (char *)str->data + str->len, flag, &end, theword);
	if (start)
	{
		found->data = (unsigned char *)start;
		found->len = end - start;
		return true;
	}
	*found = xstr_null;
	return false;
}

char *wdict_search(wdict_t *wd, const char *str, int flag, char **p_end, char *theword/*NULL*/)
{
	return _wdict_search(wd, str, NULL, flag, p_end, theword);
}

void wdict_destroy(wdict_t *wd)
{
	if (wd)
	{
		free(wd->nodes);
		free(wd);
	}
}


#ifdef TEST_WDICT

int main(int argc, char **argv)
{
	char *begin, *end;
	char *word_file;
	char *filename;
	char *str, *s;
	int len;
	FILE *fp;
	wdict_t *wd = NULL;
	char theword[1024];

	if (argc < 3)
	{
		fprintf(stderr, "Usage: %s <word_file> <string_file>\n", argv[0]);
		exit(1);
	}
	word_file = argv[1];
	filename = argv[2];

	wd = wdict_load_file(word_file);
	if (!wd)
	{
		fprintf(stderr, "wdict_compile_file() failed");
		exit(1);
	}

	printf("num_word=%d\n", wdict_num_word(wd));

	fp = fopen(filename, "rb");
	if (!fp)
	{
		fprintf(stderr, "fopen() failed: %s\n", filename);
		exit(1);
	}
	str = (char *)malloc(10240);
	len = fread(str, 1, 10240-1, fp);
	printf("len=%d\n", len);
	str[len] = '\0';
	fclose(fp);

	s = str;
	while ((begin = wdict_search(wd, s, WDICT_NOTGBK | WDICT_ICASE | WDICT_ISPACE, &end, theword)) != 0)
	{
		fwrite(s, 1, begin - s, stdout);
		printf("<b>");
		fwrite(begin, 1, end - begin, stdout);
		printf("</b>");
		printf(":%s\n", theword);
		s = end;
	}
	printf("%s", s);
	free(str);

	/* wdict_dump(wd, stderr); */
	wdict_destroy(wd);

	return 0;
}

#endif

