#include "dlog_imp.h"
#include "dlog.h"
#include "plugin.h"
#include "luadlog.h"
#include "recpool.h"
#include "cabin.h"
#include "banlist.h"
#include "misc.h"
#include "xslib/cstr.h"
#include "xslib/path.h"
#include "xslib/ScopeGuard.h"
#include "xslib/md5.h"
#include "xslib/hex.h"
#include "xslib/unix_user.h"
#include "xslib/unixfs.h"
#include "xslib/opt.h"
#include "xslib/rdtsc.h"
#include "xslib/xlog.h"
#include "xslib/xnet.h"
#include "xslib/queue.h"
#include "xslib/loc.h"
#include "xslib/obpool.h"
#include "xslib/XEvent.h"
#include "xslib/daemon.h"
#include "xslib/dirwalk.h"
#include "xslib/mlzo.h"
#include <lz4.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <signal.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>

#define STACK_SIZE		(256*1024)

#define FLUSH_INTERVAL		(2*1000)	/* in milliseconds */
#define LOAD_INTERVAL		10		/* in seconds */
#define DLOGD_TIMEOUT		(900*1000)	/* in milliseconds */

#define LOGFILE_PREFIX		"xl."
#define ZIPFILE_SUFFIX		".lz4"

#define LOGFILE_RESERVE 	(DLOG_RECORD_MAX_SIZE - 256)
#define LOGFILE_SWITCH_MIN	(1024*1024*1 - LOGFILE_RESERVE)
#define LOGFILE_SWITCH_DFT	(1024*1024*1024 - LOGFILE_RESERVE)

#define BLOCK_SIZE	(DLOG_PACKET_MAX_SIZE + offsetof(struct packet_block, pkt))


static const char *TYPECODE = ">#@!????????????";

struct packet_block
{
	TAILQ_ENTRY(packet_block) link;
	struct dlog_timeval time;
	char pkt_ip[40];
	char out_addr[48];
	struct dlog_packet pkt;
};

static char _program_dir[PATH_MAX];
static char _program_name[32];

static XEvent::DispatcherPtr dispatcher;

static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static bool run_logger = true;
static bool flush_timer_expire;
static bool load_expire;
static bool minute_expire;
static bool hour_expire;
static bool day_expire;

static obpool_t _block_pool = OBPOOL_LIMIT_INITIALIZER(BLOCK_SIZE, 4096);

TAILQ_HEAD(block_queue, packet_block);
static struct block_queue _queue_pair[2], *_current_queue;
static int _endian;

static volatile time_t _current_time;
static char _log_dir[PATH_MAX];
static char _log_pathname[PATH_MAX];
static FILE *_logfile_fp;
static time_t _logfile_time;
static int _logfile_size;
static int _logfile_switch = LOGFILE_SWITCH_DFT;
static cabin_t *_cab;

static int _euid = -1;
static char _euser[32];
static struct timeval _utv;
static struct rusage _usage;

static unsigned short port = DLOG_CENTER_PORT;
static unsigned long long total_size;
static time_t active_time;
static char start_time_str[24];
static xatomic_t num_client;
static char the_ip[40];

static unsigned long long num_banned;
static unsigned long long num_record_error;
static unsigned long long num_record;

static xatomiclong_t num_block;
static xatomiclong_t num_block_error;
static xatomiclong_t num_zip_block;
static xatomiclong_t num_unzip_fail;

static plugin_t *plugin;
static char plugin_file[PATH_MAX];
static time_t plugin_mtime;
static char plugin_md5[33] = "-";
static unsigned long long plugin_run;
static unsigned long long plugin_error;
static unsigned long long plugin_discard;

static banlist_t *banlist;
static char banlist_file[PATH_MAX];
static time_t banlist_mtime;

static int64_t usec_diff(const struct timeval* t1, const struct timeval* t2)
{
	int64_t x1 = t1->tv_sec * 1000000 + t1->tv_usec;
	int64_t x2 = t2->tv_sec * 1000000 + t2->tv_usec;
	return x1 - x2;
}

static FILE *_open_log_file(const char *dir, time_t *fp_time)
{
	char time_str[32];
	char subdir[PATH_MAX];
	FILE *fp = NULL;

	time_t now = dispatcher->msecRealtime() / 1000;
	get_time_str(now, time_str);
	snprintf(subdir, sizeof(subdir), "%s/%.6s", dir, time_str);
	snprintf(_log_pathname, sizeof(_log_pathname), "%s/%s%s", subdir, LOGFILE_PREFIX, time_str);

	if (mkdir(subdir, 0775) == -1 && errno != EEXIST)
	{
		return NULL;
	}

	fp = fopen(_log_pathname, "ab");
	if (fp)
	{
		char pathname[PATH_MAX];
		int dir_len = strlen(dir);

		snprintf(pathname, sizeof(pathname), "%s/%s%s", dir, LOGFILE_PREFIX, time_str);
		link(_log_pathname, pathname);

		snprintf(pathname, sizeof(pathname), "%s/zlog", dir);
		unlink(pathname);
		symlink(_log_pathname + dir_len + 1, pathname);

		if (fp_time)
			*fp_time = now;
	}

	return fp;
}

