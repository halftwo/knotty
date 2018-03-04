
BASE_HOME = ..

INC = -I. -I$(BASE_HOME) -I$(BASE_HOME)/include
LIBS = -rdynamic -pthread -L${BASE_HOME}/lib -lxic -ldlog -lxs -lrt

CFLAGS = -g -Wall -O2
CXXFLAGS = -g -Wall -O2

OBJS = FcgiQuest.o FcgiClient.o

EXES = x4fcgi

all: ${OBJS} ${EXES}

$(EXES): $(OBJS)

.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(INC) -o $@ $<

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) $(INC) -o $@ $<

.c:
	$(CC) $(CFLAGS) $(CPPFLAGS) $(INC) -o $@ $< $(LIBS)

.cpp:
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(INC) -o $@ $^ $(LIBS)

strip:
	$(STRIP) $(EXES)

clean:
	$(RM) $(OBJS) $(EXES)

