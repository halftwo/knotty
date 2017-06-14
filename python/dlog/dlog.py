#! /bin/env python

from __future__ import print_function

__version__ = "170614.170614.18"

import socket
import os
import struct
import time
import sys
import inspect 

# set print error and print file tag
_option = 0 
_identity = "-"

DLOG_PORT = 6109

DLOG_STDERR = 0x01
DLOG_PERROR = 0x02

RECORD_VERSION = 2
TYPE_RAW = 0

IDENT_MAX = 63
TAG_MAX = 63
LOCUS_MAX = 127
RECORD_MAX_SIZE = 4000
RECORD_HEAD_SIZE = 16

_pid = os.getpid()
_socket = None
_myfilename = "dlog.py"

def _normalize_str(str, maxLen):
    """"""
    if len(str) > maxLen:
        return str[0:maxLen]
    elif len(str) == 0:
        return "-"
    else:
        return str
    
def _get_socket():
    global _socket
    if _socket is None:
        _socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        _socket.connect(("127.0.0.1", DLOG_PORT))
    return _socket

def _get_locus():
    f = inspect.currentframe().f_back
    while hasattr(f, "f_code"):
        co = f.f_code
        filename = os.path.basename(co.co_filename)
        lineno = f.f_lineno
        if filename != _myfilename:
            break
        f = f.f_back
    return "%s:%d" % (filename, lineno)

def _get_time():
    return time.strftime("%y%m%d-%H%M%S")

def dlog_set(ident, option):
    """set process dlog info"""
    global _identity
    global _option
    
    _identity = ident
    _option = option

def dlog(tag, *arg):
    """like c function to record log"""
    xdlog(_identity, tag, _get_locus(), *arg)

def xdlog(identity, tag, locus, *arg):
    """like c function to record log"""
    global _socket

    if not identity:
        identity = _identity

    # gen log string
    itlocus = "%s %s %s" % (_normalize_str(identity, IDENT_MAX), _normalize_str(tag, TAG_MAX), _normalize_str(locus, LOCUS_MAX))
    itlocus = itlocus.encode()

    # gen packed log info
    truncated = 0x00
    maxStrLen = RECORD_MAX_SIZE - 2 - RECORD_HEAD_SIZE - len(itlocus)
    strArg = " ".join([str(x) for x in arg])
    strArg = strArg.encode()
    if len(strArg) > maxStrLen:
        strArg = strArg[0:maxStrLen]
        truncated = 0x01
    
    composeInfo = truncated << 7 | TYPE_RAW << 4 |RECORD_VERSION
    recordSize = RECORD_HEAD_SIZE + 2 + len(itlocus) + len(strArg)
    data = struct.pack("@H2BHh2i", recordSize, composeInfo, len(itlocus),
             0, _pid, 0, 0) + itlocus + b' ' + strArg + b'\0'
    
    # send log 
    failed = False
    try:
        _get_socket().send(data)
    except Exception as e:
        _socket = None
        failed = True

    # print to stderr
    if (_option & DLOG_STDERR) or (failed and (_option & DLOG_PERROR)):
        print(_get_time(), _pid, itlocus, strArg, file=sys.stderr)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("usage:", sys.argv[0], "<tag> <message> ...", file=sys.stderr)
        sys.exit(1)
    dlog_set("dlogpy", DLOG_STDERR)
    dlog(sys.argv[1], *sys.argv[2:])

#
# vim: ts=4 sw=4 et:
#