static void do_compress(const char *pathname)
{
	struct stat st;
	if (stat(pathname, &st) != 0)
		return;
	size_t size = st.st_size;

	const char *p;
	p = strrchr(pathname, '/');
	if (!p)
		return;
	int subdir_len = p - pathname;

	p = (const char *)memrchr(pathname, '/', subdir_len);
	if (!p)
		return;
	int dir_len = p - pathname;

	const char *filename = pathname + subdir_len + 1;
	assert(strncmp(filename, LOGFILE_PREFIX, sizeof(LOGFILE_PREFIX)-1) == 0);

	char path2[PATH_MAX];
	int status = -1;
	if (size > 0)
	{
		snprintf(path2, sizeof(path2), "%.*s/%s%s", subdir_len, pathname,
			&filename[sizeof(LOGFILE_PREFIX) - 1], ZIPFILE_SUFFIX);

		char cmd[PATH_MAX];
		snprintf(cmd, sizeof(cmd), 
			"/usr/bin/env PATH=.:%s:/usr/local/bin:/usr/bin:${PATH} lz4 -f %s %s 2> /dev/null",
			_program_dir, pathname, path2);
		int rc = system(cmd);
		status = WEXITSTATUS(rc);
	}
	else
	{
		status = 0;
	}

	if (status == 0)
		unlink(pathname);

	snprintf(path2, sizeof(path2), "%.*s/%s", dir_len, pathname, filename);
	unlink(path2);
}

static void *compressor(void *arg)
{
	char *pathname = (char *)arg;
	pthread_detach(pthread_self());
	if (pathname)
	{
		do_compress(pathname);
		free(pathname);
	}
	return NULL;
}

static void _switch_log_file()
{
	fclose(_logfile_fp);

	pthread_t thr;
	pthread_create(&thr, NULL, compressor, strdup(_log_pathname));

	_logfile_size = 0;
	_logfile_fp = _open_log_file(_log_dir, &_logfile_time);
	if (_logfile_fp == NULL)
	{
		fprintf(stderr, "_open_log_file() failed, pathname=%s, errno=%d, %m\nexit(1)", _log_pathname, errno);
		exit(1);
	}
}

static void init_global()
{
	TAILQ_INIT(&_queue_pair[0]);
	TAILQ_INIT(&_queue_pair[1]);
	_current_queue = &_queue_pair[0];
	_endian = xnet_get_endian();
}

static void load_plugin()
{
	struct stat st;
	if (stat(plugin_file, &st) == -1)
	{
		if (plugin)
		{
			plugin_close(plugin);
			plugin = NULL;
			plugin_mtime = 0;
			plugin_run = 0;
			plugin_discard = 0;
			plugin_error = 0;
		}
	}
	else if (st.st_mtime != plugin_mtime)
	{
		plugin_close(plugin);
		plugin_mtime = st.st_mtime;
		plugin = plugin_load(plugin_file);
		plugin_run = 0;
		plugin_discard = 0;
		plugin_error = 0;

		unsigned char *content = NULL;
		size_t size = 0;
		ssize_t n = unixfs_get_content(plugin_file, &content, &size);
		ON_BLOCK_EXIT(free_pptr<unsigned char>, &content);
		if (n >= 0)
		{
			unsigned char digest[16];
			md5_checksum(digest, content, n);
			hexlify(plugin_md5, digest, sizeof(digest));
		}
		else
		{
			strcpy(plugin_md5, "-");
		}
	}
}

static void load_banlist()
{
	struct stat st;
	if (stat(banlist_file, &st) == -1)
	{
		banlist_close(banlist);
		banlist = NULL;
		banlist_mtime = 0;
	}
	else if (st.st_mtime != banlist_mtime)
	{
		banlist_close(banlist);
		banlist = banlist_load(banlist_file);
		banlist_mtime = st.st_mtime;
	}
}

static bool get_ip(char ip[], bool external)
{
	bool fail = external ? (!xnet_get_external_ip(ip) && !xnet_get_internal_ip(ip))
			: (!xnet_get_internal_ip(ip) && !xnet_get_external_ip(ip));

	if (fail)
		strcpy(ip, "-");

	return !fail;
}

class Listener: public XEvent::FdHandler
{
	int _event_fd;
public:
	Listener(int sock)
		: _event_fd(sock)
	{
	}

	virtual ~Listener()
	{
		close(_event_fd);
	}

	virtual void event_on_fd(const XEvent::DispatcherPtr& dispatcher, int events);
};

