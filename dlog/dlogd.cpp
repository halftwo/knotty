#include "dlog_imp.h"
#include "plugin.h"
#include "luadlog.h"
#include "recpool.h"
#include "misc.h"
#include "netaddr.h"
#include "top.h"
#include "diskspace.h"
#include "xslib/xstr.h"
#include "xslib/cxxstr.h"
#include "xslib/opt.h"
#include "xslib/rdtsc.h"
#include "xslib/ScopeGuard.h"
#include "xslib/md5.h"
#include "xslib/hex.h"
#include "xslib/unixfs.h"
#include "xslib/unix_user.h"
#include "xslib/xatomic.h"
#include "xslib/cpu.h"
#include "xslib/path.h"
#include "xslib/cirqueue.h"
#include "xslib/daemon.h"
#include "xslib/xlog.h"
#include "xslib/xnet.h"
#include "xslib/XEvent.h"
#include "xslib/loc.h"
#include "xslib/queue.h"
#include "xslib/obpool.h"
#include "xslib/lz4.h"
#include "xslib/cstr.h"
#include "xslib/iobuf.h"
#include <libgen.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <utmp.h>
#include <stdint.h>
#include <dirent.h>
#include <string.h>
#include <netinet/in.h>
#include <stddef.h>
#include <signal.h>
#include <errno.h>
#include <map>
#include <sstream>


#define MAX_NUM_BLOCK	512

#define BLOCK_SIZE	(DLOG_PACKET_MAX_SIZE + offsetof(struct block, pkt))

struct block
{
	TAILQ_ENTRY(block) link;
	struct dlog_packet pkt;
};

static char _program_name[32];

static int current_center_revision;

static XEvent::DispatcherPtr dispatcher;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static obpool_t _bpl = OBPOOL_INITIALIZER(BLOCK_SIZE);
static struct block *_b;
static TAILQ_HEAD(block_queue, block) _q;
static int _endian;

static cirqueue_t *_rec_cq;

static bool ip_external = false;
static bool is_ipv6 = false;
static char the_ip[40];
static uint8_t the_ip64[16];

static char start_time_str[24];
static bool run_logger = true;
static bool dlog_on = true;

static bool to_flush;
static bool to_load;

/* worker threads variables, need lock */
static xatomic_t num_client;
static time_t last_record_time;
static xatomiclong_t num_record_overflow;
static time_t record_overflow_time;

/* log thread variables */
static xatomiclong_t num_record_bad;
static unsigned long long num_record_take;
static unsigned long long num_record_cooked;
static unsigned long long num_block_overflow;
static unsigned long long num_block_send;
static time_t block_overflow_time;

static plugin_t *plugin;
static char plugin_file[PATH_MAX];
static time_t plugin_mtime;
static char plugin_md5[33] = "-";
static unsigned long long plugin_run;
static unsigned long long plugin_error;
static unsigned long long plugin_discard;

/* sender thread variables */
static int center_sock = -1;
static unsigned long dlog_idle;
static time_t last_connect_time;
static unsigned long num_reconnect;
static unsigned int compress_threshold = 256;
static unsigned long long num_compressed_block;
static unsigned long long num_compress_failure;
static unsigned long long num_incompressible;


void init_global()
{
	TAILQ_INIT(&_q);
	_endian = xnet_get_endian();
	_rec_cq = cirqueue_create(1024*32);
}

static bool get_ip(char ip[], bool external)
{
	bool fail = external ? (!xnet_get_external_ip(ip) && !xnet_get_internal_ip(ip))
			: (!xnet_get_internal_ip(ip) && !xnet_get_external_ip(ip));

	if (fail)
		strcpy(ip, "-");

	return !fail;
}

static void make_time_str(time_t t, char buf[])
{
	if (t)
	{
		get_time_str(t, buf);
	}
	else
	{
		buf[0] = '-';
		buf[1] = 0;
	}
}

static inline void copy_and_replace(char *dst, const char *src, size_t size)
{
	char *p;
	size_t len = size;

	memcpy(dst, src, size);
	while (len > 0 && (p = (char *)memchr(dst, '\n', len)) != NULL)
	{
		ssize_t k;
		if (p > dst && p[-1] == '\r')
			p[-1] = '\x1d';
		*p++ = '\x1a';
		k = p - dst;
		dst += k;
		len -= k;
	}
}

static void do_log(struct dlog_record *rec)
{
	char *p;

	++num_record_take;
	if (rec->type == DLOG_TYPE_COOKED)
	{
		++num_record_cooked;
	}
	else if (plugin && rec->version == DLOG_RECORD_VERSION && rec->type == DLOG_TYPE_RAW)
	{
		int rc = plugin_filter(plugin, NULL, the_ip, rec, NULL);
		plugin_run++;

		if (rc > 0)
		{
			plugin_discard++;
			return;
		}

		if (rc < 0)
			plugin_error++;
	}

	if (_b == NULL || _b->pkt.size + rec->size > DLOG_PACKET_MAX_SIZE)
	{
		pthread_mutex_lock(&mutex);
		if (_b)
			TAILQ_INSERT_TAIL(&_q, _b, link);

		if (_bpl.num_acquire < MAX_NUM_BLOCK)
			_b = (struct block *)obpool_acquire(&_bpl);
		else
		{
			/* Discard the first block. */
			_b = TAILQ_FIRST(&_q);
			TAILQ_REMOVE(&_q, _b, link);
			++num_block_overflow;
			block_overflow_time = dispatcher->msecRealtime() / 1000;
		}
		pthread_mutex_unlock(&mutex);

		_b->pkt.size = DLOG_PACKET_HEAD_SIZE;
		_b->pkt.flag = 0;
		_b->pkt._reserved1 = 0;
		_b->pkt._reserved2 = 0;
	}

	p = (char *)&_b->pkt + _b->pkt.size;
	memcpy(p, rec, DLOG_RECORD_HEAD_SIZE);
	copy_and_replace(p + DLOG_RECORD_HEAD_SIZE, rec->str, rec->size - DLOG_RECORD_HEAD_SIZE);
	_b->pkt.size += rec->size;
}

static void log_it(struct dlog_record *rec, bool block)
{
	int64_t ms = dispatcher->msecRealtime();
	time_t sec = ms / 1000;
	rec->time.tv_sec = sec;
	rec->time.tv_usec = (ms % 1000) * 1000;

	if (!block)
		last_record_time = sec;

	if (!cirqueue_put(_rec_cq, rec, block))
	{
		recpool_release(rec);
		xatomiclong_inc(&num_record_overflow);
		record_overflow_time = sec;
	}
}

