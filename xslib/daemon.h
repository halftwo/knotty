/* $Id: daemon.h,v 1.8 2009/02/09 04:46:57 jiagui Exp $ */
/*
   Author: XIONG Jiagui
   Date: 2006-06-10
 */
#ifndef DAEMON_H_
#define DAEMON_H_

#ifdef __cplusplus
extern "C" {
#endif


/* This function fork()s twice, set the umask to 0.  
   Redirect stdin and stdout to "/dev/null".
   Doing all these things to make the program to be a daemon.
 */
int daemon_init();


/* This function open and lock the file specified by the argument, 
   and write the pid of current process into the opened file.
   The file descripter of the file is returned.
   If error, -1 is returned.
 */
int daemon_lock_pidfile(const char *pathname);



void daemon_redirect_stderr(const char *filename);


#ifdef __cplusplus
}
#endif

#endif

