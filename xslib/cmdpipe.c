#include "cmdpipe.h"
#include "xnet.h"
#include <time.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <poll.h>
#include <errno.h>


#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: cmdpipe.c,v 1.4 2015/10/26 07:03:04 gremlin Exp $";
#endif

#define MAX_NUM_ARG	128
#define BSIZE_MIN	512
#define BSIZE_DFT	(1024*16)


int cmdpipe_execl(struct cmdpipe_info *cpi, const char *mode, const char *path, const char *arg, ...)
{
	char *argv[MAX_NUM_ARG];

	argv[0] = (char *)arg;
	if (arg)
	{
		int i;
		va_list ap;

		va_start(ap, arg);
		for (i = 1; i < MAX_NUM_ARG; ++i)
		{
			argv[i] = va_arg(ap, char *);	
			if (!argv[i])
				break;
		}
		va_end(ap);

		if (i == MAX_NUM_ARG)
			return -1;
	}

	return cmdpipe_execve(cpi, mode, path, argv, NULL);
}

struct execve_info
{
	const char *path;
	char **argv;
	char **envp;
};

static void execve_procedure(void *ctx)
{
	struct execve_info *info = (struct execve_info *)ctx;
	if (execve(info->path, info->argv, info->envp) == -1)
		exit(127);
}

int cmdpipe_execve(struct cmdpipe_info *cpi, const char *mode, const char *path, char *const argv[], char *const envp[])
{
	struct execve_info info;
	info.path = path;
	info.argv = (char **)argv;
	info.envp = (char **)envp;
	return cmdpipe_exec(cpi, mode, execve_procedure, &info);
}

int cmdpipe_exec(struct cmdpipe_info *cpi, const char *mode, void (*procedure)(void *ctx), void *ctx)
{
	pid_t pid;
	int i;
	enum
	{
		M_IN = 0x01,
		M_OUT = 0x02,
		M_ERR = 0x04,
		M_OUTERR = 0x08,
	};
	int m = 0;
	struct fdpair
	{
		int parent;
		int child;
	} f[3] = {{ -1, -1 }, { -1, -1 }, { -1, -1 }};
	int place_holder[3] = { -1, -1, -1 };

	if (mode && mode[0])
	{
		if (strchr(mode, 'i'))
			m |= M_IN;

		if (strchr(mode, 'x'))
			m |= M_OUTERR;
		else
		{
			if (strchr(mode, 'o'))
				m |= M_OUT;
			if (strchr(mode, 'e'))
				m |= M_ERR;
		}
	}

	if (m)
	{
		int fds[2];

		for (i = 0; i < 3; ++i)
		{
			int fd = open("/dev/null", O_RDONLY);
			if (fd > 2)
			{
				close(fd);
				break;
			}
			place_holder[i] = fd;
		}

		if (m & M_IN)
		{
			if (pipe(fds) == -1)
				goto error;
			f[0].parent = fds[1];
			f[0].child = fds[0];
		}
		if ((m & M_OUT) || (m & M_OUTERR))
		{
			if (pipe(fds) == -1)
				goto error;
			f[1].parent = fds[0];
			f[1].child = fds[1];
		}
		if (m & M_ERR)
		{
			if (pipe(fds) == -1)
				goto error;
			f[2].parent = fds[0];
			f[2].child = fds[1];
		}
	}

	pid = fork();
	if (pid == -1)
	{
		goto error;
	}
	else if (pid == 0)
	{
		int max_opened_fd = 64;		/* just a guess */
		int fd;

		if (f[0].child >= 0)
			dup2(f[0].child, 0);
		else
		{
			fd = open("/dev/null", O_RDONLY);
			dup2(fd, 0);
			close(fd);
		}
			
		if (f[1].child >= 0)
		{
			dup2(f[1].child, 1);
			if (m & M_OUTERR)
				dup2(f[1].child, 2);
		}
		else
		{
			fd = open("/dev/null", O_WRONLY);
			dup2(fd, 1);
			if (m & M_OUTERR)
				dup2(fd, 2);
			close(fd);
		}

		if (f[2].child >= 0)
			dup2(f[2].child, 2);
		else
		{
			fd = open("/dev/null", O_WRONLY);
			dup2(fd, 2);
			close(fd);
		}

		for (i = 0; i < 3; ++i)
		{
			if (f[i].child >= 0)
			{
				if (max_opened_fd < f[i].child)
					max_opened_fd = f[i].child;
				if (max_opened_fd < f[i].parent)
					max_opened_fd = f[i].parent;
			}
		}

		for (i = 3; i <= max_opened_fd; ++i)
			close(i);

		procedure(ctx);
		exit(0);
	}

	for (i = 0; i < 3; ++i)
	{
		if (place_holder[i] >= 0)
			close(place_holder[i]);
		if (f[i].child >= 0)
			close(f[i].child);
	}

	cpi->pid = pid;
	cpi->fd0 = f[0].parent;
	cpi->fd1 = f[1].parent;
	cpi->fd2 = f[2].parent;

	return 0;
error:
	for (i = 0; i < 3; ++i)
	{
		if (place_holder[i] >= 0)
			close(place_holder[i]);
		if (f[i].child >= 0)
		{
			close(f[i].child);
			close(f[i].parent);
		}
	}

	cpi->pid = 0;
	cpi->fd0 = -1;
	cpi->fd1 = -1;
	cpi->fd2 = -1;
	return -1;
}