class Worker: public XEvent::FdHandler, public XEvent::TaskHandler
{
public:
	Worker(int sock);
	virtual ~Worker();
	virtual void event_on_fd(const XEvent::DispatcherPtr& dispatcher, int events);
	virtual void event_on_task(const XEvent::DispatcherPtr& dispatcher);

private:
	int do_read();
	int do_write();
	int send_ack();

private:
	int _event_fd;
	int64_t _last_msec;
	int64_t _diff;
	char _addr[48];
	char _ip[40];
	uint16_t _port;
	loc_t _loc;
	struct packet_block *_block;
	unsigned int _ipos;
	unsigned int _ack_num;
};

void Listener::event_on_fd(const XEvent::DispatcherPtr& dispatcher, int events)
{
	while (true)
	{
		int fd = accept(_event_fd, NULL, NULL);
		if (fd < 0)
			break;
		Worker *worker = new Worker(fd);
		if (worker)
		{
			dispatcher->addFd(worker, fd, XEvent::READ_EVENT | XEvent::ONE_SHOT);
			dispatcher->addTask(worker, DLOGD_TIMEOUT);
		}
	}
	dispatcher->replaceFd(this, _event_fd, XEvent::READ_EVENT | XEvent::ONE_SHOT);
}

static inline struct packet_block *acquire_block()
{
	struct packet_block *b = NULL;
	pthread_mutex_lock(&mutex);
	b = (struct packet_block *)obpool_acquire(&_block_pool);
	while (b == NULL)
	{
		pthread_cond_wait(&cond, &mutex);
		b = (struct packet_block *)obpool_acquire(&_block_pool);
	}
	pthread_mutex_unlock(&mutex);
	return b;
}

static inline void release_block(struct packet_block *b)
{
	if (b)
	{
		pthread_mutex_lock(&mutex);
		obpool_release(&_block_pool, b);
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&mutex);
	}
}

static inline struct block_queue *switch_current_queue()
{
	struct block_queue *q;
	pthread_mutex_lock(&mutex);
	q = _current_queue;
	_current_queue = (q == &_queue_pair[0]) ? &_queue_pair[1] : &_queue_pair[0];
	pthread_mutex_unlock(&mutex);
	return q;
}

static inline void insert_into_current_queue(struct packet_block *b)
{
	pthread_mutex_lock(&mutex);
	TAILQ_INSERT_TAIL(_current_queue, b, link);
	pthread_mutex_unlock(&mutex);
}

Worker::Worker(int sock)
	: _event_fd(sock)
{
	int on = 1;
	setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));

	_last_msec = 0;
	_diff = 0;
	LOC_RESET(&_loc);
	_block = NULL;
	_ipos = 0;
	_ack_num = 0;

	_port = xnet_get_peer_ip_port(_event_fd, _ip);
	snprintf(_addr, sizeof(_addr), "%s+%u", _ip, _port);
	xatomic_inc(&num_client);
	xdlog(NULL, _program_name, "NODE_CONNECT", NULL, "v2 peer=%s+%u", _ip, _port);
}

Worker::~Worker()
{
	xatomic_dec(&num_client);

	if (_block)
		release_block(_block);

	close(_event_fd);
}

void Worker::event_on_fd(const XEvent::DispatcherPtr& dispatcher, int events)
{
	int r = -1;
	if (events & XEvent::READ_EVENT)
	{
		r = do_read();
	}
	else if (events & XEvent::WRITE_EVENT)
	{
		r = do_write();
	}

	if (r >= 0)
	{
		int ev = XEvent::READ_EVENT | (_ack_num > 0 ? XEvent::WRITE_EVENT : 0) | XEvent::ONE_SHOT;
		dispatcher->replaceFd(this, _event_fd, ev);
		dispatcher->replaceTask(this, DLOGD_TIMEOUT);
	}
	else
	{
		xdlog(NULL, _program_name, "NODE_DISCONNECT", NULL, "v2 peer=%s+%u", _ip, _port);
		dispatcher->removeFd(this);
		dispatcher->removeTask(this);
	}
}

void Worker::event_on_task(const XEvent::DispatcherPtr& dispatcher)
{
	xdlog(NULL, _program_name, "NODE_TIMEOUT", NULL, "v2 peer=%s+%u", _ip, _port);
	dispatcher->removeFd(this);
}