static void log_cooked(struct dlog_record *rec)
{
	rec->type = DLOG_TYPE_COOKED;
	log_it(rec, true);
}

static void log_sys(struct dlog_record *rec, bool block)
{
	rec->type = DLOG_TYPE_SYS;
	log_it(rec, block);
}

static void log_alert(struct dlog_record *rec, bool block)
{
	rec->type = DLOG_TYPE_ALERT;
	log_it(rec, block);
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

			struct dlog_record *rec = recpool_acquire();
			dlog_make(rec, NULL, _program_name, "PLUGIN_NOFILE", NULL, "v1 %s", "");
			log_sys(rec, false);
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

		struct dlog_record *rec = recpool_acquire();
		dlog_make(rec, NULL, _program_name, "PLUGIN_RELOAD", NULL, "v1 status=%s", plugin ? "success" : "fail");
		log_sys(rec, false);
	}
}

static void *log_thread(void *arg)
{
	struct timespec wait;
	size_t num = 0;

	while (1)
	{
		struct dlog_record *recs[128];

		if (num == 0)
		{
			clock_gettime(CLOCK_REALTIME, &wait);
			wait.tv_sec += 1;
		}

		if (to_load)
		{
			to_load = false;
			load_plugin();
		}

		num = cirqueue_timed_getv(_rec_cq, (void **)(void*)recs, 128, &wait);
		if (num)
		{
			for (size_t i = 0; i < num; ++i)
			{
				do_log(recs[i]);
			}
			recpool_release_all(recs, num);
		}

		if (to_flush && _b && _b->pkt.size > DLOG_PACKET_HEAD_SIZE && !TAILQ_FIRST(&_q))
		{
			to_flush = false;
			pthread_mutex_lock(&mutex);
			TAILQ_INSERT_TAIL(&_q, _b, link);
			pthread_mutex_unlock(&mutex);
			_b = NULL;
		}
	}
	return NULL;
}

static void ipstr_to_ip64(const char *ipstr, uint8_t ip64[], bool& is6)
{
	if (strchr(ipstr, ':'))
	{
		is6 = true;
		uint8_t buf[16];
		xnet_ipv6_aton(ipstr, buf);
		memcpy(ip64, buf, 16);
	}
	else	
	{
		ip64[15] = 0;
		is6 = false;
		memcpy(ip64, ipstr, 15);
	}
}

static void *sender_thread(void *arg)
{
	struct block_queue todo;
	
	current_center_revision = dlog_center_revision;

	while (run_logger || TAILQ_FIRST(&_q))
	{
		if (!run_logger)
			dispatcher->cancel();

		char buf[MAX_NUM_BLOCK];
		struct block *b;
		size_t num_todo = 0;
		size_t num_done = 0;

		if (current_center_revision != dlog_center_revision)
		{
			xlog(XLOG_INFO, "%s.plugin changed", _program_name);
			current_center_revision = dlog_center_revision;
			if (center_sock != -1)
			{
				close(center_sock);
				center_sock = -1;
			}
		}

		if (!dlog_on)
		{
			if (!run_logger)
				return NULL;

			if (center_sock != -1)
			{
				close(center_sock);
				center_sock = -1;
			}

			sleep(1);
			continue;
		}

		if (!TAILQ_FIRST(&_q))
		{
			sleep(1);
			++dlog_idle;
			if (dlog_idle % 60 == 0 && center_sock != -1)
			{
				int n = recv(center_sock, buf, 1, MSG_PEEK | MSG_DONTWAIT);
				if (n == 0 || (n < 0 && errno != EAGAIN && errno != EINTR))
				{
					xlog(XLOG_NOTICE, "recv()=%d errno=%d", n, errno);
					close(center_sock);
					center_sock = -1;
				}
			}
			continue;
		}

		if (center_sock < 0)
		{
			if (time(NULL) == last_connect_time)
			{
				sleep(1);
			}

			++num_reconnect;
			center_sock = xnet_tcp_connect(dlog_center_host, dlog_center_port);
			xlog(XLOG_DEBUG, "xnet_tcp_conenct()=%d", center_sock);
			if (center_sock < 0)
			{
				if (!run_logger)
					return NULL;
				sleep(1);
				continue;
			}
			xnet_set_nonblock(center_sock);
			last_connect_time = time(NULL);

			char ip[40];
			if (xnet_get_sock_ip_port(center_sock, ip))
			{
				if (strcmp(ip, "127.0.0.1") == 0 || strcmp(ip, "::1") == 0)
				{
					get_ip(ip, ip_external);
				}

				if (ip[0] && strcmp(ip, the_ip) != 0)
				{
					memcpy(the_ip, ip, sizeof(the_ip));
					ipstr_to_ip64(the_ip, the_ip64, is_ipv6);
				}
			}
		}

		TAILQ_INIT(&todo);

		pthread_mutex_lock(&mutex);
		while ((b = TAILQ_FIRST(&_q)) != NULL)
		{
			TAILQ_REMOVE(&_q, b, link);
			TAILQ_INSERT_TAIL(&todo, b, link);

			if (++num_todo > MAX_NUM_BLOCK / 16)
				break;
		}
		pthread_mutex_unlock(&mutex);
	
		TAILQ_FOREACH(b, &todo, link)
		{
			struct dlog_packet *pkt = NULL;
			unsigned char pktbuf[DLOG_PACKET_MAX_SIZE + (DLOG_PACKET_MAX_SIZE / 255) + 16 + 4096];
			bool compressed = false;

			if (compress_threshold && b->pkt.size >= compress_threshold)
			{
				pkt = (struct dlog_packet *)pktbuf;
				int in_len = b->pkt.size - DLOG_PACKET_HEAD_SIZE;
				int out_len = LZ4_compress(b->pkt.buf, pkt->buf, in_len);

				if (out_len < 0)
				{
					++num_compress_failure;
				}
				else if (out_len > in_len * 0.95)
				{
					++num_incompressible;
				}
				else
				{
					++num_compressed_block;
					compressed = true;
					memcpy(pkt, &b->pkt, DLOG_PACKET_HEAD_SIZE);
					pkt->size = DLOG_PACKET_HEAD_SIZE + out_len;
					pkt->flag |= DLOG_PACKET_FLAG_LZ4;
				}
			}

			if (!compressed)
			{
				pkt = &b->pkt;
			}

			int64_t current_ms = dispatcher->msecRealtime();
			pkt->time.tv_sec = current_ms / 1000;
			pkt->time.tv_usec = current_ms % 1000 * 1000;
			pkt->version = DLOG_PACKET_VERSION;
			memcpy(pkt->ip64, the_ip64, sizeof(pkt->ip64));
			if (_endian)
				pkt->flag |= DLOG_PACKET_FLAG_BIG_ENDIAN;
			if (is_ipv6)
				pkt->flag |= DLOG_PACKET_FLAG_IPV6;

			size_t size = pkt->size;
			int timeout = 60*1000;
			if (xnet_write_resid(center_sock, pkt, &size, &timeout) < 0)
			{
				xlog(XLOG_NOTICE, "xnet_write_resid() failed: pkt.size=%d left=%zd", pkt->size, size);
				close(center_sock);
				center_sock = -1;
				break;
			}
			else
			{
				xlog(XLOG_DEBUG, "pkt sent: pkt.size=%d left=%zd", pkt->size, size);
				++num_done;
			}
		}

		size_t num_ack = 0;
		if (center_sock != -1)
		{
			int timeout = 60*1000;
			num_ack = num_done;
			if (xnet_read_resid(center_sock, buf, &num_done, &timeout) < 0)
			{
				xlog(XLOG_NOTICE, "xnet_read_resid() failed: expect=%zd left=%zd", num_ack, num_done);
				close(center_sock);
				center_sock = -1;
				num_ack -= num_done;
			}
		}

		num_block_send += num_ack;
		pthread_mutex_lock(&mutex);
		while ((b = TAILQ_FIRST(&todo)) != NULL && num_ack-- > 0)
		{
			TAILQ_REMOVE(&todo, b, link);
			obpool_release(&_bpl, b);
		}
		while ((b = TAILQ_LAST(&todo, block_queue)) != NULL)
		{
			TAILQ_REMOVE(&todo, b, link);
			TAILQ_INSERT_HEAD(&_q, b, link);
		}
		pthread_mutex_unlock(&mutex);
		dlog_idle = 0;
	}

	dispatcher->cancel();

	fprintf(stderr, "thread sender() exit.\n");
	return NULL;
}

