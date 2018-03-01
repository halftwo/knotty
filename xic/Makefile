##
##

LIBXIC_OBJS = XicMessage.o XicCheck.o XicException.o Context.o \
	ServantI.o EngineImp.o VData.o VWriter.o \
	PtEngineImp.o StEngineImp.o STimer.o \
	IpMatcher.o SecretBox.o ShadowBox.o MyCipher.o

LIBXIC = libxic.a

EXES = shadow_gen secret_check DemoServer DemoCallback DemoClient DemoClientSt 


CFLAGS = -g -Wall -O2

CXXFLAGS = -g -Wall -O2

CPPFLAGS = -I. -I.. -I../include

LIBS = -rdynamic -pthread -Wl,-static -L../lib -lxic -ldlog -lxs -Wl,-call_shared -lrt


all: $(LIBXIC_OBJS) $(LIBXIC) $(EXES)

$(EXES): $(LIBXIC)

$(LIBXIC): $(LIBXIC_OBJS)
	$(AR) -rcs $@ $(LIBXIC_OBJS)


.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) -o $@ $<


.cpp:
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $< $(LIBS)

DemoCallback: DemoCallback.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $^ $(LIBS)

DemoServer: DemoServer.cpp DemoServant.o
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $^ $(LIBS)

DemoClient: DemoClient.cpp DemoClientImp.o
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $^ $(LIBS)

DemoClientSt: DemoClientSt.cpp DemoClientImp.o
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $^ $(LIBS) -lst

shadow_file: ShadowBox.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -DTEST_SHADOW -o $@ $< $(LIBS)

clean:
	$(RM) *.lo *.la DemoServant.o DemoClientImp.o $(LIBXIC_OBJS) $(LIBXIC) $(EXES) 