static bool _decompress(struct packet_block *dst, struct packet_block *src)
{
	int zip_len = src->pkt.size - DLOG_PACKET_HEAD_SIZE;
	int raw_len = 0;

	if (src->pkt.flag & DLOG_PACKET_FLAG_LZ4)
	{
		raw_len = LZ4_decompress_safe((char *)src->pkt.buf, (char *)dst->pkt.buf,
				zip_len, DLOG_PACKET_MAX_SIZE - DLOG_PACKET_HEAD_SIZE);
	}
	else if (src->pkt.flag & DLOG_PACKET_FLAG_LZO)
	{
		raw_len = mlzo_decompress_safe((unsigned char *)src->pkt.buf, zip_len,
				(unsigned char *)dst->pkt.buf, DLOG_PACKET_MAX_SIZE - DLOG_PACKET_HEAD_SIZE);
	}
	else
	{
		return false;
	}

	if (raw_len < 0)
	{
		return false;
	}

	memcpy(dst, src, sizeof(struct packet_block));
	dst->pkt.size = DLOG_PACKET_HEAD_SIZE + raw_len;
	dst->pkt.flag &= ~(DLOG_PACKET_FLAG_LZO | DLOG_PACKET_FLAG_LZ4);
	return true;
}

int Worker::do_read()
{
	int rc;

	LOC_BEGIN(&_loc);
	while (1)
	{
		int64_t msec, pkt_msec, diff, delta;
		int pkt_endian;
		_block = acquire_block();

		_ipos = 0;
		LOC_ANCHOR
		{
			rc = xnet_read_nonblock(_event_fd, (char *)&_block->pkt + _ipos, DLOG_PACKET_HEAD_SIZE - _ipos);
			if (rc < 0)
			{
				xlog(XLOG_NOTICE, "xnet_read_nonblock()=%d", rc);
				goto error;
			}
			_ipos += rc;
			if (_ipos < DLOG_PACKET_HEAD_SIZE)
				LOC_PAUSE(0);
		}

		pkt_endian = (_block->pkt.flag & DLOG_PACKET_FLAG_BIG_ENDIAN) ? 1 : 0;
		if (pkt_endian != _endian)
		{
			xnet_swap(&_block->pkt.size, sizeof(_block->pkt.size));
			xnet_swap(&_block->pkt.time.tv_sec, sizeof(_block->pkt.time.tv_sec));
			xnet_swap(&_block->pkt.time.tv_usec, sizeof(_block->pkt.time.tv_usec));
		}

		if (_block->pkt.size < DLOG_PACKET_HEAD_SIZE || _block->pkt.size > DLOG_PACKET_MAX_SIZE)
		{
			xlog(XLOG_ERR, "pkt.size=%d should be >= %d and <= %d", _block->pkt.size, 
					(int)DLOG_PACKET_HEAD_SIZE, (int)DLOG_PACKET_MAX_SIZE);
			goto error;
		}

		msec = dispatcher->msecRealtime();
		pkt_msec = _block->pkt.time.tv_sec * 1000 + _block->pkt.time.tv_usec / 1000;
		diff = msec - pkt_msec;
		delta = diff - _diff;
		if ((delta > -4000 && delta < 4000) || (_last_msec != 0 && llabs(diff) > llabs(_diff)))
			_diff = (_diff * 3 + diff) / 4;
		else
			_diff = diff;
		_last_msec = msec;

		msec = pkt_msec + _diff;
		_block->time.tv_sec = msec / 1000;
		_block->time.tv_usec = (msec % 1000) * 1000;
		active_time = _block->time.tv_sec;
		memcpy(_block->out_addr, _addr, sizeof(_addr));

		LOC_ANCHOR
		{
			rc = xnet_read_nonblock(_event_fd, (char *)&_block->pkt + _ipos, _block->pkt.size - _ipos);
			if (rc < 0)
			{
				xlog(XLOG_NOTICE, "xnet_read_nonblock()=%d", rc);
				goto error;
			}
			_ipos += rc;
			if (_ipos < _block->pkt.size)
				LOC_PAUSE(0);
		}
		if (send_ack() < 0)
			goto error;
	
		if (_block->pkt.version >= 2 && _block->pkt.version <= DLOG_PACKET_VERSION)
		{
			if (_block->pkt.flag & DLOG_PACKET_FLAG_IPV6) /* ipv6 binary */
			{
				xnet_ipv6_ntoa(_block->pkt.ip64, _block->pkt_ip);
			}
			else /* ip string */
			{
				if (_block->pkt.ip64[0] == 0 || _block->pkt.ip64[0] == '-')
					memcpy(_block->pkt_ip, _ip, sizeof(_ip));
				else
					memcpy(_block->pkt_ip, _block->pkt.ip64, 16);
			}

			if (_block->pkt.version == 2)
			{
				struct dlog_packet_v2 *v2 = (struct dlog_packet_v2 *)&_block->pkt;
				uint8_t flag = v2->flag;
				_block->pkt.version = 3;
				_block->pkt.flag = flag;
				_block->pkt._reserved1 = 0;
				_block->pkt._reserved2 = 0;
			}

			xatomiclong_inc(&num_block);
			if (_block->pkt.flag & (DLOG_PACKET_FLAG_LZO | DLOG_PACKET_FLAG_LZ4))
			{
				xatomiclong_inc(&num_zip_block);
				struct packet_block *b = acquire_block();
				if (_decompress(b, _block))
				{
					release_block(_block);
					_block = b;
				}
				else
				{
					xatomiclong_inc(&num_unzip_fail);
					xatomiclong_inc(&num_block_error);
				}
			}

			insert_into_current_queue(_block);
		}
		else	/* Unsupported packet version */
		{
			release_block(_block);
		}
		_block = NULL;

		xlog(XLOG_DEBUG, "Worker: read block from %s", _ip);
	}
error:
	LOC_END(&_loc);
	return -1;
}