struct CpuStat
{
	int64_t user;
	int64_t nice;
	int64_t system;
	int64_t idle;
	int64_t iowait;
	int64_t irq;
	int64_t softirq;
	int64_t stolen;
	int64_t guest;
public:
	int64_t total()
	{
		return user + nice + system + idle + iowait + irq + softirq + stolen + guest;
	}
};

struct NetStat
{
	int64_t rb;
	int64_t rpkt;
	int64_t rerr;
	int64_t rdrop;
	int64_t rfifo;
	int64_t rframe;
	int64_t tb;
	int64_t tpkt;
	int64_t terr;
	int64_t tdrop;
	int64_t tfifo;
	int64_t tcarrier;
	int64_t tcoll;
};

struct DiskStat
{
	int64_t rio;
	int64_t rmerge;
	int64_t rsect;
	int64_t ruse;
	int64_t wio;
	int64_t wmerge;
	int64_t wsect;
	int64_t wuse;
};

class MyTimer: public XEvent::TaskHandler
{
public:
	MyTimer(const char *ip);
	virtual ~MyTimer();
	virtual void event_on_task(const XEvent::DispatcherPtr& dispatcher);
private:
	uid_t _euid;
	char _euser[32];
	char _saved_ip[40];
	int _core_count;
	int _sec;
	int _minute;
	unsigned char *_buf;
	size_t _buf_size;
	struct rusage _usage;
	int64_t _last_mono_msec;
	int64_t _ctotal;
	std::string _uname;
	CpuStat _cstat;
	std::map<std::string, NetStat> _nstat;
	std::map<std::string, DiskStat> _dstat;
};

MyTimer::MyTimer(const char *ip)
	: _sec(0), _minute(0), _buf(NULL), _buf_size(0), _last_mono_msec(0)
{
	_euid = geteuid();
	_euser[0] = 0;
	if (unix_uid2user(_euid, _euser, sizeof(_euser)) < 0)
		snprintf(_euser, sizeof(_euser), "%d", _euid);
	memcpy(_saved_ip, ip, sizeof(_saved_ip));
	_core_count = cpu_count();
	_ctotal = 0;
	memset(&_cstat, 0, sizeof(_cstat));
	getrusage(RUSAGE_SELF, &_usage);

	struct utsname uts;
	if (uname(&uts) == 0)
	{
		cstr_replace(NULL, uts.sysname, ' ', '^');
		cstr_replace(NULL, uts.machine, ' ', '^');
		cstr_replace(NULL, uts.release, ' ', '^');
		cstr_replace(NULL, uts.version, ' ', '^');
		_uname = format_string("%s,%s,%s,%s", uts.sysname, uts.machine, uts.release, uts.version);
	}
}

MyTimer::~MyTimer()
{
	free(_buf);
}

static int64_t usec_diff(const struct timeval* t1, const struct timeval* t2)
{
	int64_t x1 = t1->tv_sec * 1000000 + t1->tv_usec;
	int64_t x2 = t2->tv_sec * 1000000 + t2->tv_usec;
	return x1 - x2;
}

#define FP(x)	(((x)<0.01)?0:1), (x)

