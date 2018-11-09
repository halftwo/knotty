#! /usr/bin/env python

"""
This module requires python 2.6+ and depends on 
    greenlet
    eventlet or gevent
    vbs
    dlog


XXX: It seems this module is not working with gevent.
     Is it our bug? Or a bug of gevent? I don't know yet.

"""
from __future__ import print_function

XIC_V_EDITION  = 170614
XIC_V_REVISION = 170614
XIC_V_RELEASE  = 18

__version__ = str(XIC_V_EDITION) + "." + str(XIC_V_REVISION) + "." + str(XIC_V_RELEASE)

XIC_ENGINE_VERSION  = "PY." + __version__


import vbs
import dlog
import struct
import socket
import traceback
import random
import time
import os
import sys

_PY3 = (sys.hexversion >= 0x3000000)

try:
    import resource
except:
    pass

DEFAULT_MESSAGE_SIZE = 16*1024*1024

DEFAULT_CONNECT_TIMEOUT = 60*1000
DEFAULT_CLOSE_TIMEOUT = 900*1000

DEFAULT_ACM_SERVER = 0
DEFAULT_ACM_CLIENT = 300


xic_message_size = DEFAULT_MESSAGE_SIZE

xic_dlog_sq = False
xic_dlog_sa = False
xic_dlog_sae = False

xic_dlog_cq = False
xic_dlog_ca = False
xic_dlog_cae = False

xic_dlog_warning = True
xic_dlog_debug = False

# in milliseconds, 0 for no timeout
xic_timeout_connect = DEFAULT_CONNECT_TIMEOUT
xic_timeout_close = DEFAULT_CLOSE_TIMEOUT
xic_timeout_message = 0

# NB: The following settings have no effects yet
xic_acm_server = DEFAULT_ACM_SERVER
xic_acm_client = DEFAULT_ACM_CLIENT

xic_slow_server = -1
xic_slow_client = -1

xic_sample_server = 0
xic_sample_client = 0

xic_except_server = False
xic_except_client = False


try:
    raise Exception("gevent is not supported")
    import gevent
    import gevent.monkey
    import gevent.event
    import gevent.queue
    import gevent.threadpool

    gevent.monkey.patch_all()

    Queue = gevent.queue.Queue
    Channel = gevent.queue.Channel

    _spawn = gevent.spawn
    _sleep = gevent.sleep
    _Pool = gevent.threadpool.ThreadPool
    _Timeout = gevent.timeout.Timeout

    _Event = gevent.event.Event

except:
    import eventlet

    eventlet.patcher.monkey_patch()

    Queue = eventlet.queue.Queue

    class Channel(eventlet.queue.LightQueue):
        def __init__(self):
            super(Channel, self).__init__(0)

    _spawn = eventlet.greenthread.spawn
    _sleep = eventlet.greenthread.sleep
    _Pool = eventlet.greenpool.GreenPool
    _Timeout = eventlet.timeout.Timeout

    class _Event(eventlet.event.Event):
        def set(self):
            if not self.has_result():
                self.send()

        def clear(self):
            if self.has_result():
                self.reset()


MTYPE_QUEST = 'Q'
MTYPE_ANSWER = 'A'
MTYPE_HELLO = 'H'
MTYPE_BYE = 'B'


_textify = vbs.textify


def _pack_header(type, len):
    return struct.pack('>3cBi', b'X', b'!', type, 0, len)


def _except2answer(ex):
    arguments = {
        "exname": type(ex).__name__,
        "code": getattr(ex, "code", 0),
        "tag": getattr(ex, "tag", 0),
        "message": getattr(ex, "message", ""),
        "raiser": getattr(ex, "raiser", ""),
        "detail": getattr(ex, "detail", ""),
    }
    if not isinstance(ex, RemoteException):
        arguments['local'] = True
    return Answer(-1, -1, arguments)


class XicException(Exception):
    def __init__(self, *args, **kwargs):
        """XicException({...})
        XicException(**kwargs)
        """
        tb = traceback.extract_stack(limit=128)[:-1]
        calltrace = []
        for x in tb:
            calltrace.insert(0, "%s:%s %s" % (x[0], x[1], x[3]))

        self.__dict__['detail'] = { 
            "file": tb[-1][0],
            "line": tb[-1][1],
            "calltrace": '\n'.join(calltrace),
        }

        self.__dict__['raiser'] = 'UNKNOWN_RAISER'

        for x in args:
            t = type(x)
            if t == int:
                self.__dict__['code'] = x
            elif t == str:
                self.__dict__['message'] = x
            elif t == dict:
                for k, v in x.items():
                    self.__dict__[k] = v

        for k, v in kwargs.items():
            self.__dict__[k] = v

    def __str__(self):
        items = []
        for k, v in self.__dict__.items():
            items.append("%s=%s" % (k, repr(v)))
        s = type(self).__name__ + '(' + ", ".join(items) + ')'
        return s


class RemoteException(XicException): pass

class ProtocolException(XicException): pass
class MarshalException(ProtocolException): pass
class MessageSizeException(ProtocolException): pass

class QuestFailedException(XicException): pass
class ServiceNotFoundException(QuestFailedException): pass
class ServiceEmptyException(QuestFailedException): pass
class MethodNotFoundException(QuestFailedException): pass
class MethodEmptyException(QuestFailedException): pass
class MethodOnewayException(QuestFailedException): pass

class ParameterException(QuestFailedException): pass
class ParameterLimitException(ParameterException): pass
class ParameterNameException(ParameterException): pass
class ParameterMissingException(ParameterException): pass
class ParameterTypeException(ParameterException): pass
class ParameterDataException(ParameterException): pass

class SocketException(XicException): pass
class ConnectFailedException(SocketException): pass
class ConnectionLostException(SocketException): pass

class TimeoutException(XicException): pass
class ConnectTimeoutException(TimeoutException): pass
class MessageTimeoutException(TimeoutException): pass
class CloseTimeoutException(TimeoutException): pass

class AdapterAbsentException(XicException): pass

class ConnectionClosedException(XicException): pass
class EngineStoppedException(XicException): pass
class EndpointParseException(XicException): pass
class EndpointMissingException(XicException): pass
class ServiceParseException(XicException): pass
class ProxyFixedException(XicException): pass
class ServantException(XicException): pass

class InternalException(XicException): pass
class UnknownException(InternalException): pass


class XicMessage(object):
    def __init__(self, msgType):
        self.msgType = msgType

    def _pack(self):
        return _pack_header(self.msgType, 0)