int Worker::do_write()
{
	static char buf[1024];

	if (buf[0] == 0)
		memset(buf, 'A', sizeof(buf));
	
	while (_ack_num)
	{
		int n = _ack_num < 1024 ? _ack_num : 1024;
		int rc = xnet_write_nonblock(_event_fd, buf, n);
		if (rc <= 0)
		{
			xlog(XLOG_NOTICE, "xnet_write_nonblock()=%d, n=%d", rc, n);
			return rc;
		}
		_ack_num -= rc;
	}
	return 0;
}

int Worker::send_ack()
{
	_ack_num++;
	return do_write();
}

class MyTimer: public XEvent::TaskHandler
{
public:
	MyTimer();
	virtual void event_on_task(const XEvent::DispatcherPtr& dispatcher);

private:
	time_t _last_load;
	long _min;
	int _hour;
	int _day;
};

MyTimer::MyTimer()
{
	struct tm tm;
	_last_load = time(NULL);
	localtime_r(&_last_load, &tm);
	_min = _last_load / 60;
	_hour = tm.tm_hour;
	_day = tm.tm_mday;
}

void MyTimer::event_on_task(const XEvent::DispatcherPtr& dispatcher)
{
	int64_t current_ms = dispatcher->msecRealtime();
	time_t t = current_ms / 1000;
	_current_time = t;

	flush_timer_expire = bool(t / 2);

	if (t - _last_load >= LOAD_INTERVAL)
	{
		load_expire = true;
		_last_load = t;
	}

	if (t / 60 != _min)
	{
		_min = t / 60;
		minute_expire = true;

		struct tm tm;
		localtime_r(&t, &tm);
		if (_hour != tm.tm_hour)
		{
			_hour = tm.tm_hour;
			hour_expire = true;
			if (_day != tm.tm_mday)
			{
				_day = tm.tm_mday;
				day_expire = true;
			}
		}

		char ip[40];
		if (get_ip(ip, false) && strncmp(ip, the_ip, sizeof(the_ip)))
		{
			memcpy(the_ip, ip, sizeof(the_ip));
		}
	}

	int timeout = 1000 - current_ms % 1000;
	dispatcher->addTask(this, timeout);
}

static void _do_log_cooked_str(char *str, size_t len)
{
	if (_cab)
	{
		char *p = (char *)memchr(str, ' ', len);
		if (p)
		{
			*p++ = 0;
			len -= (p - str);
			if (len > 0)
				cabin_put(_cab, str, p, len);
		}
	}
}

void log_cooked(struct dlog_record *rec)
{
	size_t len = rec->size - 1 - DLOG_RECORD_HEAD_SIZE;
	_do_log_cooked_str(rec->str, len);
	recpool_release(rec);
}