void MyTimer::event_on_task(const XEvent::DispatcherPtr& dispatcher)
{
	++_sec;
	if (_sec % 10 == 0)
	{
		to_load = true;
	}
	to_flush = true;

	int64_t current_ms = dispatcher->msecRealtime();
	time_t sec = current_ms / 1000;
	int m = sec / 60;
	if (_minute != m)
	{
		_minute = m;

		struct dlog_record *rec;
		char current_ts[32];
		char active_ts[32], plugin_ts[32], block_overflow_ts[32], record_overflow_ts[32];

		get_time_str(sec, current_ts);
		make_time_str(last_record_time, active_ts);
		make_time_str(plugin_mtime, plugin_ts);
		make_time_str(block_overflow_time, block_overflow_ts);
		make_time_str(record_overflow_time, record_overflow_ts);

		uid_t euid = geteuid();
		if (euid != _euid)
		{
			_euid = euid;
			if (unix_uid2user(_euid, _euser, sizeof(_euser)) < 0)
				snprintf(_euser, sizeof(_euser), "%d", _euid);
		}

		int64_t mono_msec = dispatcher->msecMonotonic();
		double interval = (mono_msec - _last_mono_msec) / 1000.0;
		if (interval < 1.0)
			interval = 1.0;
		_last_mono_msec = mono_msec;

		double self_cpu = 9999.9;
		struct rusage usage;
		if (getrusage(RUSAGE_SELF, &usage) == 0)
		{
			int64_t x = usec_diff(&usage.ru_utime, &_usage.ru_utime)
				 + usec_diff(&usage.ru_stime, &_usage.ru_stime);
			self_cpu = 100.0 * ((double)x / 1000000.0) / interval;
			_usage = usage;
		}

		uint64_t freq = get_cpu_frequency(0);

		rec = recpool_acquire();
		dlog_make(rec, NULL, _program_name, "THROB", NULL, "v1 version=%s start=%s now=%s active=%s client=%d"
					" info=euser:%s,MHz:%u,cpu:%.1f%%"
					" record=bad:%ld,take:%llu,cooked:%llu,overflow:%ld,overflow_time:%s"
					" block=send:%llu,zip:%llu,overflow:%llu,overflow_time:%s"
					" plugin=file:%s,md5:%s,mtime:%s,status:%c,run:%llu,discard:%llu,error:%llu",
				DLOG_VERSION,
				start_time_str, current_ts, active_ts, xatomic_get(&num_client),
				_euser, (int)(freq / 1000000), self_cpu,
				xatomiclong_get(&num_record_bad), 
				num_record_take, num_record_cooked,
				xatomiclong_get(&num_record_overflow), record_overflow_ts,
				num_block_send, num_compressed_block, 
				num_block_overflow, block_overflow_ts,
				plugin_file, plugin_md5, plugin_ts, (plugin ? '#' : plugin_mtime ? '*' : '-'),
				plugin_run, plugin_discard, plugin_error);
		log_sys(rec, true);

		ssize_t n;
		xstr_t s, line;
		bool first;
		iobuf_t ob;

		std::ostringstream io_os;
		ob = make_iobuf(io_os, NULL, 0);
		n = unixfs_get_content("/proc/diskstats", &_buf, &_buf_size);
		xstr_init(&s, _buf, n > 0 ? n : 0);
		first = true;
		while (xstr_delimit_char(&s, '\n', &line))
		{
			DiskStat ds;
			xstr_t major, minor, name;

			xstr_token_cstr(&line, " \t", &major);
			xstr_token_cstr(&line, " \t", &minor);
			xstr_delimit_char(&line, ' ', &name);
			ds.rio = xstr_to_llong(&line, &line, 0);
			ds.rmerge = xstr_to_llong(&line, &line, 0);
			ds.rsect = xstr_to_llong(&line, &line, 0);
			ds.ruse = xstr_to_llong(&line, &line, 0);
			ds.wio = xstr_to_llong(&line, &line, 0);
			ds.wmerge = xstr_to_llong(&line, &line, 0);
			ds.wsect = xstr_to_llong(&line, &line, 0);
			ds.wuse = xstr_to_llong(&line, &line, 0);
			if (line.len == 0 || (ds.rmerge == 0 && ds.rsect == 0 && ds.wmerge == 0 && ds.wsect == 0))
				continue;

			std::string dev_name = make_string(name);
			std::map<std::string, DiskStat>::iterator iter = _dstat.find(dev_name);
			if (iter == _dstat.end())
			{
				DiskStat null_ds;
				memset(&null_ds, 0, sizeof(null_ds));
				iter = _dstat.insert(_dstat.end(), std::make_pair(dev_name, null_ds));
			}

			const DiskStat& ds0 = iter->second;

			double rkb = (ds.rsect - ds0.rsect) * (512.0 / 1024.0) / interval;	// in kB
			double wkb = (ds.wsect - ds0.wsect) * (512.0 / 1024.0) / interval;	// in kB
			double rtps = (ds.rio - ds0.rio) / interval;
			double wtps = (ds.wio - ds0.wio) / interval;

			if (first)
				first = !first;
			else
				iobuf_putc(&ob, ',');

			iobuf_printf(&ob, "%.*s:%.*f/%.*f~%.*f/%.*f", XSTR_P(&name),
				FP(rkb), FP(rtps), FP(wkb), FP(wtps));

			iter->second = ds;
		}

		std::ostringstream net_os;
		ob = make_iobuf(net_os, NULL, 0);
		n = unixfs_get_content("/proc/net/dev", &_buf, &_buf_size);
		xstr_init(&s, _buf, n > 0 ? n : 0);
		first = true;
		while (xstr_delimit_char(&s, '\n', &line))
		{
			NetStat ns;
			xstr_t name;

			xstr_delimit_char(&line, ':', &name);
			if (line.len == 0)
				continue;

			xstr_trim(&name);

			ns.rb = xstr_to_llong(&line, &line, 0);
			ns.rpkt = xstr_to_llong(&line, &line, 0);
			ns.rerr = xstr_to_llong(&line, &line, 0);
			ns.rdrop = xstr_to_llong(&line, &line, 0);
			ns.rfifo = xstr_to_llong(&line, &line, 0);
			ns.rframe = xstr_to_llong(&line, &line, 0);
				xstr_to_llong(&line, &line, 0);		/* discard */
				xstr_to_llong(&line, &line, 0);		/* discard */
			ns.tb = xstr_to_llong(&line, &line, 0);
			ns.tpkt = xstr_to_llong(&line, &line, 0);
			ns.terr = xstr_to_llong(&line, &line, 0);
			ns.tdrop = xstr_to_llong(&line, &line, 0);
			ns.tfifo = xstr_to_llong(&line, &line, 0);
			ns.tcoll = xstr_to_llong(&line, &line, 0);
			ns.tcarrier = xstr_to_llong(&line, &line, 0);

			if (ns.rb == 0 && ns.tb == 0)
				continue;

			std::string dev_name = make_string(name);
			std::map<std::string, NetStat>::iterator iter = _nstat.find(dev_name);
			if (iter == _nstat.end())
			{
				NetStat null_ns;
				memset(&null_ns, 0, sizeof(null_ns));
				iter = _nstat.insert(_nstat.end(), std::make_pair(dev_name, null_ns));
			}

			const NetStat& ns0 = iter->second;

			double rkb = (ns.rb - ns0.rb) / 1024.0 / interval;
			double rpkt = (ns.rpkt - ns0.rpkt) / interval;
			char rstatus[8];
			int ridx = 0;
			if (ns.rerr - ns0.rerr)
				rstatus[ridx++] = 'e';
			if (ns.rdrop - ns0.rdrop)
				rstatus[ridx++] = 'd';
			if (ns.rfifo - ns0.rfifo)
				rstatus[ridx++] = 'o';
			if (ns.rframe - ns0.rframe)
				rstatus[ridx++] = 'f';
			rstatus[ridx] = 0;

			double tkb = (ns.tb - ns0.tb) / 1024.0 / interval;
			double tpkt = (ns.tpkt - ns0.tpkt) / interval;
			char tstatus[8];
			int tidx = 0;
			if (ns.terr - ns0.terr)
				tstatus[tidx++] = 'e';
			if (ns.tdrop - ns0.tdrop)
				tstatus[tidx++] = 'd';
			if (ns.tfifo - ns0.tfifo)
				tstatus[tidx++] = 'o';
			if (ns.tcarrier - ns0.tcarrier)
				tstatus[tidx++] = 'c';
			if (ns.tcoll - ns0.tcoll)
				tstatus[tidx++] = 'C';
			tstatus[tidx] = 0;

			if (first)
				first = !first;
			else
				iobuf_putc(&ob, ',');

			iobuf_printf(&ob, "%.*s:%.*f/%.*f^%c%s~%.*f/%.*f^%c%s", XSTR_P(&name),
				FP(rkb), FP(rpkt), (ridx ? '*' : '#'), rstatus,
				FP(tkb), FP(tpkt), (tidx ? '*' : '#'), tstatus);

			iter->second = ns;
		}

		char loadavg[64];
		n = unixfs_get_content("/proc/loadavg", &_buf, &_buf_size);
		xstr_init(&s, _buf, n > 0 ? n : 0);
		xstr_trim(&s);
		xstr_replace_char(&s, ' ', ',');
		xstr_copy_to(&s, loadavg, sizeof(loadavg));

		long memTotal = 0, memFree = 0, memBuffer = 0, memCache = 0;
		long swapTotal = 0, swapFree = 0;
		n = unixfs_get_content("/proc/meminfo", &_buf, &_buf_size);
		xstr_init(&s, _buf, n > 0 ? n : 0);
		while (xstr_delimit_char(&s, '\n', &line))
		{
			xstr_t key;
			xstr_delimit_char(&line, ':', &key);
			long x = xstr_to_long(&line, NULL, 0);		/* in k */
			if (xstr_equal_cstr(&key, "MemTotal"))
				memTotal = x / 1024;
			else if (xstr_equal_cstr(&key, "MemFree"))
				memFree = x / 1024;
			else if (xstr_equal_cstr(&key, "Buffers"))
				memBuffer = (x + 512) / 1024;
			else if (xstr_equal_cstr(&key, "Cached"))
				memCache = (x + 512) / 1024;
			else if (xstr_equal_cstr(&key, "SwapTotal"))
				swapTotal = x / 1024;
			else if (xstr_equal_cstr(&key, "SwapFree"))
				swapFree = x / 1024;
		}

		n = unixfs_get_content("/proc/stat", &_buf, &_buf_size);
		xstr_init(&s, _buf, n > 0 ? n : 0);
		CpuStat cstat = _cstat;
		while (xstr_delimit_char(&s, '\n', &line))
		{
			xstr_t name;
			xstr_token_cstr(&line, " \t", &name);
			if (xstr_equal_cstr(&name, "cpu"))
			{
				cstat.user = xstr_to_llong(&line, &line, 0);
				cstat.nice = xstr_to_llong(&line, &line, 0);
				cstat.system = xstr_to_llong(&line, &line, 0);
				cstat.idle = xstr_to_llong(&line, &line, 0);
				cstat.iowait = xstr_to_llong(&line, &line, 0);
				cstat.irq = xstr_to_llong(&line, &line, 0);
				cstat.softirq = xstr_to_llong(&line, &line, 0);
				cstat.stolen = xstr_to_llong(&line, &line, 0);
				cstat.guest = xstr_to_llong(&line, &line, 0);
				break;
			}
		}

		std::ostringstream cpu_os;
		ob = make_iobuf(cpu_os, NULL, 0);
		{
			int64_t total = cstat.total();
			double factor = (total > _ctotal) ? (100.0 / (total - _ctotal)) : 0.0;

			#define CALC(NAME)	(factor * (cstat.NAME - _cstat.NAME))
			double us = CALC(user);
			double sy = CALC(system);
			double ni = CALC(nice);
			double id = CALC(idle);
			double wa = CALC(iowait);
			double hi = CALC(irq);
			double si = CALC(softirq);
			double st = CALC(stolen);
			#undef CALC

			iobuf_printf(&ob, "%.*f%%us~%.*f%%sy~%.*f%%ni~%.*f%%id~%.*f%%wa~%.*f%%hi~%.*f%%si~%.*f%%st",
					FP(us), FP(sy), FP(ni), FP(id), FP(wa), FP(hi), FP(si), FP(st));

			_ctotal = total;
			_cstat = cstat;
		}

		n = unixfs_get_content("/proc/uptime", &_buf, &_buf_size);
		xstr_init(&s, _buf, n > 0 ? n : 0);
		long uptime = xstr_to_long(&s, NULL, 0);
		int up_day = uptime / 86400;
		int up_hour = (uptime % 86400) / 3600;
		int up_min = (uptime % 3600) / 60;

		// Check who is logined currently.
		std::ostringstream who_os;
		ob = make_iobuf(who_os, NULL, 0);
		char path[256] = "/dev/";
		size_t plen = strlen(path);
		size_t user_count = 0;
		setutent();
		for (struct utmp *ut; ((ut = getutent()) != NULL); )
		{
			if (ut->ut_type != USER_PROCESS)
				continue;

			int login_sec = sec - ut->ut_tv.tv_sec;
			int idle_sec = -1;
			struct stat st;
			strcpy(path + plen, ut->ut_line);
			if (stat(path, &st) == 0)
				idle_sec = sec - st.st_atime;

			if (user_count++)
				iobuf_putc(&ob, ',');
			iobuf_printf(&ob, "%s:%s~%d~%d~%s", ut->ut_user, ut->ut_line, idle_sec, login_sec, ut->ut_host);
		}
		endutent();

		rec = recpool_acquire();
		dlog_make(rec, NULL, _program_name, "STATS", NULL, "v1 up=%d-%02d:%02d user=%zd load=%s"
					" cpu=%dcore:%s"
					" mem=t:%ldM,u:%ldM,b:%ldM,c:%ldM,f:%ldM"
					" swap=t:%ldM,u:%ldM,f:%ldM"
					" net=%s io=%s os=%s",
				up_day, up_hour, up_min, user_count, loadavg,
				_core_count, cpu_os.str().c_str(),
				memTotal, (memTotal - memFree - memBuffer - memCache),
				memBuffer, memCache, memFree,
				swapTotal, (swapTotal - swapFree), swapFree,
				net_os.str().c_str(),
				io_os.str().c_str(),
				_uname.c_str());
		log_sys(rec, false);


		if (user_count)
		{
			rec = recpool_acquire();
			dlog_make(rec, NULL, _program_name, "WHO", NULL, "v1 count=%zd who=%s",
				user_count, who_os.str().c_str());
			log_sys(rec, false);
		}

		// Check disk space
		std::ostringstream disk_os;
		diskspace(disk_os);
		rec = recpool_acquire();
		dlog_make(rec, NULL, _program_name, "DISK", NULL, "v1 disk=%s",
			disk_os.str().c_str());
		log_sys(rec, false);

		// Check network
		std::ostringstream address_os;
		if (_euid)
			seteuid(0);
		int naddr = get_netaddr(address_os);
		if (_euid)
			seteuid(_euid);

		if (naddr > 0)
		{
			rec = recpool_acquire();
			dlog_make(rec, NULL, _program_name, "NET", NULL, "v2 interface=%s",
				address_os.str().c_str());
			log_sys(rec, false);
		}

		// Check top processes, by cpu usage and memory usage
		std::ostringstream top_cos, top_mos;
		if (_euid)
			seteuid(0);
		top_processes(top_cos, top_mos);
		if (_euid)
			seteuid(_euid);

		std::string top_cpu = top_cos.str();
		if (!top_cpu.empty())
		{
			rec = recpool_acquire();
			dlog_make(rec, NULL, _program_name, "TOP_CPU", NULL, "v1 %s", top_cpu.c_str());
			log_sys(rec, false);
		}

		std::string top_mem = top_mos.str();
		if (!top_mem.empty())
		{
			rec = recpool_acquire();
			dlog_make(rec, NULL, _program_name, "TOP_MEM", NULL, "v1 %s", top_mem.c_str());
			log_sys(rec, false);
		}


		// Check if ip changed.
		if (strcmp(_saved_ip, the_ip) != 0)
		{
			rec = recpool_acquire();
			dlog_make(rec, NULL, _program_name, "IP_CHANGE", NULL, "v1 old=%s new=%s", _saved_ip, the_ip);
			log_sys(rec, false);
			memcpy(_saved_ip, the_ip, sizeof(the_ip));
		}

		current_ms = dispatcher->msecRealtime();
	}

	// Make this function run at every 0.3 second
	dispatcher->addTask(this, 1000 - (current_ms - 300) % 1000);
}