class _KHelloMessage(XicMessage):
    def __init__(self):
        self.msgType = MTYPE_HELLO

class _KByeMesssage(XicMessage):
    def __init__(self):
        self.msgType = MTYPE_BYE


_HELLO_MESSAGE = _KHelloMessage()
_CLOSE_MESSAGE = _KByeMesssage()


class Parameters(dict):
    def __init__(self, init):
        super(dict, self).__init__(init)

    def get_int(self, k, d=0):
        return self._get(k, d, int) if _PY3 else self._get(k, d, int, long)

    def get_float(self, k, d=0.0):
        return self._get(k, d, float)

    def get_bool(self, k, d=False):
        return self._get(k, d, bool)
    
    def get_str(self, k, d=''):
        return self._get(k, d, str)
    
    def get_bytes(self, k, d=''):
        return self._get(k, d, vbs.blob, str)

    def get_list(self, k, d=[]):
        return self._get(k, d, list)

    def get_dict(self, k, d={}):
        return self._get(k, d, dict)

    def _get(self, k, d, *types):
        v = self.get(k)
        if v == None:
            return d
        if type(v) not in types:
            raise ParameterTypeException(k)
        return v


    def want_int(self, k):
        return self._want(k, int) if _PY3 else self._want(k, int, long)

    def want_float(self, k):
        return self._want(k, float)

    def want_bool(self, k):
        return self._want(k, bool)
    
    def want_str(self, k):
        return self._want(k, str)
    
    def want_bytes(self, k):
        return self._want(k, vbs.blob, str)

    def want_list(self, k):
        return self._want(k, list)

    def want_dict(self, k):
        return self._want(k, dict)

    def _want(self, k, *types):
        v = self.get(k)
        if v == None:
            raise ParameterMissingException(k)
        if type(v) not in types:
            raise ParameterTypeException(k)
        return v
        

class Context(dict):
    def __init__(self, init):
        super(dict, self).__init__(init)

    def get_int(self, k, d=0):
        return self._get(k, d, int) if _PY3 else self._get(k, d, int, long)

    def get_float(self, k, d=0.0):
        return self._get(k, d, float)

    def get_bool(self, k, d=False):
        return self._get(k, d, bool)
    
    def get_str(self, k, d=''):
        return self._get(k, d, str)
    
    def get_bytes(self, k, d=''):
        return self._get(k, d, vbs.blob, str)

    def _get(self, k, d, *types):
        v = self.get(k)
        if v == None:
            return d
        if type(v) not in types:
            raise ParameterTypeException(k)
        return v


    def want_int(self, k):
        return self._want(k, int) if _PY3 else self._want(k, int, long)

    def want_float(self, k):
        return self._want(k, float)

    def want_bool(self, k):
        return self._want(k, bool)
    
    def want_str(self, k):
        return self._want(k, str)
    
    def want_bytes(self, k):
        return self._want(k, vbs.blob, str)

    def _want(self, k, *types):
        v = self.get(k)
        if v == None:
            raise ParameterMissingException(k)
        if type(v) not in types:
            raise ParameterTypeException(k)
        return v
        

class Quest(XicMessage):
    def __init__(self, txid, service, method, arguments, ctx=None):
        self.msgType = MTYPE_QUEST
        self.txid = txid
        self.service = service
        self.method = method
        self.args = arguments
        self.ctx = ctx if ctx else {}

    @classmethod
    def create(cls, method, arguments, ctx=None):
        return cls(-1, '__DUMMY_', method, arguments, ctx)

    @classmethod
    def createOneway(cls, method, arguments, ctx=None):
        return cls(0, '__DUMMY__', method, arguments, ctx)

    def _pack(self):
        body = vbs.pack((self.txid, self.service, self.method, self.ctx, self.args))
        return _pack_header(self.msgType, len(body)) + body


class Answer(XicMessage):
    def __init__(self, txid, status, arguments):
        self.msgType = MTYPE_ANSWER
        self.txid = txid
        self.status = status
        self.args = arguments

    @classmethod
    def create(cls, status, arguments):
        return cls(-1, status, arguments)

    def _pack(self):
        body = vbs.pack((self.txid, self.status, self.args))
        return _pack_header(self.msgType, len(body)) + body


class Result(object):
    def __init__(self, con, prx, quest, callback):
        self._con = con
        self._prx = prx
        self._event = _Event()
        self._quest = quest
        self._callback = callback
        self._ex = None

    def _questSent(self):
        if self._callback:
            self._callback.sent(self)

    def _giveError(self, ex):
        self._ex = ex
        a = _except2answer(ex)
        self._giveAnswer(a)

    def _giveAnswer(self, answer):
        if xic_dlog_ca or (answer.status and xic_dlog_cae):
            tag = "XIC.CAE" if answer.status else "XIC.CA"
            con = self._con
            q = self._quest
            dlog.xdlog(None, tag, "/", "%s/%s %d Q=%s::%s T=-1 A=%d %s" % (
                        con._sock_addr, con._peer_addr,
                        q.txid, q.service, q.method,
                        answer.status, _textify(answer.args))
                )

        self._answer = answer
        self._event.set()
        if self._callback:
            self._callback.completed(self)

    def waitForCompleted(self):
        self._event.wait()

    def takeAnswer(self, throwit=True):
        self._event.wait()
        answer = self._answer
        if throwit and (self._ex or answer.status):
            if self._ex:
                raise self._ex
            else:
                raise RemoteException(answer.args)
        return answer

    def getAnswerArgs(self):
        answer = self.takeAnswer()
        return answer.args


def _connect(endpoint):
    try:
        ep = endpoint[1:] if endpoint[0] == '@' else endpoint
        proto, host, port = ep.split()[0].split('+')
    except:
        raise EndpointParseException(endpoint)

    if proto == '':
        proto = 'tcp'
    port = int(port)

    if not host:
        host = "localhost"

    if host.find(':') >= 0:
        domain = socket.AF_INET6
    else:
        domain = socket.AF_INET

    if proto == 'tcp':
            sock = socket.socket(domain, socket.SOCK_STREAM)
    else:
        raise XicException("unsupported transport")

    sock.connect((host, port))
    return sock


