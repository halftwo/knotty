#include "top.h"
#include "xslib/XHeap.h"
#include "xslib/queue.h"
#include "xslib/hdict.h"
#include "xslib/ostk.h"
#include "xslib/iobuf.h"
#include "xslib/unixfs.h"
#include "xslib/unix_user.h"
#include "xslib/ScopeGuard.h"
#include <sys/types.h>
#include <sys/time.h>
#include <linux/param.h>	/* for HZ */
#include <unistd.h>
#include <dirent.h>

static const int PAGE_SIZE = sysconf(_SC_PAGESIZE);

#define PROC_NUM	10
#define CMDLINE_MAXLEN	512

#define byte2k(x)	(((x) + 1023) >> 10)
#define page2k(x)	((x) * (PAGE_SIZE / 1024))

/*
   procname:pid~user~nthread~nfd~cpu~mem~realtime-utime-stime
   php:5204~root~5th~3fd~52.2%~71m/285m~9000.20-5.11-1.11
 */

typedef struct proc_info_t proc_info_t;
typedef struct manager_t manager_t;

struct proc_info_t
{
	pid_t pid;
	char *name;
	int state;
	int pri, nice, threads;
	unsigned long vsize, rss, shared;    /* in k */
	unsigned long utime, stime;
	unsigned long start_time;
	double pcpu;

	int fd_num;
	char *user;
	char *cmdline;
};


struct manager_t
{
	ostk_t *ostk;
	hdict_t *hdict;
};

static manager_t *man0;
static struct timeval tv0;
static char the_procfs[32] = "/proc/";

static manager_t *man_create()
{
	ostk_t *ostk = ostk_create(0);
	manager_t *man = (manager_t *)ostk_hold(ostk, sizeof(*man));
	man->ostk = ostk;
	man->hdict = hdict_create(1024, sizeof(pid_t), sizeof(proc_info_t));
	return man;
}

static void man_destroy(manager_t *man)
{
	if (man)
	{
		hdict_destroy(man->hdict);
		ostk_destroy(man->ostk);
	}
}


struct CpuGreater
{
	bool operator()(const proc_info_t* a, const proc_info_t* b)
	{
		return a->pcpu > b->pcpu;
	}
};

struct MemGreater
{
	bool operator()(const proc_info_t* a, const proc_info_t* b)
	{
		return a->rss > b->rss;
	}
};


static int proc_owner(pid_t pid, char *user, int size)
{
	char pathname[1024];
	struct stat st;
	sprintf(pathname, "%s%d", the_procfs, pid);
	if (stat(pathname, &st) == 0)
	{
		int r = unix_uid2user(st.st_uid, user, size); 
		if (r <= 0)
		{
			r = snprintf(user, size, "%d", st.st_uid);
		}
		return r;
	}

	user[0] = '-';
	user[1] = 0;
	return 1;
}

static int get_proc_fd_number(pid_t pid)
{
	char pathname[1024];
	sprintf(pathname, "%s%d/fd", the_procfs, (int)pid);

	DIR *dir = opendir(pathname);
	if (!dir)
		return -1;
	ON_BLOCK_EXIT(closedir, dir);

	struct dirent *ent;
	int num = 0;
	while ((ent = readdir(dir)) != NULL)
	{
		if (!isdigit(ent->d_name[0]))
			continue;
		++num;
	}
	return num;
}

static void fill_it(ostk_t *ostk, std::vector<proc_info_t*>& procs)
{
	size_t size = procs.size();

	for (size_t i = 0; i < size; ++i)
	{
		proc_info_t *proc = procs[i];

		if (proc->fd_num < 0)
			proc->fd_num = get_proc_fd_number(proc->pid);

		if (proc->user == NULL)
		{
			char user[64];
			proc_owner(proc->pid, user, sizeof(user));
			proc->user = ostk_strdup(ostk, user);
		}

		if (proc->cmdline == NULL)
		{
			static unsigned char *cmdline = NULL;
			static size_t cmdsize = 0;
			char pathname[1024];
			sprintf(pathname, "%s%d/cmdline", the_procfs, proc->pid);
			ssize_t n = unixfs_get_content(pathname, &cmdline, &cmdsize);
			xstr_t xs = XSTR_INIT((unsigned char *)cmdline, n > 0 ? n : 0);
			xstr_rstrip_char(&xs, '\0');
			if (xs.len > CMDLINE_MAXLEN)
			{
				xs.len = CMDLINE_MAXLEN;
				cmdline[xs.len - 1] = '.';
				cmdline[xs.len - 2] = '.';
				cmdline[xs.len - 3] = '.';
			}
			xstr_replace_char(&xs, '\0', '^');
			xstr_replace_char(&xs, ' ', '~');
			proc->cmdline = ostk_strdup_xstr(ostk, &xs);
		}
	}
}

static void print_it(std::ostream& oss, double uptime, const std::vector<proc_info_t*>& procs)
{
	iobuf_t ob = make_iobuf(oss, NULL, 0);
	size_t size = procs.size();

	for (size_t i = 0; i < size; ++i)
	{
		proc_info_t *proc = procs[i];
		iobuf_printf(&ob, "TOP%02zd=%s;%d~%s~%dth~%dfd~%.1f%%~%luM/%luM~%.2f-%.2f-%.2f;%s ",
			i + 1, proc->name, proc->pid, proc->user, proc->threads, proc->fd_num,
			proc->pcpu * 100.0, (proc->rss + 1023) / 1024, (proc->vsize + 1023) / 1024,
			uptime - (double)proc->start_time / HZ, (double)proc->utime / HZ, (double)proc->stime / HZ,
			proc->cmdline);
	}
}

