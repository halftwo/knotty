##
## 
OBJS= misc.o dlog.o dlog_imp.o plugin.o luadlog.o recpool.o \
	cabin.o banlist.o eth_speed.o diskspace.o netaddr.o top.o

LIBRARY= libdlog.a

EXES= dlogd dlog_center dlog_write dstsd

CFLAGS= -g -Wall -fPIC -O2

CXXFLAGS = -g -Wall -fPIC -O2

CPPFLAGS= -DXSLIB_RCSID -I. -I.. -I../3rd_party/lua/src -I../3rd_party/lz4/lib

LIBS= -rdynamic -pthread -Wl,-static -L../lib -L../3rd_party/lua/src -L../3rd_party/lz4/lib -lxs -llua -llz4 -Wl,-call_shared -ldl -lm -lrt

all: $(OBJS) $(LIBRARY) $(EXES)

xslib.make:
	(cd ../xslib; make)

rebuild:
	svn up
	$(MAKE) clean
	$(MAKE) all

libdlog.a: dlog.o dlog_imp.o
	$(AR) rcs $@ $?

libdlog.so: dlog.o dlog_imp.o
	g++ -shared -o $@ $?

python: _dlog.so


_dlog.so: dlog.o
	swig -python dlog.swg
	$(CC) -c -I/usr/include/python2.5/ dlog_wrap.c
	$(LD) -shared -o _dlog.so dlog.o dlog_wrap.o


$(EXES):  $(OBJS)

.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

.c:
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $< -pthread -L../lib -lxs

.cpp:
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $< -pthread -L. -L../lib -lxs


dlogd: dlogd.cpp 
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $^ $(LIBS)

dlog_center: dlog_center.cpp 
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $^ $(LIBS)

dlog_write: dlog_write.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $< libdlog.a -pthread -L../lib -lxs

dstsd: dlogd
	ln -f dlogd dstsd
	
install:

	cp -rf libdlog.a ../lib
	cp -rf libdlog.a ../lib/dlog

clean:
	$(RM) $(EXES) $(OBJS) $(LIBRARY) *.o dlog_wrap.c _dlog.so

