dnl $Id$
dnl config.m4 for extension xic

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

PHP_ARG_WITH(xic, for xic support,
[  --with-xic             Include xic support])

dnl Otherwise use enable:

PHP_ARG_ENABLE(xic, whether to enable xic support,
[  --enable-xic           Enable xic support])

if test "$PHP_XIC" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-xic -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/xic.h"  # you most likely want to change this
  dnl if test -r $PHP_XIC/$SEARCH_FOR; then # path given as parameter
  dnl   XIC_DIR=$PHP_XIC
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for xic files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       XIC_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$XIC_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the xic distribution])
  dnl fi

  dnl # --with-xic -> add include path
  dnl PHP_ADD_INCLUDE($XIC_DIR/include)

  dnl # --with-xic -> check for lib and symbol presence
  dnl LIBNAME=xic # you may want to change this
  dnl LIBSYMBOL=xic # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $XIC_DIR/lib, XIC_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_XICLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong xic lib version or lib not found])
  dnl ],[
  dnl   -L$XIC_DIR/lib -lm -ldl
  dnl ])
  dnl
  dnl PHP_SUBST(XIC_SHARED_LIBADD)

  PHP_REQUIRE_CXX()

  dnl PHP_ADD_LIBRARY(xs_x, 1, XIC_SHARED_LIBADD)
  dnl PHP_ADD_LIBRARY(xs_s, 1, XIC_SHARED_LIBADD)
  PHP_ADD_LIBRARY(stdc++, 1, XIC_SHARED_LIBADD)
  PHP_ADD_LIBRARY(rt, 1, XIC_SHARED_LIBADD)
  PHP_SUBST(XIC_SHARED_LIBADD)

  PHP_NEW_EXTENSION(xic, [	\
		php_xic.cpp util.cpp Connection.cpp xs_XError.cpp \
		xic_Engine.cpp xic_Proxy.cpp xic_Exception.cpp \
		vbs_Blob.cpp vbs_Dict.cpp vbs_Decimal.cpp vbs_Data.cpp \
		vbs_codec.cpp smart_write.c \
		xslib/XRefCount.cpp xslib/XError.cpp xslib/XLock.cpp \
		xslib/cxxstr.cpp xslib/xio_cxx.cpp \
		xslib/Srp6a.cpp \
		xslib/vbs_pack.c xslib/xnet.c xslib/urandom.c \
		xslib/xstr.c xslib/bset.c xslib/xstr_cxx.cpp \
		xslib/rope.c xslib/xio.c xslib/xmem.c xslib/msec.c \
		xslib/xlog.c xslib/xformat.c xslib/iobuf.c \
		xslib/ostk.c xslib/xbase32.c xslib/xbase57.c xslib/xbuf.c \
		xslib/decContext.c xslib/decDouble.c xslib/decQuad.c \
		xslib/decimal64.c xslib/mp.c xslib/bit.c \
		xslib/unixfs.c xslib/vbs.c xslib/sha1.c xslib/sha256.c \
		xslib/rijndael.c xslib/aes_eax.c xslib/cmac_aes.c \
		dlog/dlog.c dlog/dlog_imp.c dlog/misc.c \
		xic/SecretBox.cpp xic/VData.cpp xic/VWriter.cpp \
		xic/XicMessage.cpp xic/XicCheck.cpp xic/XicException.cpp \
		xic/Context.cpp xic/MyCipher.cpp \
	], $ext_shared)
fi