void *logger(void *arg)
{
	const struct timespec ms10 = { 0, 1000*1000*10 };
	const struct timespec ms500 = { 0, 1000*1000*500 };
	struct block_queue *queue = NULL;
	struct packet_block *block = NULL;
	time_t last_time = 0;
	char last_record_time_str[32];

	while (run_logger || TAILQ_FIRST(_current_queue))
	{
		if (!run_logger)
			dispatcher->cancel();

		queue = switch_current_queue();
		TAILQ_FOREACH(block, queue, link)
		{
			assert((block->pkt.flag & (DLOG_PACKET_FLAG_LZO | DLOG_PACKET_FLAG_LZ4)) == 0);

			struct dlog_packet *pkt = &block->pkt;
			int pkt_endian = (pkt->flag & DLOG_PACKET_FLAG_BIG_ENDIAN) ? 1 : 0;
			char *pkt_end = (char *)pkt + pkt->size;
			char *cur = (char *)pkt + DLOG_PACKET_HEAD_SIZE;
			while (cur < pkt_end)
			{
				bool discard = false;
				struct dlog_timeval mytime;
				char rec_head[DLOG_RECORD_HEAD_SIZE + 1];
				struct dlog_record *rec = (struct dlog_record *)rec_head;
				char *recstr;

				if (cur + DLOG_RECORD_HEAD_SIZE + 1 > pkt_end)
				{
					num_record_error++;
					break;
				}

				memcpy(rec_head, cur, DLOG_RECORD_HEAD_SIZE);
				rec_head[DLOG_RECORD_HEAD_SIZE] = 0;
				recstr = cur + DLOG_RECORD_HEAD_SIZE;

				if (rec->version == 1)
				{
					struct dlog_record_v1 *v1 = (struct dlog_record_v1 *)rec;
					int locus_end = v1->identity_len + 1 + v1->tag_len + 1 + v1->locus_len;
					if (locus_end > 255)
					{
						locus_end = 255;
						num_record_error++;
					}
					rec->version = DLOG_RECORD_VERSION;
					rec->locus_end = locus_end;
					rec->port = 0;
				}

				if (rec->version != DLOG_RECORD_VERSION)
				{
					num_record_error++;
					break;
				}

				if (pkt_endian != _endian)
				{
					xnet_swap(&rec->size, sizeof(rec->size));
					xnet_swap(&rec->pid, sizeof(rec->pid));
					xnet_swap(&rec->time.tv_sec, sizeof(rec->time.tv_sec));
					xnet_swap(&rec->time.tv_usec, sizeof(rec->time.tv_usec));
				}

				if (rec->size < (DLOG_RECORD_HEAD_SIZE + rec->locus_end + 2)
					|| (cur + rec->size) > pkt_end)
				{
					num_record_error++;
					break;
				}

				size_t orig_size = rec->size;
				if (rec->size > DLOG_RECORD_MAX_SIZE)
				{
					rec->size = DLOG_RECORD_MAX_SIZE;
					rec->truncated = true;
				}
				cur[rec->size - 1] = 0;
				cur += orig_size;

				if (rec->type == DLOG_TYPE_COOKED)
				{
					size_t len = rec->size - 1 - DLOG_RECORD_HEAD_SIZE;
					_do_log_cooked_str(recstr, len);
					// TODO
					// continue;
				}
			
				dlog_timesub(&rec->time, &pkt->time, &mytime);
				dlog_timeadd(&block->time, &mytime, &rec->time);
				if (rec->time.tv_sec != last_time)
				{
					last_time = rec->time.tv_sec;
					get_time_str(last_time, last_record_time_str);
				}

				if (banlist)
				{
					if (banlist_check(banlist, block->pkt_ip, rec))
					{
						num_banned++;
						continue;
					}
				}

				if (plugin && rec->type != DLOG_TYPE_COOKED)
				{
					int rc = plugin_filter(plugin, last_record_time_str, block->pkt_ip, rec, recstr);
					plugin_run++;
					if (rc > 0)
					{
						plugin_discard++;
						discard = true;
					}
					else if (rc < 0)
						plugin_error++;
				}

				if (!discard)
				{
					int rc;
					if (rec->type == DLOG_TYPE_SYS && 
						(!strncmp(recstr, "dlogd ", 6) || !strncmp(recstr, "dstsd ", 6)))
					{
						char *p = (char *)memrchr(recstr, ' ', rec->locus_end);
						int len = p ? p - recstr : 0;
						rc = fprintf(_logfile_fp, "%c%s %s %d+%d %.*s %s %s%s\n",
							TYPECODE[rec->type], last_record_time_str,
							block->pkt_ip, rec->pid, rec->port,
							len, recstr, block->out_addr,
							&recstr[rec->locus_end + 1], rec->truncated ? "\a ..." : "");
					}
					else
					{
						rc = fprintf(_logfile_fp, "%c%s %s %d+%d %s%s\n",
							TYPECODE[rec->type], last_record_time_str,
							block->pkt_ip, rec->pid, rec->port,
							recstr, rec->truncated ? "\a ..." : "");
					}

					if (rc < 0)
					{
						fprintf(stderr, "fprintf() failed, pathname=%s\nexit(1)", _log_pathname);
						exit(1);
					}
					_logfile_size += rc;
					total_size += rc;
					++num_record;

					if (_logfile_size > _logfile_switch && _current_time > _logfile_time)
						_switch_log_file();
				}
			}
		}

		if (flush_timer_expire)
		{
			flush_timer_expire = false;
			fflush(_logfile_fp);
			if (_cab)
				cabin_flush(_cab);

			if (load_expire)
			{
				load_expire = false;
				load_banlist();
				load_plugin();
			}
		}

		if (minute_expire)
		{
			minute_expire = false;

			char active_ts[32], plugin_ts[32];

			if (active_time)
				get_time_str(active_time, active_ts);
			else
				strcpy(active_ts, "-");

			if (plugin_mtime)
				get_time_str(plugin_mtime, plugin_ts);
			else
				strcpy(plugin_ts, "-");

			struct timeval utv;
			gettimeofday(&utv, NULL);

			double self_cpu = 9999.9;
			struct rusage usage;
			if (getrusage(RUSAGE_SELF, &usage) == 0)
			{
				int64_t x = usec_diff(&usage.ru_utime, &_usage.ru_utime)
					 + usec_diff(&usage.ru_stime, &_usage.ru_stime);
				int64_t y = usec_diff(&utv, &_utv);
				self_cpu = 100.0 * x / (y > 100000 ? y : 100000);
				_utv = utv;
				_usage = usage;
			}

			int euid = geteuid();
			if (_euid != euid)
			{
				_euid = euid;
				if (unix_uid2user(_euid, _euser, sizeof(_euser)) < 0)
					snprintf(_euser, sizeof(_euser), "%d", _euid);
			}

			uint64_t freq = get_cpu_frequency(0);

			xdlog(NULL, _program_name, "THROB", NULL, "v2 version=%s start=%s active=%s client=%d"
					" info=euser:%s,MHz:%u,cpu:%.1f%%"
					" record=get:%llu,error:%llu"
					" block=pool:%lu,get:%ld,error:%ld,zip:%ld,unzip_error:%ld"
					" plugin=file:%s,md5:%s,mtime:%s,status:%c,run:%llu,discard:%llu,error:%llu",
				DLOG_VERSION,
				start_time_str, active_ts, xatomic_get(&num_client),
				_euser, (int)(freq / 1000000), self_cpu,
				num_record, num_record_error,
				_block_pool.num_limit,
				xatomiclong_get(&num_block), xatomiclong_get(&num_block_error),
				xatomiclong_get(&num_zip_block), xatomiclong_get(&num_unzip_fail),
				plugin_file, plugin_md5, plugin_ts, (plugin ? '#' : plugin_mtime ? '*' : '-'),
				plugin_run, plugin_discard, plugin_error);

			if (hour_expire)
			{
				hour_expire = false;

				/* TODO */

				if (day_expire)
				{
					day_expire = false;

					char *p = strrchr(_log_pathname, '.');
					if (p)
					{
						char time_str[32];
						++p;
						get_time_str(time(NULL), time_str);
						time_str[6] = 0;
						if (memcmp(p, time_str, 6))
							_switch_log_file();
					}

					if (_cab)
					{
						cabin_t *cb = _cab;
						_cab = cabin_create(_log_dir);
						cabin_destroy(cb);
					}
				}
			}
		}

		if (TAILQ_FIRST(queue))
		{
			while ((block = TAILQ_FIRST(queue)) != NULL)
			{
				TAILQ_REMOVE(queue, block, link);
				release_block(block);
			}
			nanosleep(&ms10, NULL);
		}
		else
		{
			nanosleep(&ms500, NULL);
		}
	}

	dispatcher->cancel();

	return NULL;
}

