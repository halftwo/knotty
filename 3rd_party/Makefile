

INSTALL_DIR=/usr/local

TOP_DIR=..


all: lua st

install: st-install

clean: lua-clean st-clean


lua:
	tar -xf lua-5.1.5.tar.gz
	(cd lua-5.1.5 					\
		&& make linux 				\
		&& cd ..				\
		&& ln -s lua-5.1.5 lua 			\
	)

lua-clean:
	$(RM) lua
	$(RM) -r lua-5.1.5


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

