#include "xslib/xsdef.h"
#include "php_xic.h"
#include "util.h"
#include "vbs_codec.h"
#include "xslib/ScopeGuard.h"
#include "xslib/vbs_pack.h"
#include "xslib/xio.h"
#include "xslib/ostk.h"
#include "xslib/xbase57.h"
#include "xslib/urandom.h"
#include "xslib/xnet.h"
#include "zend_API.h"
#include "zend_variables.h"
#include <string>
#include <sstream>

#define STRCAST(x)	((char *)(x))


zend_string *get_ClassName(zval *obj)
{
	zend_string *s = NULL;
	if (Z_TYPE_P(obj) == IS_OBJECT && Z_OBJ_HANDLER_P(obj, get_class_name))
	{
		s = Z_OBJ_HANDLER_P(obj, get_class_name)(Z_OBJ_P(obj));
	}

	return s;
}

void *_create_MyObject(zend_class_entry *ce, size_t size TSRMLS_DC)
{
	zend_object *obj = static_cast<zend_object*>(ecalloc(1, size));

	zend_object_std_init(obj, ce TSRMLS_CC);
	object_properties_init(obj, ce);
	return obj;
}

void *_extract_MyObject(zval *zv TSRMLS_DC)
{
	zend_object *obj = Z_OBJ_P(zv);
	if (!obj)
	{
		raise_Exception(0 TSRMLS_CC, "No object found in %s()", get_active_function_name(TSRMLS_C));
		return 0;
	}
	return obj;
}

void raise_Exception(long code TSRMLS_DC, const char *format, ...)
{
	va_list ap;
	char buf[1024];

	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);

	zend_object *ex = zend_throw_exception(zend_exception_get_default(TSRMLS_C), buf, code);
}


std::string get_self_process(const char *self_id/*NULL*/)
{
	std::string self_process = "XXX-UNKNOWN.php";
	
	// NB: It seems we should call this zend_is_auto_global() to make 
	// $_SERVER effective when we call get_self_process() in RINIT. 
	// I don't know why, but it works.
	if (!zend_is_auto_global_str((char *)"_SERVER", 7))
		return self_process;

	zval* arr = zend_hash_str_find(&EG(symbol_table), (char *)"_SERVER", 7);
	if (!arr)
		return self_process;

	HashTable* ht = HASH_OF(arr);
	if (!ht)
		return self_process;

	zval* zz = NULL;

#define GETX(VAR, NAME, VALUE)						\
	xstr_t VAR = XSTR_CONST(VALUE);					\
	zz = zend_hash_str_find(ht, NAME, sizeof(NAME) - 1);		\
	if (zz) \
	{ (VAR).data = (unsigned char *)Z_STRVAL_P(zz); (VAR).len = Z_STRLEN_P(zz); }

	ostk_t *ostk = ostk_create(0);

	GETX(method, "REQUEST_METHOD", "CLI");
	GETX(script_filename, "SCRIPT_FILENAME", "/XXX.php");
	xstr_t program = script_filename;
	if (xstr_equal_cstr(&method, "CLI"))
	{
		if (!xstr_char_equal(&script_filename, 0, '/'))
		{
			GETX(pwd, "PWD", "");
			if (pwd.len > 0)
			{
				program = ostk_xstr_printf(ostk, "%.*s/%.*s", XSTR_P(&pwd), XSTR_P(&script_filename));
			}
		}
	}
	else
	{
		GETX(uri, "REQUEST_URI", "/XXX.php");
		GETX(php_self, "PHP_SELF", "/XXX.php");
		GETX(server, "SERVER_NAME", "");
		xstr_t xs = uri;
		xstr_delimit_char(&xs, '?', &uri);
		if (xstr_equal(&uri, &php_self) || php_self.len == 0)
		{
			program = server.len ? ostk_xstr_printf(ostk, "%.*s%.*s", XSTR_P(&server), XSTR_P(&uri))
						: uri;
		}
		else
		{
			program = ostk_xstr_printf(ostk, "%.*s%.*s|%.*s", XSTR_P(&server), XSTR_P(&uri), XSTR_P(&php_self));
		}
	}
#undef GETX

	xstr_t xs;
	if (self_id)
		xs = ostk_xstr_printf(ostk, "%.*s+%.*s+%s", XSTR_P(&method), XSTR_P(&program), self_id);
	else
		xs = ostk_xstr_printf(ostk, "%.*s+%.*s", XSTR_P(&method), XSTR_P(&program));
	self_process.assign((char *)xs.data, (size_t)xs.len);
	ostk_destroy(ostk);
	return self_process;
}

std::string get_default_ctx()
{
	return pack_ctx(NULL);
}

std::string pack_ctx(zval *ctx)
{
	std::ostringstream os;
	vbs_packer_t pk = VBS_PACKER_INIT(ostream_xio.write, (std::ostream*)&os, 1);

	vbs_pack_head_of_dict0(&pk);

	zval* myrid = get_xic_rid();
	if (myrid)
	{
		char buf[24];
		int len = generate_xic_rid(buf, Z_STRVAL_P(myrid), Z_STRLEN_P(myrid));
		vbs_pack_lstr(&pk, "RID", 3);
		vbs_pack_lstr(&pk, buf, len);
	}

	zval* caller = get_xic_self();
	if (caller)
	{
		vbs_pack_lstr(&pk, "CALLER", 6);
		vbs_pack_lstr(&pk, Z_STRVAL_P(caller), Z_STRLEN_P(caller));
	}

	if (ctx)
	{
		vbs::v_encode_args_without_headtail(&pk, ctx TSRMLS_CC);
	}

	vbs_pack_tail(&pk);
	if (pk.error)
		throw XERROR_MSG(XError, "Invalid context");

	return os.str();
}


int generate_xic_rid(char buf[24], const char *myrid, int len)
{
	uint8_t r[8];
	urandom_get_bytes(r, sizeof(r));
	r[0] &= 0x7f;
	xbase57_encode(buf, r, sizeof(r));

	if (len > 0)
	{
		const char *p = (const char *)memchr(myrid, '-', len);
		if (p)
			len = p - myrid;
		if (len > 11)
			len = 11;
	}

	if (len <= 0)
	{
		buf[11] = 0;
		return 11;
	}

	buf[11] = '-';
	memcpy(buf + 12, myrid, len);
	buf[12+len] = 0;
	return 12 + len;
}

// Obselete
static void update_ctx_caller(zval *ctx)
{
	zval* caller = get_xic_self();
	if (caller)
	{
		HashTable *ht = HASH_OF(ctx);
		if (ht)
		{
			Z_ADDREF_P(caller);
			zend_hash_str_update(ht, "CALLER", sizeof("CALLER") - 1, caller);
		}
	}
}