static int dw_callback(const dirwalk_item_t *item, void *ctx)
{
        if (item->level < 1)
        {
                if (item->isdir && strlen(item->name) == 6)
                        return 1;
        }
        else if (item->level == 1)
        {
                const char *pathname = item->path;
                const char *filename = item->name;
                int prefix_len = sizeof(LOGFILE_PREFIX) - 1;
                if (strncmp(filename, LOGFILE_PREFIX, prefix_len) == 0)
                {
                        const char *dir_end = filename - 1;
                        const char *dir_start = (char*)memrchr(pathname, '/', dir_end - pathname);
                        if (dir_start && dir_end - dir_start == 7 && memcmp(dir_start + 1, &filename[prefix_len], 6) == 0)
                        {
                                do_compress(item->path);
                        }
                }
        }

	return 0;
}

void handle_old_logs()
{
	dirwalk_run(_log_dir, dw_callback, NULL);
}

static void sig_handler(int sig)
{
	run_logger = false;
}

static void cleanup()
{
	if (_logfile_fp)
		fclose(_logfile_fp);

	do_compress(_log_pathname);
}

static int _dlog_callback(void *state, const char *str, size_t length)
{
	static int pid = getpid();

	if (_logfile_fp)
	{
		char time_str[32];
		time_t t = dispatcher->msecRealtime() / 1000;
		get_time_str(t, time_str);
		fprintf(_logfile_fp, "%c%s %s %d+0 %.*s\n", TYPECODE[DLOG_TYPE_SYS], time_str, the_ip, pid, (int)length, str);
	}
	return 0;
}

static void usage(const char *prog)
{
	const char *p = strrchr(prog, '/');
	if (!p)
		p = prog;
	else
		p++;
	
	fprintf(stderr, "Usage: %s [options] <logdir>\n", p);
	fprintf(stderr, 
"  -D                 Do not run as daemon\n"
"  -l plugin          plugin file, default %s.plugin\n"
"  -s file_size       max size of file in mega bytes\n"
"  -P port            port number, default %d\n"
"  -u user            process user, default the user of logdir, or nobody\n"
"  -x xlog_level      xlog_level, default 0\n"
"  -g errlog_file     errlog file, default /tmp/%s.log\n"
		, _program_name, DLOG_CENTER_PORT, _program_name);
	exit(1);
}

