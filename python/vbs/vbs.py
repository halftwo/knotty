#! /usr/bin/env python
#

__version__ = "170614.170614.18"

import sys
import decimal

Decimal = decimal.Decimal

_PY3 = (sys.hexversion >= 0x3000000)

if _PY3:
    blob = bytes
    default_encoding = "utf-8"
    _itemtype = lambda x: bytes((x,))
    from io import StringIO as _StrIO
    from io import BytesIO as _BinIO
elif "blob" not in dir():
    blob = str
    default_encoding = ""
    _itemtype = chr
    from cStringIO import StringIO as _StrIO
    from cStringIO import StringIO as _BinIO


try:   
    # NB: _vbs dose not support VBS_DECIMAL yet
    # disable it now
    raise ImportError
    from _vbs import *
except ImportError:
    import struct

    VBS_ERR_TOOBIG	= -3
    VBS_ERR_INCOMPLETE	= -2
    VBS_ERR_INVALID	= -1

    VBS_TAIL 	    = 0x01
    VBS_LIST 	    = 0x02
    VBS_DICT 	    = 0x03
    VBS_NULL 	    = 0x0F
    VBS_DESCRIPTOR 	= 0x10
    VBS_BOOL 	    = 0x18
    VBS_BLOB 	    = 0x1B
    VBS_DECIMAL     = 0x1C
    VBS_FLOATING    = 0x1E
    VBS_STRING	    = 0x20
    VBS_INTEGER     = 0x40

    VBS_DESCRIPTOR_MAX      = 0x7fff
    VBS_SPECIAL_DESCRIPTOR  = 0x8000

    _CHR_TAIL 	    = _itemtype(VBS_TAIL)
    _CHR_LIST 	    = _itemtype(VBS_LIST)
    _CHR_DICT 	    = _itemtype(VBS_DICT)
    _CHR_NULL 	    = _itemtype(VBS_NULL)
    _CHR_DESCRIPTOR = _itemtype(VBS_DESCRIPTOR)
    _CHR_BLOB	    = _itemtype(VBS_BLOB)
    _CHR_DECIMAL    = _itemtype(VBS_DECIMAL)
    _CHR_FLOATING   = _itemtype(VBS_FLOATING)
    _CHR_FALSE 	    = _itemtype(VBS_BOOL + 0)
    _CHR_TRUE 	    = _itemtype(VBS_BOOL + 1)

    _FLT_ZERO	    = 1	# +0.0
    _FLT_INF	    = 2	# +inf
    _FLT_NAN	    = 3	# +NaN
    _FLT_SNAN	    = 4	# +sNaN

    _decimalCtx = decimal.Context(prec=16)

    def _pk_strlen(out, num):
        while num >= 0x20:
            out.write(_itemtype(0x80 + (num & 0x7f)))
            num = num >> 7
        out.write(_itemtype(VBS_STRING | num))

    def _pk_int(out, num):
        if num >= 0:
            tag = VBS_INTEGER
        else:
            tag = VBS_INTEGER | 0x20
            num = -num

        while num >= 0x20:
            out.write(_itemtype(0x80 + (num & 0x7F)))
            num = num >> 7
        out.write(_itemtype(tag | num))

    def _pk_num_tag(out, tag, num):
        while num > 0:
            out.write(_itemtype(0x80 + (num & 0x7F)))
            num = num >> 7
        out.write(_itemtype(tag))

    def _pk_floating(out, x):
        tag = VBS_FLOATING
        if x != 0.0:
            if x == float("inf"):
                n0 = 0
                n1 = _FLT_INF
            elif x == float("-inf"):
                n0 = 0
                n1 = -_FLT_INF
            elif (x == x):	# Normal float number
                if x < 0.0:
                    tag = VBS_FLOATING + 1
                    x = -x
                q = struct.unpack('Q', struct.pack('d', x))[0]
                significant = (q & 0xFFFFFFFFFFFFF)
                expo = q >> 52
                if expo > 0:
                    significant = significant | 0x10000000000000
                    expo -= 1023
                else:
                    expo = 1 - 1023

                shift = 0
                while (significant & 0x01) == 0:
                    significant >>= 1
                    shift += 1
                expo = expo - 52 + shift

                n0 = significant
                n1 = expo
            else:		# NaN
                n0 = 0
                n1 = _FLT_NAN
        else:
            n0 = 0
            n1 = _FLT_ZERO
        _pk_num_tag(out, tag, n0)
        _pk_int(out, n1)

    def _pk_decimal(out, x):
        tag = VBS_DECIMAL
        dec = _decimalCtx.plus(x)
        if dec._sign:
            tag += 1

        if dec._is_special:
            n0 = 0
            if dec._exp == 'F':
                expo = _FLT_INF
            elif dec._exp == 'n':
                expo = _FLT_NAN
            elif dec._exp == 'N':
                expo = _FLT_SNAN
            else:
                expo = _FLT_SNAN
        else:
            n0 = int(dec._int)
            if n0 == 0:
                expo = _FLT_ZERO
            else:
                expo = dec._exp

        if n0 == 0 and dec._sign:
            expo = -expo

        _pk_num_tag(out, tag, n0)
        _pk_int(out, expo);

    def _pk_one(out, x, encoding, errors):
        t = type(x)

        if t == int:
            _pk_int(out, x)
        elif t == list or t == tuple or t == set:
            out.write(_CHR_LIST)
            for v in x:
                _pk_one(out, v, encoding, errors)
            out.write(_CHR_TAIL)
        elif t == dict:
            out.write(_CHR_DICT)
            for k, v in x.items():
                _pk_one(out, k, encoding, errors)
                _pk_one(out, v, encoding, errors)
            out.write(_CHR_TAIL)
        elif t == bool:
            if x: out.write(_CHR_TRUE)
            else: out.write(_CHR_FALSE)
        elif t == float:
            _pk_floating(out, x)
        elif t == Decimal:
            _pk_decimal(out, x)
        elif t == type(None):
            out.write(_CHR_NULL)

        elif _PY3:
            if t == str:
                x = x.encode(encoding, errors)
                _pk_strlen(out, len(x))
                out.write(x);
            elif t == bytes:
                _pk_num_tag(out, _CHR_BLOB, len(x))
                out.write(x)
            else:
                out.write(_CHR_NULL)
        else:
            if t == long:
                _pk_int(out, x)
            elif t == str:
                _pk_strlen(out, len(x))
                out.write(x);
            elif t == unicode:
                if encoding == "":
                    encoding = "utf-8"
                x = x.encode(encoding, errors)
                _pk_strlen(out, len(x))
                out.write(x);
            else:
                out.write(_CHR_NULL)


    def _uk_tag(input):
        descriptor = 0
        while True:
            c = input.read(1)
            if c == b'':
                raise StopIteration

            x = ord(c)
            num = 0
            negative = 0
            if x < 0x80:
                tag = x
                if (x >= VBS_STRING):
                    tag = (x & 0x60)
                    num = (x & 0x1F)
                    if (tag == 0x60):
                        tag = VBS_INTEGER
                        negative = 1
                        
                elif (x >= VBS_BOOL):
                    if (x != VBS_BLOB):
                        tag = (x & ~0x1)
                        negative = (x & 0x1)

                    if (x <= VBS_BOOL + 1):
                        num = (x & 0x1)

                elif (x >= VBS_DESCRIPTOR):
                    tag = VBS_DESCRIPTOR
                    num = (x & 0x07)
                    if num == 0:
                        if (descriptor & VBS_SPECIAL_DESCRIPTOR) == 0:
                            descriptor |= VBS_SPECIAL_DESCRIPTOR
                        else:
                            raise ValueError
                    else:
                        if (descriptor & VBS_DESCRIPTOR_MAX) == 0:
                            descriptor |= num;
                        else:
                            raise ValueError
                    continue

            else:
                shift = 0
                while x >= 0x80:
                    num += (x & 0x7F) << shift
                    shift += 7
                    c = input.read(1)
                    if c == b'':
                        raise ValueError
                    x = ord(c)

                tag = x
                if (x >= VBS_STRING):
                    tag = (x & 0x60)
                    num += (x & 0x1F) << shift
                    if (tag == 0x60):
                        tag = VBS_INTEGER
                        negative = 1
                        
                elif (x >= VBS_BOOL):
                    if (x != VBS_BLOB):
                        tag = (x & ~0x1)
                        negative = (x & 0x1)

                    if (x <= VBS_BOOL + 1):
                        num = (x & 0x1)

                elif (x >= VBS_DESCRIPTOR):
                    tag = VBS_DESCRIPTOR
                    num += (x & 0x07) << shift
                    if (num == 0 or num > VBS_DESCRIPTOR_MAX):
                        raise ValueError

                    if (descriptor & VBS_DESCRIPTOR_MAX) == 0:
                        descriptor |= num;
                    else:
                        raise ValueError
                    continue

            if negative:
                num = -num

            return tag, num, descriptor

        # Can't reach here
        return 0, 0, 0

    class _ClosureUnpacked(BaseException):
        pass

    def _uk_until_tail(input, isdict, encoding, errors):
        if isdict:
            r = {}
            while True:
                try:
                    k = _uk_one(input, encoding, errors)
                except _ClosureUnpacked:
                    break
                v = _uk_one(input, encoding, errors)
                r[k] = v
        else:
            r = []
            while True:
                try:
                    v = _uk_one(input, encoding, errors)
                    r.append(v)
                except _ClosureUnpacked:
                    break
        return r

    def _make_floating(significant, expo):
        if significant == 0:
            negative = False
            if expo < 0:
                negative = True
                expo = -expo

            if expo <= _FLT_ZERO:
                if negative:
                    return -0.0
                else:
                    return 0.0
            elif expo == _FLT_INF:
                if negative:
                    return float('-inf')
                else:
                    return float('inf')
            else:
                return float('nan')

        return float(significant * pow(2, expo))

    def _make_decimal(significant, expo):
        if significant:
            s = "%dE%d" % (significant, expo)
        else:
            if expo < 0:
                expo = -expo
                s = '-'
            else:
                s = '+'

            if expo <= _FLT_ZERO:
                s += '0'
            elif expo == _FLT_INF:
                s += 'Inf'
            elif expo == _FLT_NAN:
                s += 'NaN'
            else:
                s += 'sNaN'
 
        return Decimal(s)

    def _uk_one(input, encoding, errors):
        t, n, descriptor = _uk_tag(input)
        if t == VBS_INTEGER:
            return n
        elif t == VBS_STRING:
            s = input.read(n)
            if len(s) < n:
                raise ValueError
            if _PY3:
                s = str(s, encoding, errors)
            elif encoding != "":
                s = unicode(s, encoding, errors)
            return s
        elif t == VBS_BLOB:
            s = input.read(n)
            if len(s) < n:
                raise ValueError
            return s
        elif t == VBS_BOOL:
            return bool(n)
        elif t == VBS_FLOATING:
            t1, n1, d1 = _uk_tag(input)
            if t1 != VBS_INTEGER or d1 != 0:
                raise ValueError
            return _make_floating(n, n1)
        elif t == VBS_DECIMAL:
            t1, n1, d1 = _uk_tag(input)
            if t1 != VBS_INTEGER or d1 != 0:
                raise ValueError
            return _make_decimal(n, n1)
        elif t == VBS_LIST:
            return _uk_until_tail(input, False, encoding, errors)
        elif t == VBS_DICT:
            return _uk_until_tail(input, True, encoding, errors)
        elif t == VBS_TAIL:
            raise _ClosureUnpacked
        elif t == VBS_NULL:
            return None
        raise ValueError


    def encode(obj, encoding=default_encoding, errors="strict"):
        out = _BinIO()
        _pk_one(out, obj, encoding, errors)
        return out.getvalue()

    def decode(buf, encoding=default_encoding, errors="strict"):
        input = _BinIO(buf)
        x = _uk_one(input, encoding, errors)
        return x


    def pack(sequence, encoding=default_encoding, errors="strict"):
        if not isinstance(sequence, tuple) and not isinstance(sequence, list):
            raise TypeError("The first argument 'sequence' should be a tuple or list")

        out = _BinIO()
        for x in sequence:
            _pk_one(out, x, encoding, errors)
        return out.getvalue()

    def unpack(buf, offset=0, num=0, encoding=default_encoding, errors="strict"):
        result = []
        input = _BinIO(buf[offset:])
        n = 0
        while num <= 0 or n < num:
            try:
                x = _uk_one(input, encoding, errors)
                result.append(x)
                n += 1
            except StopIteration:
                break
            except _ClosureUnpacked:
                raise ValueError
            except:
                raise
        result.insert(0, input.tell())
        return tuple(result)


