#include "ShadowBox.h"
#include "xslib/Srp6a.h"
#include "xslib/opt.h"
#include "xslib/xbase64.h"
#include "xslib/xbase32.h"
#include "xslib/urandom.h"
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define SHADOW_GEN_VERSION	"221028.11"

#define ID_LEN_MAX		79
#define RANDID_LEN_MIN		23
#define RANDID_R_LEN		16

#define PS_LEN_MAX		79
#define RANDPS_LEN		39


void display_version(const char *program)
{
	fprintf(stderr, 
"shadow_gen " SHADOW_GEN_VERSION "\n"
"$build: shadow_gen-" SHADOW_GEN_VERSION " " __DATE__ " " __TIME__ " $\n"
	);
	exit(1);
}

void usage(const char *program)
{
	fprintf(stderr, "Usage: %s [options] <identity> [password]\n", program);
	fprintf(stderr,
"  -f filename          shadow file to read SRP6a parameters\n"
"  -p paramId:hashId    parameter and hash used\n"
"  -V                   display version information\n"
"\n"
"paramId can be read from file, which must specified with option -f\n"
"or be internal parameter, which may be @512, @1024, @2048, @4096.\n"
"\n"
"hashId can only be SHA256 or SHA1. If not specified, it will be SHA256.\n"
"\n"
"If identity ends with '%%', the '%%' will be replaced with a random string.\n"
"If it embeds one '^', the '^' will be replaced by a 8-char datetime string.\n"
"The 8-char datetime is in UTC time, and has a resolution of 1/8 seconds.\n"
"\n"
"If password is omitted, it will be a randomly generated string.\n"
"If it is '-', it will be read (at most %d printable character except ':'\n"
"and space) from stdin.\n"
"\n",
	PS_LEN_MAX);
	exit(1);
}

// return 8
static ssize_t _gm_datetime(char *buf, struct timespec *ts)
{
	struct tm tm;
	gmtime_r(&ts->tv_sec, &tm);
	return sprintf(buf, "%02d%c%c%c%c%c%c",
		tm.tm_year%100,
		xbase32_alphabet[tm.tm_mon+1],
		xbase32_alphabet[tm.tm_mday],
		xbase32_alphabet[tm.tm_hour],
		xbase32_alphabet[tm.tm_min/2],
		xbase32_alphabet[15 * (tm.tm_min&0x1) + tm.tm_sec/4],
		xbase32_alphabet[8 * (tm.tm_sec&0x3) + ts->tv_nsec/125000000]);
}

static bool generate_id(char *buf, size_t buflen, const char *identity)
{
	static bset_t id_bset = make_bset_by_add_cstr(&alnum_bset, "@._-^%");

	xstr_t xs = XSTR_C(identity);
	if (!isalpha(xs.data[0]))
	{
		fprintf(stderr, "ERROR: the first char of identity must be a letter\n");
		exit(1);
	}

	ssize_t rc;
	if ((rc = xstr_find_not_in_bset(&xs, 0, &id_bset)) >= 0)
	{
		fprintf(stderr, "ERROR: invalid char '%c' in identity, which can only contain letters, digits and those in \"@._-\"\n", xs.data[rc]);
		exit(1);
	}

	ssize_t k = xstr_find_char(&xs, 1, '^');
	if (k > 0 && xstr_find_char(&xs, k + 1, '^') > 0)
	{
		fprintf(stderr, "ERROR: char '^' can only occurr once in identity\n");
		exit(1);
	}

	ssize_t j = xstr_find_char(&xs, 1, '%');
	if (j > 0 && j != xs.len - 1)
	{
		fprintf(stderr, "ERROR: char '%%' can only be the last char of identity\n");
		exit(1);
	}

	if (k < 0 && j < 0)
		return false;

	size_t resultlen = xs.len;
	if (j > 0)
	{
		resultlen += RANDID_R_LEN - 1;
		if (k > 0)
			resultlen--;
	}
	else if (k > 0)
	{
		resultlen += 8 - 1;	// length of datetime string minus length of '^'
	}

	if (resultlen >= buflen)
	{
		fprintf(stderr, "ERROR: identity too long, total length must be no more than %zd after finishing substitutes\n", buflen - 1);
		exit(1);
	}

	int pos = 0;
	if (k > 0)
	{
		memcpy(buf + pos, identity, k);
		pos += k;

		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		pos += _gm_datetime(buf + pos, &ts);
	}

	if (j > 0)
	{
		int k1 = k + 1;
		int n = j - k1;
		if (n > 0)
		{
			memcpy(buf + pos, identity + k1, n);
			pos += n;
		}

		int rlen = RANDID_R_LEN;
		if (k > 0)
			rlen -= 8;
		if (rlen < RANDID_LEN_MIN - pos)
			rlen = RANDID_LEN_MIN - pos;
		base32id_from_entropy(buf + pos, rlen + 1, getentropy);
	}
	return true;
}

