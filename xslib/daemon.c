/* $Id: daemon.c,v 1.8 2009/02/09 04:46:57 jiagui Exp $ */
/*
   Author: XIONG Jiagui
   Date: 2006-06-10
 */
#include "daemon.h"
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: daemon.c,v 1.8 2009/02/09 04:46:57 jiagui Exp $";
#endif

int daemon_init()
{
	int null_fd;
	pid_t pid;

	pid = fork();
	if (pid == -1)
		return -1;
	if (pid > 0)
		exit(0);
	
	setsid();
	signal(SIGHUP, SIG_IGN);

	pid = fork();
	if (pid == -1)
		return -1;
	if (pid > 0)
		exit(0);

	umask(0);
	
	null_fd = open("/dev/null", O_RDWR);

	close(0);
	close(1);

	if (null_fd != -1)
	{
		dup2(null_fd, 0);
		dup2(null_fd, 1);
		close(null_fd);
	}

	return 0;
}

void daemon_redirect_stderr(const char *filename)
{
	int fd;
	if (filename)
		fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0664);
	else
		fd = open("/dev/null", O_RDWR);

	if (fd != -1)
	{
		dup2(fd, 2);
		close(fd);
	}
}

int daemon_lock_pidfile(const char *pathname)
{
	char buf[32];
	int len;
	pid_t pid;
	struct flock lck;
	int fd = open(pathname, O_RDWR | O_CREAT, 0664);
	if (fd == -1)
		goto error;

	lck.l_type = F_WRLCK;
	lck.l_whence = SEEK_SET;
	lck.l_start = 0;
	lck.l_len = 0;
	if (fcntl(fd, F_SETLK, &lck) == -1)
		goto error;

	pid = getpid();
	len = sprintf(buf, "%d", pid);
	write(fd, buf, len);
	return fd;
error:
	if (fd != -1)
		close(fd);
	return -1;
}