int main(int argc, char **argv)
{
	char *prog = argv[0];
	struct rlimit rlim;
	pthread_t thr;
	int sock = -1;
	int optend;
	char pathbuf[PATH_MAX];
	bool daemon = true;
	const char *pg_file = NULL;
	const char *ban_file = NULL;
	const char *user = NULL;
	char errlog_file_buf[64];
	const char *errlog_file = errlog_file_buf;

	XError::how = 0;
	strcpy(pathbuf, prog);
	cstr_ncopy(_program_name, sizeof(_program_name), basename(pathbuf));
	snprintf(errlog_file_buf, sizeof(errlog_file_buf), "/tmp/%s.log", _program_name);

	OPT_BEGIN(argc, argv, &optend)
	{
	case 'l':
		pg_file = OPT_EARG(usage(prog));
		break;
	case 'b':
		ban_file = OPT_EARG(usage(prog));
		break;
	case 'D':
		daemon = false;
		break;
	case 'P':
		port = strtoul(OPT_EARG(usage(prog)), NULL, 0);
		break;
	case 'u':
		user = OPT_EARG(usage(prog));
		break;
	case 's':
		_logfile_switch = atoi(OPT_EARG(usage(prog))) * 1024 * 1024 - LOGFILE_RESERVE;
		if (_logfile_switch < LOGFILE_SWITCH_MIN)
			_logfile_switch = LOGFILE_SWITCH_MIN;
		break;
	case 'x':
		xlog_level = atoi(OPT_EARG(usage(prog)));
		break;
	case 'g':
		errlog_file = OPT_EARG(usage(prog));
		break;
	default:
		usage(prog);
	} OPT_END();

	if (argc < optend + 1)
		usage(prog);

	init_global();

	strcpy(pathbuf, prog);
	path_realpath(_program_dir, NULL, dirname(pathbuf));

	if (!path_realpath(_log_dir, NULL, argv[optend]))
	{
		fprintf(stderr, "can't get the real path of `%s`, errno=%d, %m\n", argv[optend], errno);
		goto error;
	}

	if (daemon && daemon_init() < 0)
	{
		fprintf(stderr, "daemon_init() failed\n");
		goto error;
	}
	umask(02);

	rlim.rlim_cur = 65536;
	rlim.rlim_max = 65536;
	setrlimit(RLIMIT_NOFILE, &rlim);

	if (user && user[0])
	{
		if (unix_set_user_group(user, NULL) < 0)
		{
			fprintf(stderr, "unix_set_user_group() failed, user=%s\n", user);
			return 1;
		}
	}
	else
	{
		struct stat st;
		if (stat(_log_dir, &st) == 0 && st.st_uid != 0)
		{
			setgid(st.st_gid);
			setuid(st.st_uid);
		}
		else
		{
			unix_set_user_group("nobody", NULL);
		}
	}

	sock = xnet_tcp_listen(NULL, port, 256);
	if (sock < 0)
	{
		fprintf(stderr, "xnet_tcp_listen() failed, port=%d\n", port);
		goto error;
	}

	handle_old_logs();

	dispatcher = XEvent::Dispatcher::create(NULL);

	pthread_attr_t thr_attr;
	pthread_attr_init(&thr_attr);
	pthread_attr_setstacksize(&thr_attr, STACK_SIZE);

	if (pthread_create(&thr, &thr_attr, logger, NULL) != 0)
	{
		fprintf(stderr, "pthread_create() failed\n");
		goto error;
	}

	_logfile_fp = _open_log_file(_log_dir, &_logfile_time);
	if (!_logfile_fp)
	{
		fprintf(stderr, "_open_log_file() failed, pathname=%s, errno=%d, %m\n", _log_pathname, errno);
		goto error;
	}

	get_ip(the_ip, false);
	dlog_set(_program_name, 0);
	dlog_set_callback(_dlog_callback, NULL);

	atexit(cleanup);

	signal(SIGXFSZ, SIG_IGN);	/* in case the plugin exceeds the file size limit (2G). */
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	if (ban_file)
		path_realpath(banlist_file, NULL, ban_file);
	else
	{
		snprintf(banlist_file, sizeof(banlist_file), "%s/%s.ban", _program_dir, _program_name);
	}

	load_banlist();

	if (pg_file)
	{
		path_realpath(plugin_file, NULL, pg_file);
	}
	else
	{
		snprintf(plugin_file, sizeof(plugin_file), "%s/%s.plugin", _program_dir, _program_name);
	}

//	_cab = cabin_create(_log_dir);		XXX
	luadlog_init(log_cooked, _program_name);
	load_plugin();

	dispatcher->addFd(new Listener(sock), sock, XEvent::READ_EVENT | XEvent::ONE_SHOT);
	dispatcher->addTask(new MyTimer(), 0);

	if (daemon)
		daemon_redirect_stderr(errlog_file);

	get_time_str(time(NULL), start_time_str);

	dispatcher->setThreadPool(4, 32, STACK_SIZE);
	dispatcher->start();

	pthread_join(thr, NULL);
	return 0;
error:
	return 1;
}