#undef FP

void check_record(struct dlog_record *rec)
{
	if (((char *)rec)[rec->size - 1] != '\0')
	{
		throw XERROR_FMT(XError, "Record is not terminated with nil byte '\\0'");
	}

	if (rec->version == 0)
	{
		struct dlog_record_v0 *v0 = (struct dlog_record_v0 *)rec;
		int32_t pid = v0->pid;
		struct dlog_timeval the_time = v0->time;

		char *p1 = strchr(rec->str, ' ');
		char *p2 = p1 ? strchr(p1 + 1, ' ') : NULL;
		char *p3 = p2 ? strchr(p2 + 1, ' ') : NULL;
		int locus_end = p3 ? p3 - rec->str : rec->size - 2;
		if (locus_end > 255)
			throw XERROR_FMT(XError, "The total length (%d) of (IDENTITY TAG LOCUS) are too long", locus_end);

		rec->version = DLOG_RECORD_VERSION;
		rec->type = DLOG_TYPE_RAW;
		rec->locus_end = locus_end;
		rec->pid = pid;
		rec->time = the_time;
		
	}
	else if (rec->version == 1)
	{
		struct dlog_record_v1 *v1 = (struct dlog_record_v1 *)rec;
		int locus_end = v1->identity_len + 1 + v1->tag_len + 1 + v1->locus_len;
		if (locus_end > 255)
			throw XERROR_FMT(XError, "The total length (%d) of (IDENTITY TAG LOCUS) are too long", locus_end);

		rec->version = DLOG_RECORD_VERSION;
		rec->locus_end = locus_end;
	}

	if (rec->version != DLOG_RECORD_VERSION)
		throw XERROR_FMT(XError, "Unknown record version (%d)", rec->version);

	if (rec->type != DLOG_TYPE_RAW)
		throw XERROR_FMT(XError, "Record type (%d) is not DLOG_TYPE_RAW", rec->type);

	if (rec->locus_end == 0)
		throw XERROR_FMT(XError, "locus_end should not be 0");

	if (rec->size < DLOG_RECORD_HEAD_SIZE + rec->locus_end + 2)
		throw XERROR_FMT(XError, "Invalid record size (%d), locus_end=%d", rec->size, rec->locus_end);
}

