##
##

EDITION  = 170623
REVISION = 170719
RELEASE  = 18

XSLIB_VERSION = $(EDITION).$(REVISION).$(RELEASE)

OBJS_C = xsdef.o strbuf.o bit.o xnet.o xbase64.o xbase85.o xbase32.o \
	obpool.o jenkins.o md5.o sha1.o sha256.o crc.o crc64.o cpu.o \
	xlog.o rdtsc.o hseq.o xbuf.o vbs_build.o rbtree.o \
	daemon.o strhash.o hdict.o rijndael.o setting.o xutf8.o \
	wdict.o path.o ratcliff.o cmdpipe.o binary_prefix.o urlparse.o \
	cirqueue.o minilzo.o mlzo.o cstr.o hmap.o calltrace.o mp.o \
	hex.o ostk.o hashint.o zlog.o unixfs.o xio.o xmem.o dirwalk.o \
	xmalloc.o xformat.o xstr.o bset.o msec.o \
	vbs_pack.o vbs_stream.o vbs.o vbs_json.o carp.o iconv_convert.o \
	iobuf.o uuid.o unix_user.o rope.o escape.o \
	httpcode.o httpmisc.o urandom.o gzipper.o \
	decContext.o decSingle.o decDouble.o decQuad.o decimal64.o \
	unit_prefix.o hamming12.o hamming7.o golay24.o \
	hmac_sha1.o cmac_aes.o aes_ccm.o aes_eax.o
	

OBJS_CXX = XRefCount.o xptr.o XLock.o XError.o XEvent.o StringHashSet.o \
	xstr_cxx.o cxxstr.o xmalloc_cxx.o xio_cxx.o Setting.o XThread.o \
	XTimer.o HSequence.o CarpSequence.o Srp6a.o


# For C
LIBXS_S_SO = libxs_s.so
LIBXS_S_SONAME = $(LIBXS_S_SO).$(EDITION)
LIBXS_S_REVISION = $(LIBXS_S_SONAME).$(REVISION)
LIBXS_S_RELEASE = $(LIBXS_S_REVISION).$(RELEASE)

# For C++
LIBXS_X_SO = libxs_x.so
LIBXS_X_SONAME = $(LIBXS_X_SO).$(EDITION)
LIBXS_X_REVISION = $(LIBXS_X_SONAME).$(REVISION)
LIBXS_X_RELEASE = $(LIBXS_X_REVISION).$(RELEASE)


CFLAGS = -g -Wall -fPIC -O2

CXXFLAGS = -g -Wall -fPIC -O2

CPPFLAGS = -DXSLIB_VERSION=$(XSLIB_VERSION)


#all: libxs.a $(LIBXS_S_SO) $(LIBXS_X_SO) 
all: libxs.a


libxs.a: $(OBJS_C) $(OBJS_CXX)
	$(AR) -rcs $@ $(OBJS_C) $(OBJS_CXX)


$(LIBXS_S_SO): $(LIBXS_S_SONAME)
	ln -sf $(LIBXS_S_SONAME) $@

$(LIBXS_S_SONAME): $(LIBXS_S_RELEASE)
	ln -sf $(LIBXS_S_RELEASE) $@

$(LIBXS_S_RELEASE): $(OBJS_C)
	$(CC) -shared -o $@ -Wl,-soname,$(LIBXS_S_SONAME) $(OBJS_C)


$(LIBXS_X_SO): $(LIBXS_X_SONAME)
	ln -sf $(LIBXS_X_SONAME) $@

$(LIBXS_X_SONAME): $(LIBXS_X_RELEASE)
	ln -sf $(LIBXS_X_RELEASE) $@

$(LIBXS_X_RELEASE): $(OBJS_CXX)
	$(CC) -shared -o $@ -Wl,-soname,$(LIBXS_X_SONAME) $(OBJS_CXX)


.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) -o $@ $<


install_so:
	cp -fa $(LIBXS_S_RELEASE) /usr/local/lib
	( cd /usr/local/lib; ln -sf $(LIBXS_S_RELEASE) $(LIBXS_S_SONAME); ln -sf $(LIBXS_S_SONAME) $(LIBXS_S_SO) )
	cp -fa $(LIBXS_X_RELEASE) /usr/local/lib
	( cd /usr/local/lib; ln -sf $(LIBXS_X_RELEASE) $(LIBXS_X_SONAME); ln -sf $(LIBXS_X_SONAME) $(LIBXS_X_SO) )
	ldconfig

clean:
	find . -name \*.lo -o -name \*.o | xargs rm -f
	find . -name \*.la -o -name \*.a | xargs rm -f 
	find . -name \*.so | xargs rm -f
	find . -name .libs -a -type d | xargs rm -rf
	rm -f $(LIBXS_X_SO) $(LIBXS_X_SONAME) $(LIBXS_X_RELEASE)
	rm -f $(LIBXS_S_SO) $(LIBXS_S_SONAME) $(LIBXS_S_RELEASE)

