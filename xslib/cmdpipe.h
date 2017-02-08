#ifndef CMDPIPE_H_
#define CMDPIPE_H_

#include <sys/types.h>
#include <unistd.h>
#include "xio.h"

#ifdef __cplusplus
extern "C" {
#endif


struct cmdpipe_info
{
	pid_t pid;	/* pid of the child process. */
	int fd0;	/* connected to the stdin of the child process. */
	int fd1;	/* connected to the stdout of the child process. */
	int fd2;	/* connected to the stderr of the child process. */
};


/* 
   The argument mode points to a string that is empty or contains one or more 
   of the following characters:
	i	cmdpipe_info.fd0 will be connected to the stdin of the executed program.
	o	cmdpipe_info.fd1 will be connected to the stdout of the executed program.
	e	cmdpipe_info.fd2 will be connected to the stderr of the executed program.
	x	cmdpipe_info.fd1 will be connected to the stdout and stderr of the executed program.
   If 'o' occurs with 'x', 'o' is ignored. So is the 'e'.

   Return 0 on success.
   Return -1 on error.
 */
int cmdpipe_exec(struct cmdpipe_info *cpi, const char *mode, void (*procedure)(void *ctx), void *ctx);



int cmdpipe_execve(struct cmdpipe_info *cpi, const char *mode, const char *path, char *const argv[], char *const envp[]/*NULL*/);



/* The number of args (including the trailing NULL) should be no more than 128.
 */
int cmdpipe_execl(struct cmdpipe_info *cpi, const char *mode, const char *path, const char *arg, ...);


/*
 * NB: The first argument 'struct cmdpine_info *cpi' must initialized by cmdpipe_exec() etc functions.
 * After calling the function, the fds are closed, the child is waitpid()ed.
 * Return a integer that is exit status of the child.
 * Or a negative number to indicate an error.
 */
int cmdpipe_communicate(struct cmdpipe_info *cpi, int timeout /* in milliseconds */, 
		xio_read_function in, void *ictx, 
		xio_write_function out, void *octx, 
		xio_write_function err, void *ectx);



#ifdef __cplusplus
}
#endif

#endif