class DlogUdpWorker: public XEvent::FdHandler
{
	int _event_fd;
	struct dlog_record *_rec;

	void do_read();
public:
	DlogUdpWorker(int sock)
		: _event_fd(sock), _rec(NULL)
	{
	}

	virtual ~DlogUdpWorker()
	{
		close(_event_fd);
		if (_rec)
			recpool_release(_rec);
	}

	virtual void event_on_fd(const XEvent::DispatcherPtr& dispatcher, int events);
};

void DlogUdpWorker::event_on_fd(const XEvent::DispatcherPtr& dispatcher, int events)
{
	if (events & XEvent::READ_EVENT)
	{
		do_read();
	}

	dispatcher->replaceFd(this, _event_fd, XEvent::READ_EVENT | XEvent::ONE_SHOT);
}

void DlogUdpWorker::do_read()
{
	while (1)
	{
		xnet_inet_sockaddr_t addr;
		socklen_t addrlen = sizeof(addr);
		if (!_rec)
			_rec = recpool_acquire();

		int port = 0;
		try {
			ssize_t len = xnet_recvfrom_nonblock(_event_fd, _rec, DLOG_RECORD_MAX_SIZE, 0, 
					(struct sockaddr *)&addr, &addrlen);
			if (len < 0)
				throw XERROR_FMT(XError, "xnet_recvfrom_nonblock()=%zd, errno=%d, %m", len, errno);
			else if (len == 0)
				break;

			port = (addr.family == AF_INET6) ? addr.a6.sin6_port : addr.a4.sin_port;

			if (len < (ssize_t)DLOG_RECORD_HEAD_SIZE + 4)
				throw XERROR_FMT(XError, "Size of recvfrom()ed data too small (%zd)", len);

			if (len < DLOG_RECORD_MAX_SIZE)
			{
				if (len != _rec->size)
					throw XERROR_FMT(XError, "record->size (%d) not equal to recvfrom()=%zd", _rec->size, len);
			}
			else if (_rec->size < DLOG_RECORD_MAX_SIZE)
			{
				throw XERROR_FMT(XError, "record->size (%d) not match with recvfrom()=%zd", _rec->size, len);
			}

			if (_rec->size > DLOG_RECORD_MAX_SIZE)
			{
				_rec->size = DLOG_RECORD_MAX_SIZE;
				_rec->truncated = true;
				((char *)_rec)[_rec->size - 1] = '\0';
			}

			check_record(_rec);
			_rec->port = port;
			log_it(_rec, false);
			_rec = NULL;
		}
		catch (std::exception& ex)
		{
			xatomiclong_inc(&num_record_bad);
			dlog_make(_rec, NULL, _program_name, "CLIENT_ERROR", NULL, "v2 peer=udp+%d ex=%s", port, ex.what());
			_rec->port = 0;
			log_alert(_rec, false);
			_rec = NULL;
		}
	}
}