void top_processes(std::ostream& os_cpu, std::ostream& os_mem)
{
	DIR *dir;
	struct dirent *ent;
	char pathname[1024];
	ssize_t n;
	double uptime;
	double elapsed_jiffies = 1;
	struct timeval tv;
	unsigned char *content = NULL;
	size_t content_size = 0;
	XHeap<proc_info_t*, CpuGreater> cpuheap(PROC_NUM);
	XHeap<proc_info_t*, MemGreater> memheap(PROC_NUM);

	ON_BLOCK_EXIT(free_pptr<unsigned char>, &content);

	manager_t *man = man_create();
	if (!man)
		return;

	dir = opendir(the_procfs);
	if (!dir)
		return;
	ON_BLOCK_EXIT(closedir, dir);

	sprintf(pathname, "%suptime", the_procfs);
	n = unixfs_get_content(pathname, &content, &content_size);
	uptime = n > 0 ? strtod((char *)content, NULL) : -1;

	gettimeofday(&tv, NULL);

	if (man0)
	{
		struct timeval delta;
		timersub(&tv, &tv0, &delta);
		elapsed_jiffies = (long)(delta.tv_sec * HZ + ((delta.tv_usec * HZ + (HZ - 1) * 1000000 / HZ) / 1000000));
		if (elapsed_jiffies < 1)
			elapsed_jiffies = 1;
	}

	while ((ent = readdir(dir)) != NULL)
	{
		xstr_t xs, token;
		proc_info_t *proc;

		if (!isdigit(ent->d_name[0]))
			continue;

		pid_t pid = atoi(ent->d_name);

		sprintf(pathname, "%s%d/stat", the_procfs, pid);
		n = unixfs_get_content(pathname, &content, &content_size);
		if (n <= 0)
			continue;

		xstr_init(&xs, content, n);
		proc = (proc_info_t *)hdict_insert(man->hdict, &pid, NULL);
		proc->pid = pid;
		proc->fd_num = -1;
		xstr_token_space(&xs, NULL);		// pid

		ssize_t pos;
		if (xstr_char_equal(&xs, 0, '(') && (pos = xstr_find_char(&xs, 1, ')')) > 0)
		{
			token = xstr_slice(&xs, 1, pos);
			xstr_advance(&xs, pos + 1);
			xstr_replace_char(&token, ' ', '~');
		}
		else
		{
			xstr_token_space(&xs, &token);
		}
		xstr_replace_char(&token, ';', '^');
		proc->name = (char *)ostk_copyz(man->ostk, token.data, token.len);

		xstr_token_space(&xs, NULL);		// state
		xstr_token_space(&xs, NULL);		// ppid
		xstr_token_space(&xs, NULL);		// pgrp
		xstr_token_space(&xs, NULL);		// session
		xstr_token_space(&xs, NULL);		// tty_nr
		xstr_token_space(&xs, NULL);		// tpgid
		xstr_token_space(&xs, NULL);		// flags
		xstr_token_space(&xs, NULL);		// minflt
		xstr_token_space(&xs, NULL);		// cminflt
		xstr_token_space(&xs, NULL);		// majflt
		xstr_token_space(&xs, NULL);		// cmajflt
		proc->utime = xstr_to_ulong(&xs, &xs, 10);
		proc->stime = xstr_to_ulong(&xs, &xs, 10);
		xstr_token_space(&xs, NULL);		// cutime
		xstr_token_space(&xs, NULL);		// cstime
		proc->pri = xstr_to_long(&xs, &xs, 10);
		proc->nice = xstr_to_long(&xs, &xs, 10);
		proc->threads = xstr_to_long(&xs, &xs, 10);
		xstr_token_space(&xs, NULL);		// itrealvalue
		proc->start_time = xstr_to_ulong(&xs, &xs, 10);
		proc->vsize = byte2k(xstr_to_ulong(&xs, &xs, 10));
		proc->rss = page2k(xstr_to_ulong(&xs, &xs, 10));

		if (proc->rss)
		{
			memheap.push(proc);
		}

		bool done = false;
		if (man0)
		{
			proc_info_t *proc0 = (proc_info_t *)hdict_find(man0->hdict, &pid);
			if (proc0 && proc0->start_time == proc->start_time)
			{
				done = true;
				unsigned long t = (proc->utime + proc->stime - proc0->utime - proc0->stime);
				if (t)
				{
					proc->pcpu = (double)t / elapsed_jiffies;
					cpuheap.push(proc);
				}
			}
		}

		if (!done)
		{
			double delta_jiffies = uptime * HZ - proc->start_time;
			if (delta_jiffies >= HZ)
			{
				done = true;
				unsigned long t = (proc->utime + proc->stime);
				if (t)
				{
					proc->pcpu = (double)t / delta_jiffies;
					cpuheap.push(proc);
				}
			}
		}
	}

	std::vector<proc_info_t*> procs;

	cpuheap.sort_and_take(procs);
	if (!procs.empty())
	{
		fill_it(man->ostk, procs);
		print_it(os_cpu, uptime, procs);
	}

	memheap.sort_and_take(procs);
	if (!procs.empty())
	{
		fill_it(man->ostk, procs);
		print_it(os_mem, uptime, procs);
	}

	man_destroy(man0);
	man0 = man;
	tv0 = tv;
}

