#! /bin/env python

import sys
import os
import signal
import fcntl
import time
import subprocess

# You may want to change this
RUN_DIR = "/stage/run"

if len(sys.argv) < 2:
	print >> sys.stderr, "Usage: %s <program>" % sys.argv[0]
	sys.exit(1)

prog_name = os.path.basename(sys.argv[1])
prog_dir = os.path.dirname(sys.argv[1])
prog_argv = sys.argv[1:]

suffix = prog_name
if len(sys.argv) > 2:
	suffix += '+' + '+'.join(sys.argv[2:]).replace('/', '^')

lockfilename = RUN_DIR + "/lock." + suffix
try:
	lockfd = os.open(lockfilename, os.O_WRONLY | os.O_CREAT, 0666)
	fcntl.flock(lockfd, fcntl.LOCK_EX | fcntl.LOCK_NB)
except:
	print >> sys.stderr, "lock failed, filename:", lockfilename
	sys.exit(1)

logfilename = RUN_DIR + "/log." + suffix
logfp = open(logfilename, "a")


daemon_pid = os.fork()
if daemon_pid != 0:
	sys.exit(0)


daemon_pid = os.getpid()
child_pid = 0

def sig_handler(sig, frame):
	print >> logfp, 'Signal(%d) caught, kill child(%d)' % (sig, child_pid)
	os.kill(child_pid, signal.SIGTERM)
	os.remove(lockfilename)
	sys.exit(0)

signal.signal(signal.SIGTERM, sig_handler)
signal.signal(signal.SIGINT, sig_handler)
signal.signal(signal.SIGPIPE, sig_handler)

while True:
	print >> logfp, "Try running", " ".join(prog_argv)
	logfp.flush()
	sp = subprocess.Popen(prog_argv, cwd=prog_dir, close_fds=True, stdout=logfp, stderr=subprocess.STDOUT)
	child_pid = sp.pid
	sp.wait()
	time.sleep(3)