class DlogListener: public XEvent::FdHandler
{
	int _event_fd;
public:
	DlogListener(int sock)
		: _event_fd(sock)
	{
	}

	virtual ~DlogListener()
	{
		close(_event_fd);
	}

	virtual void event_on_fd(const XEvent::DispatcherPtr& dispatcher, int events);
};


class DlogWorker: public XEvent::FdHandler
{
public:
	DlogWorker(int sock);
	virtual ~DlogWorker();
	virtual void event_on_fd(const XEvent::DispatcherPtr& dispatcher, int events);

private:
	int do_read();

private:
	int _event_fd;
	uint16_t _port;
	loc_t _loc;
	size_t _pos;
	iobuf_t _ib;
	union
	{
		struct dlog_record *_rec;
		char *_buf;
	};
};

void DlogListener::event_on_fd(const XEvent::DispatcherPtr& dispatcher, int events)
{
	while (true)
	{
		int fd = accept(_event_fd, NULL, NULL);
		if (fd < 0)
		{
			xlog(XLOG_NOTICE, "accept()=%d errno=%d %m", fd, errno);
			break;
		}
		dispatcher->addFd(new DlogWorker(fd), fd, XEvent::READ_EVENT | XEvent::ONE_SHOT);
	}
	dispatcher->replaceFd(this, _event_fd, XEvent::READ_EVENT | XEvent::ONE_SHOT);
}

DlogWorker::DlogWorker(int sock)
	: _event_fd(sock)
{
	char ip[40];
	LOC_RESET(&_loc);
	_port = xnet_get_peer_ip_port(_event_fd, ip);
	_pos = 0;
	_ib = make_iobuf(_event_fd, NULL, 8192);
//	iobuf_bstate_enable(&_ib);	/* This may only be helpful for a slow link. */
	_rec = NULL;
	xatomic_inc(&num_client);
}

DlogWorker::~DlogWorker()
{
	xatomic_dec(&num_client);
	iobuf_finish(&_ib);
	if (_rec)
		recpool_release(_rec);
	close(_event_fd);
}

void DlogWorker::event_on_fd(const XEvent::DispatcherPtr& dispatcher, int events)
{
	int r = -1;
	if (events & XEvent::READ_EVENT)
	{
		r = do_read();
	}

	if (r >= 0)
		dispatcher->replaceFd(this, _event_fd, XEvent::READ_EVENT | XEvent::ONE_SHOT);
	else
		dispatcher->removeFd(this);
}
		
int DlogWorker::do_read()
{
	try
	{
		int rc;

		iobuf_bstate_clear(&_ib);
		LOC_BEGIN(&_loc);
		while (1)
		{
			_rec = recpool_acquire();
			_pos = 0;
			LOC_ANCHOR
			{
				rc = iobuf_read(&_ib, _buf + _pos, DLOG_RECORD_HEAD_SIZE - _pos);
				if (rc < 0)
				{
					if (rc == -2)
						goto finished;
					throw XERROR_FMT(XSyscallError, "errno=%d", errno);
				}
				_pos += rc;
				if (_pos < DLOG_RECORD_HEAD_SIZE)
					LOC_PAUSE(0);
			}

			if (_rec->size < DLOG_RECORD_HEAD_SIZE + 4)
				throw XERROR_FMT(XError, "Invalid record size (%d)", _rec->size);

			LOC_ANCHOR
			{
				size_t size = (_rec->size <= DLOG_RECORD_MAX_SIZE) ? _rec->size : DLOG_RECORD_MAX_SIZE;
				rc = iobuf_read(&_ib, _buf + _pos, size - _pos);
				if (rc < 0)
					throw XERROR_FMT(XSyscallError, "errno=%d", errno);
				_pos += rc;
				if (_pos < size)
					LOC_PAUSE(0);
			}

			if (_rec->size > DLOG_RECORD_MAX_SIZE)
			{
				LOC_ANCHOR
				{
					rc = iobuf_skip(&_ib, _rec->size - _pos);
					if (rc < 0)
						throw XERROR_FMT(XSyscallError, "errno=%d", errno);
					_pos += rc;
					if (_pos < _rec->size)
						LOC_PAUSE(0);
				}
				_rec->size = DLOG_RECORD_MAX_SIZE;
				_rec->truncated = true;
				((char *)_rec)[_rec->size - 1] = '\0';
			}

			_rec->port = _port;
			check_record(_rec);
			log_it(_rec, false);
			_rec = NULL;
		}
	finished:
		LOC_END(&_loc);
	}
	catch (std::exception& ex)
	{
		LOC_HALT(&_loc);
		xatomiclong_inc(&num_record_bad);

		struct dlog_record *rec = recpool_acquire();
		dlog_make(rec, NULL, _program_name, "CLIENT_ERROR", NULL, "v2 peer=tcp+%d ex=%s", _port, ex.what());
		log_alert(rec, false);
	}

	return -1;
}