class Connection(object):
    ST_INIT = 0
    ST_WAITING_HELLO = 1
    ST_ACTIVE = 2
    ST_CLOSE = 3
    ST_CLOSING = 4
    ST_CLOSED = 5
    ST_ERROR = 6

    def __init__(self, engine, incoming, sock, endpoint=""):
        self._engine = engine
        self._state = Connection.ST_INIT if incoming else Connection.ST_WAITING_HELLO
        self._incoming = incoming
        self._sock = sock
        self._endpoint = endpoint.strip()
        self._last_txid = 0
        self._adapter = None
        self._processing = 0
        self._resultMap = {}
        self._queue = []
        self._queueEvent = _Event()
        self._closedEvent = _Event()
        self._send_thr = None
        self._recv_thr = None

        self._msg_timeout = 0
        self._close_timeout = 0
        self._connect_timeout = 0
        ep = self._endpoint
        if ep.startswith('@'):
            ep = ep[1:]
        for x in ep.split()[1:]:
            try:
                k, v = x.split('=')
                if k == 'timeout':
                    ts = v.split(',')
                    self._msg_timeout = int(ts[0])
                    if len(ts) > 1:
                        self._close_timeout = int(ts[1])
                        if len(ts) > 2:
                            self._connect_timeout = int(ts[2])
            except:
                pass

        if self._msg_timeout <=0 and xic_timeout_message:
            self._msg_timeout = xic_timeout_message

        if sock:
            self._peer_addr = "%s+%d" % sock.getpeername()
            self._sock_addr = "%s+%d" % sock.getsockname()
        else:
            self._peer_addr = endpoint
            self._sock_addr = ""
        self._ex = None

    @classmethod
    def _create_server_connection(cls, engine, sock, adapter):
        con = cls(engine, True, sock)
        con.setAdapter(adapter)
        con._start()
        return con

    @classmethod
    def _create_client_connection(cls, engine, endpoint):
        con = cls(engine, False, None, endpoint)
        con._start()
        return con

    def isLive(self):
        return self._state < Connection.ST_CLOSE
    
    def isWaiting(self):
        return self._state < Connection.ST_ACTIVE

    def isActive(self):
        return self._state == Connection.ST_ACTIVE
    
    def isGraceful(self):
        return self._state >= Connection.ST_CLOSE and self._state < Connection.ST_ERROR

    def isBad(self):
        return self._state >= Connection.ST_ERROR

    def _closeTimeout(self):
        if self._close_timeout > 0:
            return self._close_timeout
        elif xic_timeout_close > 0:
            return xic_timeout_close
        elif self._msg_timeout > 0:
            return self._msg_timeout
        elif xic_timeout_message > 0:
            return xic_timeout_message
        return DEFAULT_CLOSE_TIMEOUT

    def _connectTimeout(self):
        if self._connect_timeout > 0:
            return self._connect_timeout
        elif xic_timeout_connect > 0:
            return xic_timeout_connect
        elif self._msg_timeout > 0:
            return self._msg_timeout
        elif xic_timeout_message > 0:
            return xic_timeout_message
        return DEFAULT_CONNECT_TIMEOUT

    def __gen_txid(self):
        self._last_txid += 1 
        if self._last_txid > 0x7FFFFFFF or self._last_txid < 0:
            self._last_txid = 1 
        return self._last_txid

    def __recv_msg(self):
        if self._msg_timeout > 0 and len(self._resultMap) > 0:
            timeout = _Timeout(self._msg_timeout/1000.0, MessageTimeoutException)
        else:
            timeout = None

        try:
            hdr = self._sock.recv(8)
            while len(hdr) < 8:
                x = self._sock.recv(8 - len(hdr))
                if len(x) == 0:
                    raise ConnectionLostException("peer=" + self._peer_addr)
                hdr += x

            hdr = struct.unpack('>3cBi', hdr)
            assert(hdr[0] == b'X' and hdr[1] == b'!')
            msgType = hdr[2]
            # flags = hdr[3]       # NOT USED
            bodyLen = hdr[4]

            if bodyLen > 0:
                data = self._sock.recv(bodyLen)
                i = len(data)
                if i < bodyLen:
                    segs = [data]
                    while i < bodyLen:
                        x = self._sock.recv(bodyLen - i)
                        if len(x) == 0:
                            raise ConnectionLostException("peer=" + self._peer_addr)
                        segs.append(x)
                        i += len(x)
                    data = ''.join(segs)

                x = vbs.unpack(data)
                if x[0] != bodyLen:
                    raise MarshalException()

                elif msgType == MTYPE_QUEST:
                    msg = Quest(x[1], x[2], x[3], x[5], x[4])
                elif msgType == MTYPE_ANSWER:
                    msg = Answer(x[1], x[2], x[3])
                else:
                    raise ProtocolException("invalid msgType")

            elif msgType == MTYPE_HELLO:
                msg = _KHelloMessage()
            elif msgType == MTYPE_BYE:
                msg = _KByeMesssage()
            else:
                raise ProtocolException("invalid msgType")

            return msg

        finally:
            if timeout:
                timeout.cancel()

    def __handle_quest(self, quest):
        if self._state >= Connection.ST_CLOSE:
            return

        self._processing += 1
        txid = quest.txid
        service = quest.service
        method = quest.method
        try:
            if xic_dlog_sq:
                dlog.xdlog(None, "XIC.SQ", "/", "%s/%s %d Q=%s::%s C%s %s" % (
                            self._sock_addr, self._peer_addr,
                            txid, service, method,
                            _textify(quest.ctx), _textify(quest.args))
                    )

            servant = self._adapter.findServant(quest.service)
            if not servant:
                servant = self._adapter.getDefaultServant()
                if not servant:
                    raise ServiceNotFoundException("peer={} service={}".format(self._peer_addr, service))

            current = Current(self)
            answer_args = servant.process(quest, current)

            if txid:
                if answer_args == None and not current._waiter:
                    raise MethodOnewayException("peer={} method={}".format(self._peer_addr, method))
                answer = Answer(-1, 0, answer_args)

        except Exception as ex:
            if txid:
                answer = _except2answer(ex)

        if current._waiter:
            if answer:
                current._waiter.response(answer)
            return

        if txid:
            answer.txid = txid
            if xic_dlog_sa or (answer.status and xic_dlog_sae):
                tag = "XIC.SAE" if answer.status else "XIC.SA"
                dlog.xdlog(None, tag, "/", "%s/%s %d Q=%s::%s T=-1 A=%d %s" % (
                            self._sock_addr, self._peer_addr,
                            txid, service, method,
                            answer.status, _textify(answer.args))
                    )

            self._replyAnswer(answer)

        elif answer:
            if xic_dlog_warning:
                dlog.dlog("XIC.WARN", "peer=%s #=answer for oneway quest discarded" % self._peer_addr)
            # XXX: oneway quest got an answer
            pass

        if self._state == Connection.ST_CLOSE:
            self.__closing()

    def __recv_fiber(self):
        lastFiber = True
        try:
            msg = self.__recv_msg()
            if msg.msgType != MTYPE_BYE:
                if self._incoming:
                    self._recv_thr = self._engine._serverPool.spawn(self.__recv_fiber)
                else:
                    self._recv_thr = self._engine._clientPool.spawn(self.__recv_fiber)
                lastFiber = False

            if msg.msgType == MTYPE_QUEST:
                self.__handle_quest(msg)

            elif msg.msgType == MTYPE_ANSWER:
                result = self._resultMap[msg.txid]
                del self._resultMap[msg.txid]
                if self._state == Connection.ST_CLOSE:
                    self.__closing()
                result._giveAnswer(msg)

            elif msg.msgType == MTYPE_BYE:
                # We should close the socket first
                pass

            else:
                raise ProtocolException(("unexpected msgType(%d) received" % msg.msgType))

        except Exception as ex:
            if self._state < Connection.ST_CLOSE:
                self._ex = ex
                #TODO

        if lastFiber:
            self._recv_thr = None
            self.__disconnect()

    def __send_fiber(self):
        try:
            if not self._incoming:
                t = self._connectTimeout()
                t = t if t > 0 else 86400*1000
                timeout = _Timeout(t/1000.0, ConnectTimeoutException)
                try:
                    self._sock = _connect(self._endpoint)
                    self._peer_addr = "%s+%d" % self._sock.getpeername()
                    self._sock_addr = "%s+%d" % self._sock.getsockname()
                    msg = self.__recv_msg()
                    if msg.msgType != MTYPE_HELLO:
                        raise ProtocolException(("unexpected msgType(%d) received" % msg.msgType))
                finally:
                    timeout.cancel()

            if self._state < Connection.ST_ACTIVE:
                self._state = Connection.ST_ACTIVE

            self._recv_thr = self._engine._clientPool.spawn(self.__recv_fiber)

            while self._state < Connection.ST_CLOSE:
                self._queueEvent.wait()
                while len(self._queue) > 0:
                    msg = self._queue.pop(0)
                    if isinstance(msg, XicMessage):
                        data = msg._pack()
                    else:
                        data = msg

                    self._sock.sendall(data)

                    if msg.msgType == MTYPE_QUEST and msg.txid:
                        result = self._resultMap.get(msg.txid, None)
                        if result:
                            result._questSent()
                self._queueEvent.clear()

        except Exception as ex:
            self._ex = ex
            #TODO
        self._send_thr = None
        self.__disconnect()

    def __send_msg(self, msg):
        self._queue.append(msg)
        self._queueEvent.set()

    def _start(self):
        if self._incoming:
            self.__send_msg(_HELLO_MESSAGE)
        self._send_thr = _spawn(self.__send_fiber)

    def __disconnect(self):
        if self._state >= Connection.ST_CLOSED:
            return

        self._state = Connection.ST_CLOSED
        if self._send_thr:
            self._queueEvent.set()
        if self._recv_thr:
            self._recv_thr.kill()

        if self._sock:
            self._sock.close()

        resultMap = self._resultMap
        self._resultMap = None
        if not self._ex:
            self._ex = ConnectionClosedException()
        ex = self._ex
        for k, v in resultMap.items():
            v._giveError(ex)
        self._closedEvent.set()

    def __graceful(self):
        if self._state < Connection.ST_CLOSE:
            self._state = Connection.ST_CLOSE
            self.__closing()

    def __wait_and_disconnect(self, t):
        _sleep(t/1000.0)
        self.__disconnect()

    def __closing(self):
        if self._processing <= 0 and len(self._resultMap) <= 0:
            self._state = Connection.ST_CLOSING
            self.__send_msg(_CLOSE_MESSAGE)
            t = self._closeTimeout()
            if t > 0:
                _spawn(self.__wait_and_disconnect, t)

    def _wait_for_closed(self):
        self._closedEvent.wait()

    def _replyAnswer(self, msg):
        self._processing -= 1
        self.__send_msg(msg)
        if self._state == Connection.ST_CLOSE:
            self.__closing()

    def _sendQuest(self, prx, quest, callback):
        if self._state >= Connection.ST_CLOSE:
            if quest.txid:
                result = Result(self, prx, quest, callback)
                result._giveError(ConnectionClosedException())
            else:
                result = None
            return result

        txid = quest.txid
        if txid == -1:
            txid = self.__gen_txid()
            quest.txid = txid
            result = Result(self, prx, quest, callback)
            self._resultMap[txid] = result
        else:
            result = None

        if xic_dlog_cq:
            dlog.xdlog(None, "XIC.CQ", "/", "%s/%s %d Q=%s::%s C%s %s" % (
                        self._sock_addr, self._peer_addr,
                        quest.txid, quest.service, quest.method,
                        _textify(quest.ctx), _textify(quest.args))
                )

        self.__send_msg(quest)
        return result

    def close(self, force = False):
        if force:
            self.__disconnect()
        else:
            self.__graceful()

    def createProxy(self, service):
        return Proxy(self._engine, service, self)

    def setAdapter(self, adapter):
        self._adapter = adapter

    def getAdapter(self):
        return self._adapter


