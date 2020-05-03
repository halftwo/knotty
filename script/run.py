#! /usr/bin/env python3
# vim: ts=4 sw=4 et:

import sys
import os
import getopt
import signal
import fcntl
import time
import subprocess
import traceback

# XXX You may want to change this
RUN_DIR = "/xio/run"

interval = 3

def usage(myself):
    print("Usage: %s [options] <program>" % myself, file=sys.stderr)
    print("   -t interval", file=sys.stderr)
    sys.exit(1)

if len(sys.argv) < 2:
	usage(sys.argv[0])

opts, args = getopt.getopt(sys.argv[1:], "t:")

for k, v in opts:
	if k == '-t':
		interval = int(v, 0)
		if interval < 1:
			interval = 1

if len(args) == 0:
	usage(sys.argv[0])

prog_name = os.path.basename(args[0])
prog_dir = os.path.dirname(args[0])
prog_argv = args

suffix = prog_name
if len(args) >= 2:
    suffix += '+' + '+'.join(args[1:]).replace('/', '^')

lockfilename = RUN_DIR + "/lock." + suffix
try:
    lockfd = os.open(lockfilename, os.O_WRONLY | os.O_CREAT, 0o666)
    fcntl.flock(lockfd, fcntl.LOCK_EX | fcntl.LOCK_NB)
    os.ftruncate(lockfd, 0)
    os.write(lockfd, bytes(str(os.getpid()), 'utf8'))
except Exception as ex:
    traceback.print_exc(file=sys.stderr)
    sys.exit(1)

logfilename = RUN_DIR + "/log." + suffix
logfp = open(logfilename, "a")


daemon_pid = os.fork()
if daemon_pid != 0:
    sys.exit(0)


child_pid = 0
daemon_pid = os.getpid()
os.ftruncate(lockfd, 0)
os.lseek(lockfd, 0, os.SEEK_SET)
os.write(lockfd, bytes(str(daemon_pid), 'utf8'))


def sig_handler(sig, frame):
    print('Signal(%d) caught, kill child' % sig, file=logfp)
    os.kill(child_pid, signal.SIGTERM)

    pid = int(open(lockfilename).read(16))
    if daemon_pid == pid:
        os.remove(lockfilename)

    sys.exit(1)


signal.signal(signal.SIGTERM, sig_handler)
signal.signal(signal.SIGINT, sig_handler)
signal.signal(signal.SIGPIPE, sig_handler)

while True:
    print("Try running", " ".join(prog_argv), file=logfp)
    sp = subprocess.Popen(prog_argv, cwd=prog_dir, close_fds=True, stdout=logfp, stderr=subprocess.STDOUT)
    child_pid = sp.pid
    sp.wait()
    time.sleep(interval)