if _PY3:
    _meta_bs = bytes('^~`;[]{}\x7f\xff', "latin1")
    _meta_ss = '^~`;[]{}\x7f\xff'
else:
    _meta_bs = '^~`;[]{}\x7f\xff'
    _meta_ss = '^~`;[]{}\x7f\xff'


def _escape_bs_blob(out, x):
    for i in x:
        if i < 0x20 or i in _meta_bs:
            out.write('`%02X' % i)
        else:
            out.write(chr(i))

    
def _escape_bs_str(out, x):
    for i in x:
        if i < 0x20 or i in _meta_bs:
            out.write('`%02X' % i)
        else:
            out.write(chr(i))


def _print_bseq(out, x, isblob):
    l = len(x)
    if l == 0:
        if isblob: out.write("~|~")
        else: out.write("~!~")
    elif isblob or l >= 100:
        if isblob:
            out.write("~%d|" % l)
            _escape_bs_blob(out, x)
        else:
            out.write("~%d!" % l)
            _escape_bs_str(out, x)
        out.write("~");
    elif x[0].isalpha() and x[-1].isalnum():
        _escape_bs_str(out, x)
    else:
        out.write("~!")
        _escape_bs_str(out, x)
        out.write("~")


def _escape_ss_blob(out, x):
    for c in x:
        i = ord(c)
        if i < 0x20 or c in _meta_ss:
            out.write('`%02X' % i)
        else:
            out.write(c)
    