class Completion(object):
    """Callback object for quest"""

    def sent(self, result):
        """override"""
        pass

    def completed(self, result):
        """override

        Usually, you would call:
        answer = result.takeAnswer()
        """
        pass


class Waiter(object):
    def __init__(self, con):
        self._con = con

    def response(self, answer):
        if not self._con:
            # Already responded
            return

        if isinstance(answer, Exception):
            answer = _except2answer(answer)
        self._con._replyAnswer(answer)
        self._con = None


class _Listener(object):

    def __init__(self, adapter, endpoint):
        try:
            proto, ip, port = endpoint.split()[0].split('+')
        except:
            raise EndpointParseException(endpoint)

        if ip.find(':') >= 0:
            domain = socket.AF_INET6
        else:
            domain = socket.AF_INET

        if proto == 'tcp' or proto == '':
            sock = socket.socket(domain, socket.SOCK_STREAM)
        else:
            raise XicException("unsupported transport")

        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind((ip, int(port)))
        sock.listen(32)
        self._sock = sock
        self._adapter = adapter
        self._endpoint = endpoint

    def __accept_fiber(self):
        while True:
            try:
                sock, addr = self._sock.accept()
                con = Connection._create_server_connection(self._adapter._engine, sock, self._adapter)
                self._adapter._engine._incomingCons.append(con)
            except Exception as ex:
                dlog.dlog("EXCEPTION", type(ex).__name__, ex)
                _sleep(1.0)

    def activate(self):
        self._accept_thr = _spawn(self.__accept_fiber)

    def deactivate(self):
        self._accept_thr.kill()