static bool generate_pass(char *buf, size_t buflen, const char *password)
{
	static bset_t pass_bset = make_bset_by_del_cstr(&graph_bset, ":");

	if (!password)
	{
		base57id_from_entropy(buf, RANDPS_LEN + 1, getentropy);
		return true;
	}

	bool changed = false;
	xstr_t xs = XSTR_C(password);
	char line[PS_LEN_MAX+1];
	if (strcmp(password, "-") == 0)
	{
		char *s = fgets(line, sizeof(line), stdin);
		if (!s)
		{
			fprintf(stderr, "ERROR: no password inputed from stdin\n");
			exit(1);
		}
		xs = XSTR_C(s);
		xstr_trim(&xs);
		xstr_copy_cstr(&xs, buf, buflen);
		changed = true;
	}

	int rc = xstr_find_not_in_bset(&xs, 0, &pass_bset);
	if (rc >= 0)
	{
		fprintf(stderr, "ERROR: the password contains invalid char '%c'\n", xs.data[rc]);
		exit(1);
	}
	return changed;
}


int main(int argc, char **argv)
try 
{
	const char *prog = argv[0];
	const char *identity, *password;
	const char *filename = NULL;
	const char *param = NULL;
	bool show_version = false;
	int optend;

	OPT_BEGIN(argc, argv, &optend)
	{
	case 'f':
		filename = OPT_EARG(usage(prog));
		break;
	case 'p':
		param = OPT_EARG(usage(prog));
		break;
	case 'V':
		show_version = true;
		break;
	default:
		usage(prog);
	} OPT_END();

	if (show_version)
		display_version(prog);

	if (argc - optend < 1)
		usage(prog);

	if (!param || !param[0])
		param = "@2048:SHA256";

	if (param[0] != '@' && !filename)
	{
		usage(prog);
	}

	xstr_t paramId;
	xstr_t hashId = XSTR_C(param);
	xstr_delimit_char(&hashId, ':', &paramId);

	if (hashId.len == 0)
		xstr_c(&hashId, "SHA256");

	if (!xstr_case_equal_cstr(&hashId, "SHA256") && !xstr_case_equal_cstr(&hashId, "SHA1"))
	{
		fprintf(stderr, "ERROR: hashId must be SHA256 or SHA1\n");
		exit(1);
	}

	identity = argv[optend];
	password = (optend + 1 < argc) ? argv[optend + 1] : NULL;

	ShadowBoxPtr sb = ShadowBox::createFromFile(filename ? filename : "");

	int bits;
	uintmax_t g;
	xstr_t N;
	if (!sb->getSrp6aParameter(paramId, bits, g, N))
	{
		fprintf(stderr, "Unknown paramId %s\n", param);
		exit(1);
	}

	char idbuf[ID_LEN_MAX+1];
	bool id_changed = generate_id(idbuf, sizeof(idbuf), identity);
	if (id_changed)
		identity = idbuf;

	char passbuf[PS_LEN_MAX+1];
	bool ps_changed = generate_pass(passbuf, sizeof(passbuf), password);
	if (ps_changed)
		password = passbuf;

	Srp6aClientPtr srp6a = new Srp6aClient(g, N, bits, hashId);
	srp6a->set_identity(identity, password);
	srp6a->gen_salt();
	xstr_t salt = srp6a->get_salt();
	xstr_t verifier = srp6a->compute_v();

	int i, len;
	char buf[8192];
	len = xbase64_encode(&url_xbase64, buf, salt.data, salt.len, XBASE64_NO_PADDING);
	fprintf(stdout, "\n[verifier]\n\n");
	fprintf(stdout, "!%s = SRP6a:%.*s:%s:%.*s:\n", identity, XSTR_P(&paramId), srp6a->hash_name(), len, buf);

	len = xbase64_encode(&url_xbase64, buf, verifier.data, verifier.len, XBASE64_NO_PADDING);
	for (i = 0; i < len; i += 64)
	{
		int n = len - i;
		if (n > 64)
			n = 64;

		fputs("        ", stdout);
		fwrite(buf + i, 1, n, stdout);
		fputc('\n', stdout);
	}
	fputc('\n', stdout);

	if (id_changed || ps_changed)
	{
		fputs("#\n", stderr);
		fputs("# THE FOLLOWING LINES ARE OUTPUTED TO THE STDERR\n", stderr);
		fputs("# DON'T COPY THEM TO THE SHADOW FILE\n", stderr);
		fputs("#\tID : PASS =\n", stderr);
		fprintf(stderr, "### %s : %s\n", identity, password);
		fputs("#\n", stderr);
	}

	return 0;
}
catch (std::exception& ex)
{
	fprintf(stderr, "EXCEPTION: %s\n", ex.what());
	return 1;
}

