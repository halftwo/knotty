#include "xslib/xsdef.h"
#include "php_xic.h"
#include "util.h"
#include "xslib/ScopeGuard.h"
#include "xslib/vbs_pack.h"
#include "xslib/xio.h"
#include "xslib/ostk.h"
#include "xslib/xbase32.h"
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

static uint16_t _get_pid_base32(char buf[3])
{
	uint16_t pid = getpid();

	buf[0] = xbase32_alphabet[(pid >> 10) & 0x1F];
	buf[1] = xbase32_alphabet[(pid >> 5) & 0x1F];
	buf[2] = xbase32_alphabet[pid & 0x1F];
	return pid;
}

ssize_t get_self_process_id(char *id, size_t size)
{
	assert((ssize_t)size >= 8);

	int len = size - 4;
	int half = len / 2;
	urandom_generate_base57id(id, len + 1);
	memmove(id + half + 3, id + half, len - half);
	_get_pid_base32(id + half);
	len += 3;
	id[len] = 0;
	return len;
}

std::string get_self_process(const char *self_id)
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

	xstr_t xs = ostk_xstr_printf(ostk, "%.*s+%.*s+%s", XSTR_P(&method), XSTR_P(&program), self_id);
	self_process.assign((char *)xs.data, (size_t)xs.len);
	ostk_destroy(ostk);
	return self_process;
}

std::string get_default_ctx()
{
	zval* caller = get_xic_self();
	if (caller)
	{
		vbs_packer_t pk;
		std::ostringstream os;
		vbs_packer_init(&pk, ostream_xio.write, (std::ostream*)&os, 1);
		vbs_pack_head_of_dict0(&pk);
		vbs_pack_lstr(&pk, "CALLER", 6);
		vbs_pack_lstr(&pk, Z_STRVAL_P(caller), Z_STRLEN_P(caller));
		vbs_pack_tail(&pk);
		return os.str();
	}
	return std::string();
}

void update_ctx_caller(zval *ctx)
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