class Adapter(object):

    def __init__(self, engine, name, endpoints):
        self._engine = engine
        self._name = name
        self._srvMap = {}
        self._defaultServant = None
        eps = [ x.strip() for x in endpoints.split('@') ]
        self._listeners = [ _Listener(self, x) for x in eps if len(x) > 0]
        self._endpoints = ('@' + '@'.join(eps)) if len(eps) > 0 else ''
        if not self._listeners:
            raise EndpointMissingException("No invalid endpoints for Adapter(%s)" % self._name)

    @property
    def engine(self):
        return self._engine

    @property
    def services(self):
        return self._srvMap.keys()

    @property
    def endpoints(self):
        return self._endpoints

    def activate(self):
        for x in self._listeners:
            x.activate()

    def deactivate(self):
        for x in self._listeners:
            x.deactivate()

    def addServant(self, service, servant):
        self._srvMap[service] = servant
        return self._engine.stringToProxy(service + ' ' + self._endpoints);

    def removeServant(self, service):
        del self._srvMap[service]

    def findServant(self, service):
        servant = self._srvMap.get(service)
        if not servant:
            if service == '\x00':
                servant = self._engine._servant
        return servant

    def getDefaultServant(self):
        return self._defaultServant

    def setDefaultServant(self, servant):
        self._defaultServant = servant

    def unsetDefaultServant(self):
        self._defaultServant = None

class Current(object):

    def __init__(self, con):
        self._con = con
        self._waiter = None

    def asynchronous(self):
        if not self._waiter:
            self._waiter = Waiter(self._con)
        return self._waiter


class Servant(object):
    """A Servant should derived from this class.

    Servant should provides method with prototype like _xic_xxx(self, quest, current).
    Following is an example method to echo all the parameters back to the xic client.
    The method should return a dict with string as the key.

    def _xic_echo(self, quest, current):
        return quest.args
    """

    def _methods(self):
        methods = []
        for k in dir(self):
            if k.startswith("_xic_"):
                methods.append(k[5:])
        return methods

    def process(self, quest, current):
        funcname = '_xic_' + quest.method
        func = getattr(self, funcname, None)
        if func:
            return func(quest, current)
        elif quest.method == "\x00ping":
            return {}
        elif quest.method == "\x00methods":
            return {"methods": self._methods()}
        else:
            raise MethodNotFoundException(quest.method)

 
class Proxy(object):

    class _RemoteMethod(object):
        def __init__(self, prx, method):
            self._prx = prx
            self._method = method

        def __call__(self, *args, **kwargs):
            if len(args) == 1:
                kwargs.update(args[0])
            elif len(args) > 1:
                raise TypeError("Argument should be one dict or/and keyword arguments")
            return self._prx.invoke(self._method, kwargs)

    class _Remote(object):
        def __init__(self, prx):
            self._prx = prx
 
        def __getattr__(self, name):
            rm = self.__dict__.get(name)
            if not rm:
                rm = Proxy._RemoteMethod(self._prx, name)
                self.__dict__[name] = rm
            return rm

    class _RemoteOnewayMethod(object):
        def __init__(self, prx, method):
            self._prx = prx
            self._method = method

        def __call__(self, *args, **kwargs):
            if len(args) == 1:
                kwargs.update(args[0])
            elif len(args) > 1:
                raise TypeError("Argument should be one dict or/and keyword arguments")
            return self._prx.invoke_oneway(self._method, kwargs)

    class _RemoteOneway(object):
        def __init__(self, prx):
            self._prx = prx
 
        def __getattr__(self, name):
            rm = self.__dict__.get(name)
            if not rm:
                rm = Proxy._RemoteOnewayMethod(self._prx, name)
                self.__dict__[name] = rm
            return rm

    def __init__(self, engine, proxy, con=None):
        self._engine = engine
        self._proxy = proxy
        self._ctx = {}
        xs = [ x.strip() for x in proxy.split('@') ]
        self._service = xs[0]
        self._endpoints = xs[1:]
        if con:
            self._incoming = True
            self._cons = [con]
        else:
            self._incoming = False
            self._cons = [ None for i in range(len(self._endpoints)) ]
        self._idx = 0
        self._remote = Proxy._Remote(self)
        self._remote_oneway = Proxy._RemoteOneway(self)

    def __str__(self):
        return self._proxy

    def __get_con(self):
        if self._incoming:
            con = self._cons[0]
        else:
            con = self._cons[self._idx]
            if not con or not con.isLive():
                self._idx += 1
                if self._idx >= len(self._endpoints):
                    self._idx = 0
                con = self._engine._make_connection(self._endpoints[self._idx])
                self._cons[self._idx] = con
        return con

    @property
    def remote(self):
        return self._remote

    @property
    def remote_oneway(self):
        return self._remote_oneway

    def emitQuest(self, quest, callback):
        con = self._cons[0] if self._incoming else self.__get_con()
        quest.service = self._service
        return con._sendQuest(self, quest, callback)

    def invoke(self, method, arguments, ctx=None):
        if ctx == None:
            ctx = self._ctx
        quest = Quest(-1, self._service, method, arguments, ctx)
        result = self.emitQuest(quest, None)
        result.waitForCompleted()
        answer = result.takeAnswer(True)
        return answer.args

    def invoke_async(self, method, arguments, callback, ctx=None):
        if ctx == None:
            ctx = self._ctx
        quest = Quest(-1, self._service, method, arguments, ctx)
        return self.emitQuest(quest, callback)

    def invoke_oneway(self, method, arguments, ctx=None):
        if ctx == None:
            ctx = self._ctx
        quest = Quest(0, self._service, method, arguments, ctx)
        self.emitQuest(quest, None)


def _gen_suffix():
    _SUFFIX = "kmgtpezy"
    d = {}
    for i in range(len(_SUFFIX)):
        d[_SUFFIX[i]] = 1000**(i+1)
        d[_SUFFIX[i] + 'i'] = 1024**(i+1)
    return d

_suffix = _gen_suffix()