static int64_t get_mono_msec()
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

int cmdpipe_communicate(struct cmdpipe_info *cpi, int timeout, int bufsize,
		xio_read_function in, void *ictx, 
		xio_write_function out, void *octx, 
		xio_write_function err, void *ectx)
{
	int err_ret = -1;
	int status;
	unsigned char *ibuf = NULL, *obuf = NULL;
	int ipos = 0;
	int icapacity = 0;

	struct writeop {
		xio_write_function write;
		void *ctx;
	};
	int *fdptrs[3];
	struct pollfd pollfds[3];
	struct writeop wops[2];
	int num = 0, total;
	uint64_t start = 0;

	if (bufsize <= 0)
		bufsize = BSIZE_DFT;
	else if (bufsize < BSIZE_MIN)
		bufsize = BSIZE_MIN;

	if (cpi->fd1 >= 0)
	{
		if (!obuf)
			obuf = (unsigned char *)malloc(bufsize);
		xnet_set_nonblock(cpi->fd1);
		fdptrs[num] = &cpi->fd1;
		pollfds[num].fd = cpi->fd1;
		pollfds[num].events = POLLIN;
		wops[num].write = out;
		wops[num].ctx = octx;
		++num;
	}

	if (cpi->fd2 >= 0)
	{
		if (!obuf)
			obuf = (unsigned char *)malloc(bufsize);
		xnet_set_nonblock(cpi->fd2);
		fdptrs[num] = &cpi->fd2;
		pollfds[num].fd = cpi->fd2;
		pollfds[num].events = POLLIN;
		wops[num].write = err;
		wops[num].ctx = ectx;
		++num;
	}

	if (cpi->fd0 >= 0)
	{
		if (in)
		{
			ibuf = (unsigned char *)malloc(bufsize);
			xnet_set_nonblock(cpi->fd0);
			fdptrs[num] = &cpi->fd0;
			pollfds[num].fd = cpi->fd0;
			pollfds[num].events = POLLOUT | POLLERR | POLLHUP;
			++num;
		}
		else
		{
			close(cpi->fd0);
		}
	}

	total = num;
	while (num > 0)
	{
		int i;
		int event_num;

		if (timeout > 0)
		{
			uint64_t now = get_mono_msec();
			if (start > 0)
			{
				timeout -= (now - start);
				if (timeout < 0)
				{
					err_ret = -3;
					goto error;
				}
			}
			start = now;
		}

		event_num = poll(pollfds, total, timeout);
		if (event_num < 0 && errno != EINTR)
		{
			goto error;
		}

		for (i = 0; i < total; ++i)
		{
			struct pollfd *pf = &pollfds[i];

			if (pf->fd < 0 || pf->revents == 0)
				continue;

			if (pf->revents & POLLIN) /* POLLIN should be before (POLLOUT | POLLERR) */
			{
				int n;
				do {
					n = xnet_read_nonblock(pf->fd, obuf, bufsize);
					if (n < 0)
					{
						if (n == -1)
							goto error;

						if (n == -2)
						{
							--num;
							close(pf->fd);
							*fdptrs[i] = -1;
							pf->fd = -1;
							if (wops[i].write)
								wops[i].write(wops[i].ctx, obuf, 0);
						}
					}

					if (n > 0 && wops[i].write)
					{
						unsigned char *p = obuf;
						int m = n;
						do {
							int k = wops[i].write(wops[i].ctx, p, m);
							if (k < 0)
								goto error;
							p += k;
							m -= k;
						} while (m > 0);
					}
				} while (pf->fd >= 0 && n == bufsize);
			}
			else if (pf->revents & (POLLHUP | POLLERR))
			{
				--num;
				close(pf->fd);
				*fdptrs[i] = -1;
				pf->fd = -1;
			}
			else if (pf->revents & POLLOUT)	/* POLLOUT must be after (POLLUP | POLLERR) */
			{
				do {
					if (ipos == icapacity)
					{
						int k = in(ictx, ibuf, bufsize);
						if (k < 0)
							goto error;

						if (k == 0)
						{
							--num;
							close(pf->fd);
							*fdptrs[i] = -1;
							pf->fd = -1;
						}
						ipos = 0;
						icapacity = k;
					}

					if (ipos < icapacity)
					{
						int n = xnet_write_nonblock(pf->fd, ibuf + ipos, icapacity - ipos);
						if (n < 0)
						{
							goto error;
						}
						ipos += n;
					}
				} while (pf->fd >= 0 && ipos == icapacity);
			}
		}
	}

	waitpid(cpi->pid, &status, 0);
	return WEXITSTATUS(status);

error:
	kill(cpi->pid, SIGTERM);

	if (cpi->fd0 >= 0)
		close(cpi->fd0);
	if (cpi->fd1 >= 0)
		close(cpi->fd1);
	if (cpi->fd2 >= 0)
		close(cpi->fd2);

	if (ibuf)
		free(ibuf);
	if (obuf)
		free(obuf);

	waitpid(cpi->pid, &status, 0);
	return err_ret;
}


#ifdef TEST_CMDPIPE

#include <stdio.h>

int main()
{
	char *program = "/bin/ls";
	char cmd[] = "ls -l /usr/bin";
	char *mode = "oe";
/*
	char *program = "/usr/local/bin/ffmpeg";
	char cmd[] = "ffmpeg -i pipe:0 -strict -2 -ac 1 -ab 24K -ar 16k -f mp3 -vsync 2 -map 0:a pipe:1";
	char *mode = "ioe";
*/
	int len, status, rc;
	FILE *fp = NULL; 
	struct cmdpipe_info cpi;
	char *args[64], *p;
	int i = 0;

	for (p = strtok(cmd, " \t"); p; p = strtok(NULL, " \t"))
	{
		args[i++] = p;
	}
	args[i++] = NULL;

	rc = cmdpipe_execve(&cpi, mode, program, args, NULL);
	assert(rc == 0);

	rc = cmdpipe_communicate(&cpi, 100, 0, stdio_xio.read, stdin, stdio_xio.write, stdout, stdio_xio.write, stderr);
	fprintf(stderr, "RC=%d\n", rc);
	return 0;
}

#endif

