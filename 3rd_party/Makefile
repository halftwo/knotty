

INSTALL_DIR=/usr/local

TOP_DIR=..


all: lua lz4 st

install: st-install

clean: lua-clean lz4-clean st-clean


lua:
	tar -xf lua-5.1.5a.tar.gz
	(cd lua-5.1.5a 					\
		&& make posix 				\
		&& cd ..				\
		&& ln -s lua-5.1.5a lua 		\
	)

lua-clean:
	$(RM) lua
	$(RM) -r lua-5.1.5a


st:
	tar -xf st-1.9.tar.gz
	(cd st-1.9 					\
		&& make linux-optimized 		\
		&& cp -p obj/st.h ../$(TOP_DIR)/include	\
		&& cp -p obj/libst.a ../$(TOP_DIR)/lib 	\
		&& cd ..				\
		&& ln -s st-1.9 st 			\
	)

st-install:
	(cd st/obj;					\
		cp -p st.h $(INSTALL_DIR)/include; 	\
		cp -p libst.a $(INSTALL_DIR)/lib; 	\
	)

st-clean:
	$(RM) st
	$(RM) -r st-1.9
	$(RM) $(TOP_DIR)/include/st.h


lz4:
	tar -xf lz4-1.8.3.tar.gz
	(cd lz4-1.8.3 					\
		&& make 				\
		&& cp -p lib/lz4.h ../$(TOP_DIR)/include	\
		&& cp -p lib/liblz4.a ../$(TOP_DIR)/lib 	\
		&& cd ..				\
		&& ln -s lz4-1.8.3 lz4 			\
	)

lz4-clean:
	$(RM) lz4
	$(RM) -r lz4-1.8.3