class Setting(object):
    def __init__(self, map={}):
        self._map = map.copy()

    def getString(self, k, d = ''):
        v = self._map.get(k, d)
        if type(v) != str:
            v = str(v)
        return v

    def getInt(self, k, d = 0):
        v = self._map.get(k, d)
        if type(v) != int:
            if v.isdigit():
                v = int(v)
            else:
                try:
                    for i in range(0, len(v)):
                        if not v[i].isdigit():
                            break
                    v = int(v[:i]) * _suffix[v[i:].lower()]
                except:
                    v = d
        return v

    def getBool(self, k, d = False):
        v = self._map.get(k, d)
        t = type(v)
        if t != bool:
            if t == int or t == float:
                v = bool(v)
            elif t == str:
                v = v.lower()
                if v == 'yes' or v == 'true':
                    v = True
                elif v == 'no' or v == 'false':
                    v = False
                else:
                    v = d
            else:
                v = d
        return v

    def getFloat(self, k, d = 0.0):
        v = self._map.get(k, d)
        if type(v) != float:
            try:
                v = float(v)
            except ValueError:
                v = d
        return v

    def getStringList(self, k):
        v = self._map.get(k)
        if v == None:
            v = []
        elif type(v) != str:
            v = [str(v)]
        else:
            v = v.split(",")
        return v

    def set(self, k, v):
        self._map[str(k)] = v

    def insert(self, k, v):
        k = str(k)
        if not self._map.has_key(k):
            self._map[k] = v
            return True
        return False

    def update(self, k, v):
        k = str(k)
        if self._map.has_key(k):
            self._map[k] = v
            return True
        return False

    def _load(self, filename):
        for line in open(filename):
            line = line.strip()
            if line == '' or line.startswith("#"):
                continue
            try:
                k, v = line.split('=', 1)
                self.set(k.strip(), v.strip())
            except:
                pass


class _EngineServant(Servant):
    def __init__(self, engine):
        self._engine = engine

    def _xic_info(self, quest, current):
        eg = self._engine

        adapters = []
        for adapter in eg._adapterMap.values():
            v = { "endpoints": adapter.endpoints,
                    "services": adapter.services,
                    "catchall": bool(adapter.getDefaultServant())
                }
            adapters.append(v)

        proxies = []
        for prx in eg._prxMap.values():
            v = str(prx)
            proxies.append(v)

        return {
            "dlog.identity": dlog._identity,
            "engine.id": eg._id,
            "engine.start_time": eg._start_time,
            "engine.version": XIC_ENGINE_VERSION,
            "throb.logword": eg._logword,
            "adapter.count": len(eg._adapterMap),
            "proxy.count": len(eg._prxMap),
            "connection.count": len(eg._conMap),
            "adapters": adapters,
            "proxies": proxies,
	        "xic.message.size": xic_message_size,
            "xic.dlog": self.__dlog_setting(),
            "xic.timeout": self.__timeout_setting(),
            "xic.acm": self.__acm_setting(),
            "xic.slow": self.__slow_setting(),
            "xic.sample": self.__sample_setting(),
            "xic.except": self.__except_setting(),
            "xic.rlimit": self.__rlimit_setting(),
        }

    def _xic_tune(self, quest, current):
        a = {}

        x = quest.args.get("dlog")
        if isinstance(x, dict):
            if x.has_key("sq"):
                global xic_dlog_sq
                xic_dlog_sq = bool(x.get("sq"))
            if x.has_key("sa"):
                global xic_dlog_sa
                xic_dlog_sa = bool(x.get("sa"))
            if x.has_key("sae"):
                global xic_dlog_sae
                xic_dlog_sae = bool(x.get("sae"))
            if x.has_key("cq"):
                global xic_dlog_cq
                xic_dlog_cq = bool(x.get("cq"))
            if x.has_key("ca"):
                global xic_dlog_ca
                xic_dlog_ca = bool(x.get("ca"))
            if x.has_key("cae"):
                global xic_dlog_cae
                xic_dlog_cae = bool(x.get("cae"))
            if x.has_key("warning"):
                global xic_dlog_warning
                xic_dlog_warning = bool(x.get("warning"))
            if x.has_key("debug"):
                global xic_dlog_debug
                xic_dlog_debug = bool(x.get("debug"))

            a['dlog'] = self.__dlog_setting()

        x = quest.args.get("timeout")
        if isinstance(x, dict):
            if x.has_key("connect"):
                global xic_timeout_connect
                xic_timeout_connect = int(x.get("connect"))
            if x.has_key("close"):
                global xic_timeout_close
                xic_timeout_close = int(x.get("close"))
            if x.has_key("message"):
                global xic_timeout_message
                xic_timeout_message = int(x.get("message"))

            a['timeout'] = self.__timeout_setting()

        x = quest.args.get("acm")
        if isinstance(x, dict):
            if x.has_key("server"):
                global xic_acm_server
                xic_acm_server = int(x.get("server"))
            if x.has_key("client"):
                global xic_acm_client
                xic_acm_client = int(x.get("client"))

            a['acm'] = self.__acm_setting()

        x = quest.args.get("slow")
        if isinstance(x, dict):
            if x.has_key("server"):
                global xic_slow_server
                xic_slow_server = int(x.get("server"))
            if x.has_key("client"):
                global xic_slow_client
                xic_slow_client = int(x.get("client"))

            a['slow'] = self.__slow_setting()
        
        x = quest.args.get("sample")
        if isinstance(x, dict):
            if x.has_key("server"):
                global xic_sample_server
                n = int(x.get("server"))
                xic_sample_server = n if n > 0 else 0
            if x.has_key("client"):
                global xic_sample_client
                n = int(x.get("client"))
                xic_sample_client = n if n > 0 else 0

            a['sample'] = self.__sample_setting()
        
        x = quest.args.get("except")
        if isinstance(x, dict):
            if x.has_key("server"):
                global xic_except_server
                xic_except_server = bool(x.get("server"))
            if x.has_key("client"):
                global xic_except_client
                xic_except_client = bool(x.get("client"))

            a['except'] = self.__except_setting()

        x = quest.args.get("rlimit")
        if isinstance(x, dict):
            if x.has_key("nofile"):
                try:
                    n = int(x.get("nofile"))
                    _, hard = resource.getrlimit(resource.RLIMIT_NOFILE)
                    resource.setrlimit(resource.RLIMIT_NOFILE, (n, hard))
                except:
                    pass

            a['rlimit'] = self.__rlimit_setting()

        return a

    def __dlog_setting(self):
        return {
            'sq': xic_dlog_sq,
            'sa': xic_dlog_sa,
            'sae': xic_dlog_sae,
            'cq': xic_dlog_cq,
            'ca': xic_dlog_ca,
            'cae': xic_dlog_cae,
            'warning': xic_dlog_warning,
            'debug': xic_dlog_debug,
        }

    def __timeout_setting(self):
        return {
            "connect": xic_timeout_connect,
            "close": xic_timeout_close,
            "message": xic_timeout_message,
        }

    def __acm_setting(self):
        return {
            "server": xic_acm_server,
            "client": xic_acm_client,
        }

    def __slow_setting(self):
        return {
            'server': xic_slow_server,
            'client': xic_slow_client,
        }

    def __sample_setting(self):
        return {
            'server': xic_sample_server,
            'client': xic_sample_client,
        }

    def __except_setting(self):
        return {
            'server': xic_except_server,
            'client': xic_except_client,
        }

    def __rlimit_setting(self):
        try:
            nofile, _ = resource.getrlimit(resource.RLIMIT_NOFILE)
            return {
                "nofile": nofile,
            }
        except:
            return {}