def _escape_ss_str(out, x):
    for c in x:
        i = ord(c)
        if i < 0x20 or c in _meta_ss:
            out.write('`%02X' % i)
        else:
            out.write(c)


def _print_sseq(out, x, isblob):
    l = len(x)
    if l == 0:
        if isblob: out.write("~|~")
        else: out.write("~!~")
    elif isblob or l >= 100:
        if isblob:
            out.write("~%d|" % l)
            _escape_ss_blob(out, x)
        else:
            out.write("~%d!" % l)
            _escape_ss_str(out, x)
        out.write("~");
    else:
        ch = x[0]
        k = ord(x[-1])
        if (ch.isalpha() or ch == '_') and k > 32 and k < 127:
            _escape_ss_str(out, x)
        else:
            out.write("~!")
            _escape_ss_str(out, x)
            out.write("~")


def _print_one(out, x, encoding, errors):
    t = type(x)

    if t == int:
        out.write("%d" % x)
    elif t == list or t == tuple or t == set:
        out.write("[")
        first = True
        for v in x:
            if first: first = False
            else: out.write("; ")
            _print_one(out, v, encoding, errors)
        out.write("]")
    elif t == dict:
        out.write("{")
        first = True
        for k, v in x.items():
            if first: first = False
            else: out.write("; ")
            _print_one(out, k, encoding, errors)
            out.write("^")
            _print_one(out, v, encoding, errors)
        out.write("}")
    elif t == bool:
        if x: out.write("~T")
        else: out.write("~F")
    elif t == float:
        out.write("%#.17G" % x)
    elif t == Decimal:
        s = str(x) + 'D'
        out.write(s)
    elif t == type(None):
        out.write("~N")

    elif _PY3:
        if t == str:
            x = x.encode(encoding, errors)
            _print_bseq(out, x, False)
        elif t == bytes:
            _print_bseq(out, x, True)
        else:
            out.write("~U")
    else:
        if t == long:
            out.write("%d" % x)
        elif t == str:
            _print_sseq(out, x, False)
        elif t == blob:
            _print_sseq(out, x, True)
        elif t == unicode:
            if encoding == "":
                encoding = "utf-8"
            x = x.encode(encoding, errors)
            _print_sseq(out, x, False)
        else:
            out.write("~U")


def textify(x, encoding=default_encoding, errors="strict"):
    out = _StrIO()
    _print_one(out, x, encoding, errors)
    return out.getvalue()


if __name__ == '__main__':
    s = pack((0, -111, Decimal(22222), -1.0/3, float("-inf"), [4, 5], {'haha': True, 'hoho': None}))
    x = unpack(s, 0, 6)
    print(len(s), textify(s))
    print(x)

#
# vim: ts=4 sw=4 et:
#