static void sig_handler(int sig)
{
	run_logger = false;
}

void usage(const char *prog)
{
	const char *p = strrchr(prog, '/');
	if (!p)
		p = prog;
	else
		p++;
	
	fprintf(stderr, "Usage: %s [options]\n", p);
	fprintf(stderr, 
"  -D                 Do not run as daemon\n"
"  -E                 Use external ip\n"
"  -l plugin          plugin file, default %s.plugin\n"
"  -H center_host     dlog_center host, default localhost\n"
"  -P center_port     dlog_center port, default %d\n"
"  -u user            process user, default nobody\n"
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
	int udp6 = -1, tcp6 = -1, udp4 = -1, tcp4 = -1;
	char pathbuf[PATH_MAX];
	char prog_dirname[PATH_MAX];
	bool daemon = true;
	const char *pg_file = NULL;
	const char *user = NULL;
	char errlog_file_buf[64];
	const char *errlog_file = errlog_file_buf;
	
	XError::how = 0;
	strcpy(pathbuf, prog);
	cstr_ncopy(_program_name, sizeof(_program_name), basename(pathbuf));
	snprintf(errlog_file_buf, sizeof(errlog_file_buf), "/tmp/%s.log", _program_name);

	OPT_BEGIN(argc, argv, NULL)
	{
	case 'l':
		pg_file = OPT_EARG(usage(prog));
		break;
	case 'E':
		ip_external = true;
		break;
	case 'D':
		daemon = false;
		break;
	case 'H':
		cstr_ncopy(dlog_center_host, CENTER_HOST_SIZE, OPT_EARG(usage(prog)));
		break;
	case 'P':
		dlog_center_port = strtoul(OPT_EARG(usage(prog)), NULL, 0);
		break;
	case 'u':
		user = OPT_EARG(usage(prog));
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

	init_global();

	strcpy(pathbuf, prog);
	path_realpath(prog_dirname, NULL, dirname(strdupa(prog)));

	if (daemon && daemon_init() < 0)
	{
		fprintf(stderr, "daemon_init() failed\n");
		return 1;
	}

	if (strcmp(_program_name, "dlogd") == 0)
	{
		rlim.rlim_cur = 8192;
		rlim.rlim_max = 8192;
		setrlimit(RLIMIT_NOFILE, &rlim);

		if (user && user[0])
		{
			// Set the effective uid.
			// We may seteuid() to root when do some privileged reading,
			// and seteuid() back to the unprivileged user.
			if (unix_set_euser_egroup(user, NULL) < 0)
			{
				fprintf(stderr, "unix_set_user_group() failed, user=%s\n", user);
				return 1;
			}
		}
		else
		{
			unix_set_euser_egroup("nobody", NULL);
		}

		udp6 = xnet_udp_bind("::1", DLOGD_PORT);
		tcp6 = xnet_tcp_listen("::1", DLOGD_PORT, 256);

		udp4 = xnet_udp_bind("127.0.0.1", DLOGD_PORT);
		if (udp4 < 0)
		{
			fprintf(stderr, "xnet_udp_bind() failed, addr=udp+%s+%u, errno=%d\n", "127.0.0.1", DLOGD_PORT, errno);
			exit(1);
		}

		tcp4 = xnet_tcp_listen("127.0.0.1", DLOGD_PORT, 256);
		if (tcp4 < 0)
		{
			fprintf(stderr, "xnet_tcp_listen() failed, addr=tcp+%s+%u, errno=%d\n", "127.0.0.1", DLOGD_PORT, errno);
			exit(1);
		}
	}
	else if (strcmp(_program_name, "dstsd") == 0)
	{
		char pidfile[256];
		snprintf(pidfile, sizeof(pidfile), "/var/run/%s.pid", _program_name);
		if (daemon_lock_pidfile(pidfile) < 0)
		{
			fprintf(stderr, "lock pidfile failed, pidfile=%s\n", pidfile);
			exit(1);
		}
	}
	else
	{
		fprintf(stderr, "This program can only be runned as dlogd or dstsd\n");
		exit(1);
	}

	if (!dlog_center_host[0])
		cstr_ncopy(dlog_center_host, CENTER_HOST_SIZE, "localhost");

	dispatcher = XEvent::Dispatcher::create(NULL);

	if (pthread_create(&thr, NULL, log_thread, NULL) != 0)
	{
		fprintf(stderr, "pthread_create() failed\n");
		exit(1);
	}

	if (pthread_create(&thr, NULL, sender_thread, NULL) != 0)
	{
		fprintf(stderr, "pthread_create() failed\n");
		exit(1);
	}

	signal(SIGXFSZ, SIG_IGN);	/* in case the plugin exceeds the file size limit (2G). */
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	if (pg_file)
		path_realpath(plugin_file, NULL, pg_file);
	else
		snprintf(plugin_file, sizeof(plugin_file), "%s/%s.plugin", prog_dirname, _program_name);

	get_ip(the_ip, ip_external);
	ipstr_to_ip64(the_ip, the_ip64, is_ipv6);

	luadlog_init(&log_cooked, _program_name);
	load_plugin();

	dispatcher->addTask(new MyTimer(the_ip), 0);

	if (udp6 >= 0)
		dispatcher->addFd(new DlogUdpWorker(udp6), udp6, XEvent::READ_EVENT | XEvent::ONE_SHOT);

	if (tcp6 >= 0)
		dispatcher->addFd(new DlogListener(tcp6), tcp6, XEvent::READ_EVENT | XEvent::ONE_SHOT);

	if (udp4 >= 0)
		dispatcher->addFd(new DlogUdpWorker(udp4), udp4, XEvent::READ_EVENT | XEvent::ONE_SHOT);

	if (tcp4 >= 0)
		dispatcher->addFd(new DlogListener(tcp4), tcp4, XEvent::READ_EVENT | XEvent::ONE_SHOT);

	if (daemon)
		daemon_redirect_stderr(errlog_file);

	get_time_str(time(NULL), start_time_str);

	{
		struct dlog_record *rec = recpool_acquire();
		dlog_make(rec, NULL, _program_name, "IPV6", NULL, "v1 udp=%savailable tcp=%savailable",
				udp6 < 0 ? "un" : "",
				tcp6 < 0 ? "un" : "");
		log_sys(rec, false);
	}

	dispatcher->setThreadPool(2, 4, 256*1024);
	dispatcher->start();

	pthread_join(thr, NULL);
	return 0;
}