def _random_base57id(n):
    Alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789"
    Id = Alphabet[random.randint(0,48)]
    for i in range(1, n):
        Id = Id + Alphabet[random.randint(0,56)]
    return Id


class Engine(object):

    def __init__(self, setting):
        self._setting = setting
        self._adapterMap = {}
        self._prxMap = {}
        self._conMap = {}
        self._incomingCons = []
        self._shutdown = _Event()
        server_pool_size = setting.getInt("xic.SThreadPool.Server.SizeMax", 10000)
        client_pool_size = setting.getInt("xic.SThreadPool.Client.SizeMax", 10000)
        self._serverPool = _Pool(server_pool_size)
        self._clientPool = _Pool(client_pool_size)
        self._logword = ""
        self._id = _random_base57id(23)
        self._servant = _EngineServant(self)
        self._start_time = time.strftime("%Y%m%d-%H%M%S")
        self._listenAddress = ''

    @property
    def setting(self):
        return self._setting

    def _make_connection(self, endpoint):
        con = self._conMap.get(endpoint)
        if not con or not con.isLive():
            con = Connection._create_client_connection(self, endpoint)
            self._conMap[endpoint] = con
        return con

    def createAdapter(self, name='', endpoints=''):
        if name == '':
            name = 'xic'
        if endpoints == '':
            key = name + ".Endpoints"
            endpoints = self._setting.getString(key)
        adapter = Adapter(self, name, endpoints)
        self._adapterMap[name] = adapter

        listenAddress = []
        for a in self._adapterMap.values():
            eps = [x.strip() for x in a._endpoints.split('@')]
            for e in eps:
                if e == '':
                    continue
                listenAddress.append('@'+e.split()[0])
        self._listenAddress = ''.join(listenAddress)
        return adapter

    def createSlackAdapter(self):
        pass

    def stringToProxy(self, prxString):
        prx = Proxy(self, prxString)
        self._prxMap[prxString] = prx
        return prx

    def waitForShutdown(self):
        self._shutdown.wait()

    def shutdown(self):
        self._shutdown.set()

        for adapter in self._adapterMap.values():
            adapter.deactivate()

        for con in self._incomingCons:
            con.close(False)

        for con in self._conMap.values():
            con.close(False)

    def throb(self, logword):
        self._logword = logword

    def _start(self):
        _spawn(self.__throb)

    def _wait(self):
        for con in self._incomingCons:
            con._wait_for_closed()

        for con in self._conMap.values():
            con._wait_for_closed()

    def __throb(self):
        start_time = "start=" + self._start_time 
        version = XIC_ENGINE_VERSION
        while True:
            dlog.xdlog("", "THROB", version, start_time, "id="+self._id, "listen="+self._listenAddress, self._logword)
            t = time.time()
            left = 60 - (t % 60)
            _sleep(left)


def spawn(func, *args, **kwargs):
    return _spawn(func, *args, **kwargs)


def sleep(seconds):
    _sleep(seconds)


def _stderr_print(*args):
    print(*args, file=sys.stderr)


def _help():
    _stderr_print("\nUsage:", sys.argv[0], "--xic.conf=<config_file> [--AAA.BBB=ZZZ]\n")
    sys.exit(1)


def _adjust_setting(setting):
    argv = []
    configfile = None
    argdict = {}
    for x in sys.argv:
        if x == '--help' or x == '-?':
            _help()
        elif x.startswith("--") and x.find('.') > 2:
            if x.startswith("--xic.conf="):
                configfile = x[11:]
            else:
                k, v = x[2:].split('=', 1)
                argdict[k.strip()] = v.strip()
        else:
            argv.append(x)

    if configfile:
        setting._load(configfile)

    for k, v in argdict.items():
        setting.set(k, v)

    return argv


