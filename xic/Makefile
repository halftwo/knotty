##
##

OBJS = XicMessage.o XicCheck.o XicException.o Context.o \
	ServantI.o EngineImp.o VData.o VWriter.o \
	PtEngineImp.o StEngineImp.o STimer.o \
	IpMatcher.o SecretBox.o ShadowBox.o

LIBXIC = libxic.a

EXES = shadow_gen secret_check DemoServer DemoClient DemoServerSt DemoClientSt


CFLAGS = -g -Wall -O2

CXXFLAGS = -g -Wall -O2

CPPFLAGS = -I. -I.. -I../include

LIBS = -rdynamic -pthread -L../lib -lxic -ldlog -lxs -lrt


all: $(OBJS) $(LIBXIC) $(EXES)

$(EXES): $(LIBXIC)

$(LIBXIC): $(OBJS)
	$(AR) -rcs $@ $(OBJS)


.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) -o $@ $<


.cpp:
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $< $(LIBS)

DemoServer: DemoServer.cpp DemoServant.o
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $^ $(LIBS)

DemoServerSt: DemoServerSt.cpp DemoServant.o
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $^ $(LIBS) -lst

DemoClientSt: DemoClientSt.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $< $(LIBS) -lst

shadow_file: ShadowBox.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -DTEST_SHADOW -o $@ $< $(LIBS)

clean:
	$(RM) $(OBJS) $(LIBXIC) $(EXES)

