#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "dlog.h"
#include "dlog_imp.h"
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/time.h>
#include <assert.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <signal.h>


static xfmt_callback_function _xfmt_cb;
static dlog_callback_function _log_cb;
static void *_log_cb_state;
static char _identity[DLOG_IDENTITY_MAX + 1];
static xstr_t _identity_xs;
static bool _use_tcp;
static int _option;
static int _sockfd = -1;
static pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;

struct rec_head
{
	bool done;
	time_t time;
	int len;
	char buf[64];
};

static void _gen_rec_head(struct rec_head *head)
{
	static int the_pid;
	struct tm tm;
	char tzbuf[8];

	head->done = true;
	time(&head->time);
	localtime_r(&head->time, &tm);

	if (!the_pid)
		the_pid = getpid();

	head->len = sprintf(head->buf, "%02d%02d%02d-%02d%02d%02d%s %s %d+%d",
		tm.tm_year - 100, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, dlog_timezone_str(tzbuf),
		"::", the_pid, 0);
}

static int _connect_daemon()
{
	static time_t last_time;
	int fd = -1;
	time_t now = time(NULL);

	if (now == last_time)
		return -1;

	if (last_time == 0)
		signal(SIGPIPE, SIG_IGN);

	last_time = now;

	{
		static struct sockaddr_in6 addr6;
		static socklen_t a6len;
		if (!a6len)
		{
			addr6.sin6_family = AF_INET6;
			inet_pton(AF_INET6, "::1", &addr6.sin6_addr);
			addr6.sin6_port = htons(DLOGD_PORT);
			a6len = sizeof(addr6);
		}

		fd = socket(PF_INET6, _use_tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
		if (fd >= 0)
		{
			if (connect(fd, (struct sockaddr *)&addr6, a6len) < 0)
			{
				close(fd);
				fd = -1;
			}
		}
	}

	if (fd < 0)
	{
		static struct sockaddr_in addr4;
		static socklen_t a4len;
		if (!a4len)
		{
			addr4.sin_family = AF_INET;
			inet_pton(AF_INET, "127.0.0.1", &addr4.sin_addr);
			addr4.sin_port = htons(DLOGD_PORT);
			a4len = sizeof(addr4);
		}

		fd = socket(PF_INET, _use_tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
		if (fd >= 0)
		{
			if (connect(fd, (struct sockaddr *)&addr4, a4len) < 0)
			{
				close(fd);
				fd = -1;
			}
		}
	}

	if (fd >= 0)
		fcntl(fd, F_SETFD, FD_CLOEXEC);

	return fd;
}


void dlog_set(const char *identity, int option)
{
	if (identity)
	{
		const char *p = (const char *)memccpy(_identity, identity, 0, sizeof(_identity) - 1);
		xstr_init(&_identity_xs, (unsigned char *)_identity, (p ? p - 1 - _identity : sizeof(_identity) - 1));
	}

	dlog_set_option(option);

	pthread_mutex_lock(&_mutex);
	if (_sockfd < 0)
		_sockfd = _connect_daemon();
	pthread_mutex_unlock(&_mutex);
}

const char *dlog_identity()
{
	return _identity;
}

int dlog_option()
{
	return _option;
}

void dlog_set_option(int option)
{
	bool tcp = (option & DLOG_TCP);

	_option = option;
	if (tcp ^ _use_tcp)
	{
		_use_tcp = tcp;
		if (_sockfd >= 0)
		{
			int fd = _sockfd;
			_sockfd = 0;
			close(fd);
		}
	}
}

void dlog_set_xformat(xfmt_callback_function xfmt_cb)
{
	_xfmt_cb = xfmt_cb;
}

void dlog_set_callback(dlog_callback_function cb, void *state)
{
	pthread_mutex_lock(&_mutex);
	_log_cb = cb;
	_log_cb_state = state;
	pthread_mutex_unlock(&_mutex);
}

static void _do_log(struct dlog_record *rec)
{
	dlog_callback_function callback;
	struct rec_head head;
	char *buf = (char *)rec;
	bool perror_done = false;
	bool failed = false;

	head.done = false;

	if (_option & DLOG_STDERR)
	{
		if (!head.done)
			_gen_rec_head(&head);

		perror_done = true;
		fprintf(stderr, "%s %s%s\n", head.buf, rec->str, rec->truncated ? "\a ..." : "");
	}

	pthread_mutex_lock(&_mutex);
	callback = _log_cb;
	if (callback == NULL || (*callback)(_log_cb_state, rec->str, rec->size - 1 - offsetof(struct dlog_record, str)) == 0)
	{
		int left, sent, n;
		bool conn = false;

try_again:
		left = rec->size;
		sent = 0;
		if (_sockfd < 0)
		{
			conn = true;
		 	_sockfd = _connect_daemon();
		 	if (_sockfd < 0)
			{
				failed = true;
				goto finish_writing;
			}
		}

		if (rec->pid == 0)
			rec->pid = getpid();

		do
		{
			n = write(_sockfd, buf + sent, left);
			if (n < 0)
			{
				if (errno == EINTR)
					n = 0;
				else
				{
					close(_sockfd);
					_sockfd = -1;

					if (!conn)
						goto try_again;

					failed = true;
					break;
				}
			}
			sent += n;
			if (_use_tcp)
				left -= n;
			else
				left = 0;
		} while (left > 0);
	}
finish_writing:
	pthread_mutex_unlock(&_mutex);

	if (failed)
	{
		if ((_option & DLOG_PERROR) && !perror_done)
		{
			if (!head.done)
				_gen_rec_head(&head);
			fprintf(stderr, "%s %s%s\n", head.buf, rec->str, rec->truncated ? "\a ..." : "");
		}
	}
}

void zdlog(const xstr_t *identity, const xstr_t *tag, const xstr_t *locus, const xstr_t *content)
{
	char buf[DLOG_RECORD_MAX_SIZE];
	struct dlog_record *rec = (struct dlog_record *)buf;

	if (!identity || !identity->len)
	{
		identity = &_identity_xs;
	}

	dlog_compose(rec, identity, tag, locus, content);

	_do_log(rec);
}


void vxdlog(xfmt_callback_function xfmt, const char *identity,
		const char *tag, const char *locus, const char *format, va_list ap)
{
	char buf[DLOG_RECORD_MAX_SIZE];
	struct dlog_record *rec = (struct dlog_record *)buf;

	if (!identity || !identity[0])
		identity = _identity;

	dlog_vmake(rec, xfmt ? xfmt : _xfmt_cb, identity, tag, locus, format, ap);

	_do_log(rec);
}

void xdlog(xfmt_callback_function xfmt, const char *identity,
		const char *tag, const char *locus, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vxdlog(xfmt, identity, tag, locus, format, ap);
	va_end(ap);
}

char *dlog_timezone_str(char buf[])
{
	static char saved_buf[8] = "+00";
	static long saved_tz;

	if (saved_tz != timezone)
	{
		saved_tz = timezone;
		long t = timezone >= 0 ? timezone : -timezone;
		t /= 60;
		int min = t % 60;
		t /= 60;

		// positive timezone value is west, negative is east
		int n = sprintf(buf, "%c%02ld", timezone<0?'+':'-', t);
		if (min)
		{
			sprintf(buf + n, "%02d", min);
		}
		strcpy(saved_buf, buf);
	}
	strcpy(buf, saved_buf);
	return buf;
}

char *dlog_local_time_str(char buf[], time_t t, bool tz)
{
	struct tm tm;

	localtime_r(&t, &tm);

	int n = sprintf(buf, "%02d%02d%02d%c%02d%02d%02d",
		tm.tm_year < 100 ? tm.tm_year : tm.tm_year - 100, tm.tm_mon + 1, tm.tm_mday,
		"umtwrfsu"[tm.tm_wday], tm.tm_hour, tm.tm_min, tm.tm_sec);

	if (tz)
		dlog_timezone_str(buf + n);
	return buf;
}