def _apply_setting(setting):
    global xic_dlog_sq, xic_dlog_sa, xic_dlog_sae
    global xic_dlog_cq, xic_dlog_ca, xic_dlog_cae
    global xic_dlog_warning, xic_dlog_debug
    xic_dlog_sq = setting.getBool("xic.dlog.sq")
    xic_dlog_sa = setting.getBool("xic.dlog.sa")
    xic_dlog_sae = setting.getBool("xic.dlog.sae")
    xic_dlog_cq = setting.getBool("xic.dlog.cq")
    xic_dlog_ca = setting.getBool("xic.dlog.ca")
    xic_dlog_cae = setting.getBool("xic.dlog.cae")
    xic_dlog_warning = setting.getBool("xic.dlog.warning", True)
    xic_dlog_debug = setting.getBool("xic.dlog.debug")

    global xic_message_size
    xic_message_size = setting.getInt("xic.message.size", DEFAULT_MESSAGE_SIZE)

    global xic_timeout_connect, xic_timeout_close, xic_timeout_message
    xic_timeout_connect = setting.getInt("xic.timeout.connect", DEFAULT_CONNECT_TIMEOUT)
    xic_timeout_close = setting.getInt("xic.timeout.close", DEFAULT_CLOSE_TIMEOUT)
    xic_timeout_message = setting.getInt("xic.timeout.message")

    global xic_acm_server, xic_acm_client
    xic_acm_server = setting.getInt("xic.acm.server", DEFAULT_ACM_SERVER)
    xic_acm_client = setting.getInt("xic.acm.client", DEFAULT_ACM_CLIENT)

    global xic_slow_server, xic_slow_client
    xic_slow_server = setting.getInt("xic.slow.server", -1)
    xic_slow_client = setting.getInt("xic.slow.client", -1)

    global xic_sample_server, xic_sample_client
    xic_sample_server = setting.getInt("xic.sample.server")
    xic_sample_client = setting.getInt("xic.sample.client")

    global xic_except_server, xic_except_client
    xic_except_server = setting.getBool("xic.except.server")
    xic_except_client = setting.getBool("xic.except.client")

    try:
        if os.getuid() == 0:
            soft, hard = resource.getrlimit(resource.RLIMIT_NOFILE)
            if hard < 65536:
                resource.setrlimit(resource.RLIMIT_NOFILE, (soft, 65536))

        soft, hard = resource.getrlimit(resource.RLIMIT_NOFILE)
        n = setting.getInt("xic.rlimit.nofile", -1)
        if n > 0:
            resource.setrlimit(resource.RLIMIT_NOFILE, (n, hard))
    except Exception as ex:
        if xic_dlog_warning:
            dlog.dlog("XIC.WARN", "resource.setrlimit() failed.", ex)

    gid = -1
    try:
        import grp
        group = setting.getString("xic.group")

        if group:
            gr = grp.getgrnam(group)
            os.setgid(gr.gr_gid)
            gid = gr.gr_gid
    except Exception as ex:
        if xic_dlog_warning:
            dlog.dlog("XIC.WARN", "set xic.group failed.", ex)

    try:
        import pwd
        user = setting.getString("xic.user")
        if user:
            pw = pwd.getpwnam(user)
            os.setuid(pw.pw_uid)
            if gid < 0:
                os.setgid(pw.pw_gid)
    except Exception as ex:
        if xic_dlog_warning:
            dlog.dlog("XIC.WARN", "set xic.user failed.", ex)


def oneway_method(func):
    """decorator for the oneway xic method

    This decorator will check and make sure the result is None 
    returned from the servant method.
    """
    def wrapper(self, quest, current):
        r = func(self, quest, current)
        if r != None and xic_dlog_warning:
            method = func.__name__
            dlog.dlog("XIC.WARN", "Oneway method %s return non-None result: %s" % (method, r))
        return None
    return wrapper


def args_check(**arg2type):
    """decorator for the Servant xic method

    This decorator will check the parameter types in the quest
    for example:
    @args_check(a=(int, long), b=bool, c=str, d=float)
    """
    def decorator(func):
        def wrapper(self, quest, current):
            arguments = quest.args
            for k, t in arg2type.items():
                v = arguments.get(k)
                if v == None:
                    raise ParameterMissingException("Parameter '%s' missing which should be %s" % (k, t))
                elif not isinstance(v, t):
                    raise ParameterTypeException("Parameter '%s' should be %s instead of %s" % (k, t, type(v)))
            return func(self, quest, current)
        return wrapper
    return decorator


def args_optional(**argdefaults):
    """decorator for the Servant xic method

    This decorator will add missing parameters to the quest with the default values
    for example:
    @args_optional(o=1.5, p=True)
    """
    def decorator(func):
        def wrapper(self, quest, current):
            arguments = quest.args
            for k, d in argdefaults.items():
                if not arguments.has_key(k):
                    arguments[k] = d
            return func(self, quest, current)
        return wrapper
    return decorator


def args_unpack(func):
    """decorator for the Servant xic method

    This decorator will unpack the parameters when calling the servant method.
    It's should be the most inner decorator that's just before the servant method.
    """
    def wrapper(self, quest, current):
        return func(self, **quest.args)
    return wrapper


def start_xic(func, initSetting=None):
    """prototype of func is func(argv, engine)
    """
    if initSetting:
        if not isinstance(initSetting, Setting):
            initSetting = Setting(initSetting)
        setting = initSetting
    else:
        setting = Setting()

    try:
        program = os.path.basename(sys.argv[0])
        dlog.dlog_set(program, 0)
        argv = _adjust_setting(setting)
        _apply_setting(setting)
        engine = Engine(setting)
        engine._start()
        r = func(argv, engine)
        engine.shutdown()
        engine._wait()
    except Exception as ex:
        dlog.dlog("EXCEPTION", type(ex).__name__, ex)
        traceback.print_exc()
        _help()
        r = None
    return r


def _server_demo(port):
    class MyServant(Servant):
        def _xic_echo(self, quest, current):
            return quest.args

        def _xic_time(self, quest, current):
            return {"time":time.time()}

        @oneway_method
        def _xic_sink(self, quest, current):
            # NB: Oneway method should return None
            return {}

        @args_check(a=(int,), b=bool, c=str)
        @args_optional(d=1.5)
        def _xic_foo(self, quest, current):
            arguments = quest.args
            return {"A":arguments['a'], "B":arguments['b'], "C":arguments['c'], "D":arguments['d']}

        @args_check(a=(int,), b=bool, c=str)
        @args_optional(d=1.5)
        @args_unpack
        def _xic_bar(self, a, b, c, d):
            return {"A":a, "B":b, "C":c, "D":d}

    def run(argv, engine):
        adapter = engine.createAdapter()
        adapter.addServant('Test', MyServant())
        adapter.activate()
        engine.waitForShutdown()

    setting = { "xic.Endpoints": "@tcp++%d" % port, }
    start_xic(run, setting)


def _client_demo(port):
    def run(argv, engine):
        prx = engine.stringToProxy(engine.setting.getString("Test.Proxy"))

        prx.remote_oneway.sink()

        # Option A
        r = prx.invoke("echo", { "arg1": 1, "arg2": 0.2 })
        # Option B
        r = prx.remote.echo(arg1 = 1, arg2 = 0.2)
        # Option C
        q = Quest.create("echo", { "arg1": 1, "arg2": 0.2 })
        result = prx.emitQuest(q, None)
        r = result.getAnswerArgs()

        # Option A, B and C are all the same
        _stderr_print(r)

    setting = { "Test.Proxy": "Test @tcp+localhost+%d" % port, }
    start_xic(run, setting)


if __name__ == '__main__':
    if len(sys.argv) < 2:
        _stderr_print("Usage:", sys.argv[0], "server_demo|client_demo", "[port]")
        sys.exit(1)

    which = sys.argv[1]
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 9898
    if which == 'server_demo':
        _server_demo(port)
    elif which == 'client_demo':
        _client_demo(port)
    else:
        _stderr_print("Nothing to do!")
        sys.exit(1)

#
# vim: ts=4 sw=4 st=4 et:
#
