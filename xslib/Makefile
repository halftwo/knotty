##
##

OBJS_C = xsver.o strbuf.o bit.o xnet.o \
	xbase64.o xbase85.o xbase32.o xbase57.o \
	obpool.o jenkins.o md5.o sha1.o sha256.o crc.o crc64.o cpu.o \
	xlog.o rdtsc.o hseq.o xbuf.o vbs_build.o rbtree.o \
	daemon.o strhash.o hdict.o rijndael.o setting.o xutf8.o \
	wdict.o path.o ratcliff.o cmdpipe.o binary_prefix.o urlparse.o \
	cirqueue.o cstr.o hmap.o calltrace.o mp.o \
	hex.o ostk.o hashint.o zlog.o unixfs.o xio.o xmem.o dirwalk.o \
	xmalloc.o xformat.o xstr.o bset.o msec.o xatomic.o \
	vbs_pack.o vbs_stream.o vbs.o vbs_json.o carp.o iconv_convert.o \
	iobuf.o uuid.o unix_user.o rope.o escape.o \
	httpcode.o httpmisc.o urandom.o  \
	decContext.o decSingle.o decDouble.o decQuad.o decimal64.o \
	unit_prefix.o hamming12.o hamming7.o golay24.o \
	hmac_sha1.o cmac_aes.o aes_ccm.o aes_eax.o
	

OBJS_CXX = XRefCount.o xptr.o XLock.o XError.o XEvent.o StringHashSet.o \
	xstr_cxx.o cxxstr.o xmalloc_cxx.o xio_cxx.o Setting.o XThread.o \
	XTimer.o HSequence.o CarpSequence.o Srp6a.o


OBJS_C_XTRA = gzipper.o

OBJS_CXX_XTRA = 



CFLAGS = -g -Wall -fPIC -O2

CXXFLAGS = -g -Wall -fPIC -O2

CPPFLAGS = 


all: libxs.a libxsxtra.a


libxs.a: $(OBJS_C) $(OBJS_CXX)
	$(AR) -rcs $@ $(OBJS_C) $(OBJS_CXX)

libxsxtra.a: $(OBJS_C_XTRA) $(OBJS_CXX_XTRA)
	$(AR) -rcs $@ $(OBJS_C_XTRA) $(OBJS_CXX_XTRA)


.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) -o $@ $<


clean:
	find . -name \*.lo -o -name \*.o | xargs rm -f
	find . -name \*.la -o -name \*.a | xargs rm -f 
	find . -name \*.so | xargs rm -f
	find . -name .libs -a -type d | xargs rm -rf

