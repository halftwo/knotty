#include "dlog.h"
#include "xslib/opt.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

void usage(const char *program)
{
	fprintf(stderr, "usage: %s [options] <tag> <messsage>\n", program);
	fprintf(stderr,
"  -t                 Use TCP to connect the dlogd server\n"
		);
	exit(1);
}

int main(int argc, char **argv)
{
	const char *prog = argv[0];
	const char *tag, *msg;
	int tcp = 0;
	int optend;

	OPT_BEGIN(argc, argv, &optend)
	{
	case 't':
		tcp = DLOG_TCP;
		break;
	default:
		usage(prog);
	} OPT_END();

	if (argc - optend < 2)
	{
		usage(prog);
	}

	tag = argv[optend];
	msg = argv[optend + 1];

	dlog_set("dlog_write", DLOG_STDERR | tcp);
	dlog(tag, "%s", msg);

	return 0;
}

