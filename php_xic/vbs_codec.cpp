#include "vbs_codec.h"
#include "vbs_Blob.h"
#include "vbs_Dict.h"
#include "vbs_Decimal.h"
#include "vbs_Data.h"
#include "xslib/decimal64.h"

namespace vbs
{

static int array_is_dict(zval *val TSRMLS_DC)
{
	int i;
	HashTable *myht = HASH_OF(val);

	i = myht ? zend_hash_num_elements(myht) : 0;
	if (i > 0)
	{
		zend_string *key;
		ulong index, idx;
		HashPosition pos;

		zend_hash_internal_pointer_reset_ex(myht, &pos);
		idx = 0;
		for (;; zend_hash_move_forward_ex(myht, &pos)) {
			i = zend_hash_get_current_key_ex(myht, &key, &index, &pos);
			if (i == HASH_KEY_NON_EXISTENT)
				break;

			if (i == HASH_KEY_IS_STRING) {
				return 1;
			} else {
				if (index != idx) {
					return 1;
				}
			}
			idx++;
		}
	}
	return 0;
}

class HashTableRef
{
	HashTable *_ht;
public:
#if PHP_VERSION_ID < 70300
	HashTableRef(HashTable *ht)
		: _ht(ht)
	{
		if (_ht)
			_ht->u.v.nApplyCount++;
	}

	~HashTableRef()
	{
		if (_ht)
			_ht->u.v.nApplyCount--;
	}
#else
	HashTableRef(HashTable *ht)
		: _ht(ht)
	{
		if (_ht && !(GC_FLAGS(_ht) & GC_IMMUTABLE))
			GC_PROTECT_RECURSION(_ht);
	}

	~HashTableRef()
	{
		if (_ht && !(GC_FLAGS(_ht) & GC_IMMUTABLE))
			GC_UNPROTECT_RECURSION(_ht);
	}
#endif
};

static void v_encode_list_without_headtail(vbs_packer_t *job, zval *arr TSRMLS_DC)
{
	HashTable *ht = Z_ARRVAL_P(arr);

#if PHP_VERSION_ID < 70300
	if (ht->u.v.nApplyCount > 1)
#else
	if (!(GC_FLAGS(ht) & GC_IMMUTABLE) && GC_IS_RECURSIVE(ht))
#endif
	{
		throw XERROR_MSG(EncodeError, "circular references is not supported");
	}

	HashTableRef dummy(ht);
	int num = ht ? zend_hash_num_elements(ht) : 0;

	if (num > 0)
	{
		HashPosition pos;

		zend_hash_internal_pointer_reset_ex(ht, &pos);
		for (; true; zend_hash_move_forward_ex(ht, &pos))
		{
			zval *data = zend_hash_get_current_data_ex(ht, &pos);
			if (!data)
				break;

			v_encode_r(job, data TSRMLS_CC);
		}
	}
}

static void v_encode_list(vbs_packer_t *job, zval *arr TSRMLS_DC)
{
	vbs_pack_head_of_list0(job);
	v_encode_list_without_headtail(job, arr TSRMLS_CC);
	vbs_pack_tail(job);
}

static void v_encode_dict(vbs_packer_t *job, zval *arr TSRMLS_DC)
{
	HashTable *ht = HASH_OF(arr);

#if PHP_VERSION_ID < 70300
	if (ht && ht->u.v.nApplyCount > 1)
#else
	if (ht && !(GC_FLAGS(ht) & GC_IMMUTABLE) && GC_IS_RECURSIVE(ht))
#endif
	{
		throw XERROR_MSG(EncodeError, "circular references is not supported");
	}

	HashTableRef dummy(ht);
	int num = ht ? zend_hash_num_elements(ht) : 0;
	vbs_pack_head_of_dict0(job);

	if (num > 0)
	{
		ulong idx;
		zend_string *key;
		HashPosition pos;

		zend_hash_internal_pointer_reset_ex(ht, &pos);
		for (; true; zend_hash_move_forward_ex(ht, &pos))
		{
			int x = zend_hash_get_current_key_ex(ht, &key, &idx, &pos);
			if (x == HASH_KEY_NON_EXISTENT)
				break;

			zval *data = zend_hash_get_current_data_ex(ht, &pos);
			if (!data)
				continue;

			if (x == HASH_KEY_IS_STRING)
			{
				if (key->val[0] == '\0' && Z_TYPE_P(arr) == IS_OBJECT)
				{
					/* Skip protected and private members. */
					continue;
				}
				vbs_pack_lstr(job, key->val, key->len);
			}
			else
			{
				vbs_pack_integer(job, idx);
			}

			v_encode_r(job, data TSRMLS_CC);
		}
	}

	vbs_pack_tail(job);
}

static inline void v_encode_array(vbs_packer_t *job, zval *arr TSRMLS_DC)
{
	bool is_dict = false;

	if (Z_TYPE_P(arr) == IS_ARRAY) {
		is_dict = array_is_dict(arr TSRMLS_CC);
	} else {
		is_dict = true;
	}

	return is_dict ? v_encode_dict(job, arr TSRMLS_CC) : v_encode_list(job, arr TSRMLS_CC);
}

void v_encode_r(vbs_packer_t *job, zval *val TSRMLS_DC)
{
	switch(Z_TYPE_P(val))
	{
	case IS_STRING:
		vbs_pack_lstr(job, Z_STRVAL_P(val), Z_STRLEN_P(val));
		break;
		
	case IS_LONG:
		vbs_pack_integer(job, Z_LVAL_P(val));
		break;

	case IS_TRUE:
		vbs_pack_bool(job, true);
		break;

	case IS_FALSE:
		vbs_pack_bool(job, false);
		break;

	case IS_NULL:
		vbs_pack_null(job);
		break;
	
	case IS_DOUBLE:
		vbs_pack_floating(job, Z_DVAL_P(val));
		break;

	case IS_OBJECT:
		if (Z_OBJCE_P(val) == vbs::classEntry_Blob)
		{
			zval rv;
			ZVAL_UNDEF(&rv);
			zval *zv = zend_read_property(vbs::classEntry_Blob, MY_Z_OBJ_P(val), "s", sizeof("s") - 1, 0, &rv TSRMLS_CC);
			vbs_pack_blob(job, Z_STRVAL_P(zv), Z_STRLEN_P(zv));
			break;
		}
		else if (Z_OBJCE_P(val) == vbs::classEntry_Dict)
		{
			zval rv;
			ZVAL_UNDEF(&rv);
			zval *zv = zend_read_property(vbs::classEntry_Dict, MY_Z_OBJ_P(val), "a", sizeof("a") - 1, 0, &rv TSRMLS_CC);
			v_encode_dict(job, zv TSRMLS_CC);
			break;
		}
		else if (Z_OBJCE_P(val) == vbs::classEntry_Decimal)
		{
			zval rv;
			ZVAL_UNDEF(&rv);
			zval *zv = zend_read_property(vbs::classEntry_Decimal, MY_Z_OBJ_P(val), "s", sizeof("s") - 1, 0, &rv TSRMLS_CC);
			decimal64_t dec;
			decimal64_from_cstr(&dec, Z_STRVAL_P(zv), NULL);
			vbs_pack_decimal64(job, dec);
			break;
		}
		else if (Z_OBJCE_P(val) == vbs::classEntry_Data)
		{
			zval rv1, rv2;

			zval *r = zend_read_property(vbs::classEntry_Data, MY_Z_OBJ_P(val), "r", sizeof("r") - 1, 0, &rv1 TSRMLS_CC);
			int descriptor = Z_LVAL_P(r);
			vbs_pack_descriptor(job, descriptor);

			zval *d = zend_read_property(vbs::classEntry_Data, MY_Z_OBJ_P(val), "d", sizeof("d") - 1, 0, &rv2 TSRMLS_CC);
			v_encode_r(job, d);
			break;
		}

		/* fall through */
	case IS_ARRAY:
		v_encode_array(job, val TSRMLS_CC);
		break;

	case IS_REFERENCE:
		v_encode_r(job, Z_REFVAL_P(val));
		break;

	case IS_INDIRECT:
		v_encode_r(job, Z_INDIRECT_P(val));
		break;

	default:
		throw XERROR_FMT(EncodeError, "Can't encode php type(%d:%s)", Z_TYPE_P(val), zend_get_type_by_const(Z_TYPE_P(val)));
	}
}

void v_pack(vbs_packer_t *job, zval *arr TSRMLS_DC)
{
	v_encode_list_without_headtail(job, arr TSRMLS_CC);
}

void v_encode_args_without_headtail(vbs_packer_t *job, zval *args TSRMLS_DC)
{
	if (VALID_OBJECT(args, vbs::classEntry_Dict))
	{
		zval rv;
		ZVAL_UNDEF(&rv);
		zval *zv = zend_read_property(vbs::classEntry_Dict, MY_Z_OBJ_P(args), "a", sizeof("a") - 1, 0, &rv TSRMLS_CC);
		args = zv;
	}
	else if (Z_TYPE_P(args) != IS_ARRAY)
	{
		throw XERROR_FMT(EncodeError, "Parameters should be contained in an array or vbs_dict object");
	}

	HashTable *ht = Z_ARRVAL_P(args);

	HashTableRef dummy(ht);

	int num = ht ? zend_hash_num_elements(ht) : 0;
	if (num > 0)
	{
		HashPosition pos;

		zend_hash_internal_pointer_reset_ex(ht, &pos);
		for (; true; zend_hash_move_forward_ex(ht, &pos))
		{
			ulong idx;
			zend_string *key;
			int x = zend_hash_get_current_key_ex(ht, &key, &idx, &pos);
			if (x == HASH_KEY_NON_EXISTENT)
				break;

			zval *data = zend_hash_get_current_data_ex(ht, &pos);
			if (!data)
				continue;

			if (x == HASH_KEY_IS_STRING)
			{
				vbs_pack_lstr(job, key->val, key->len);
				v_encode_r(job, data TSRMLS_CC);
			}
			else
			{
				throw XERROR_MSG(EncodeError, "parameter dict should only have string key");
			}
		}
	}
}

void v_encode_args(vbs_packer_t *job, zval *args TSRMLS_DC)
{
	vbs_pack_head_of_dict0(job);
	v_encode_args_without_headtail(job, args TSRMLS_CC);
	vbs_pack_tail(job);
}

static bool v_decode_array(vbs_unpacker_t *job, bool is_dict, zval *z TSRMLS_DC)
{
	size_t i;

	array_init(z);
	for (i = 0; true; ++i)
	{
		zval ev;

		if (vbs_unpack_if_tail(job))
			break;

		if (is_dict)
		{
			vbs_data_t dat;
			if (vbs_unpack_primitive(job, &dat, NULL) < 0)
				goto error;

			if (dat.kind == VBS_INTEGER)
			{
				if (!v_decode_r(job, &ev TSRMLS_CC))
				{
					goto error;
				}

				if (dat.d_int >= LONG_MIN && dat.d_int <= LONG_MAX)
				{
					add_index_zval(z, dat.d_int, &ev);
				}
				else
				{
					char key[40];
					snprintf(key, sizeof(key), "%jd", dat.d_int);
					add_assoc_zval(z, key, &ev);
				}
			}
			else if (dat.kind == VBS_STRING)
			{
				xstr_t *key = &dat.d_xstr;
				if (!v_decode_r(job, &ev TSRMLS_CC))
				{
					goto error;
				}

				add_assoc_zval_ex(z, (char *)key->data, key->len, &ev);
			}
			else
			{
				goto error;
			}
		}
		else
		{
			if (!v_decode_r(job, &ev TSRMLS_CC))
			{
				goto error;
			}

			add_next_index_zval(z, &ev);
		}
	}
	return true;

error:
	zval_dtor(z);
	return false;
}

bool v_decode_r(vbs_unpacker_t *job, zval *zz TSRMLS_DC)
{
	vbs_data_t dat;
	int kind;

	ZVAL_UNDEF(zz);

	if (vbs_unpack_primitive(job, &dat, &kind) < 0)
		return false;

	if (dat.kind == VBS_INTEGER)
	{
		if (dat.d_int >= LONG_MIN && dat.d_int <= LONG_MAX)
		{
			ZVAL_LONG(zz, dat.d_int);
		}
		else
		{
			char tmp[40];
			int len = snprintf(tmp, sizeof(tmp), "%jd", dat.d_int);
			ZVAL_STRINGL(zz, tmp, len);
		}
	}
	else if (dat.kind == VBS_STRING)
	{
		ZVAL_STRINGL(zz, (char *)dat.d_xstr.data, dat.d_xstr.len);
	}
	else if (dat.kind == VBS_BLOB)
	{
		if (job->flags & FLAG_BLOB2STRING)
		{
			ZVAL_STRINGL(zz, (char *)dat.d_blob.data, dat.d_blob.len);
		}
		else
		{
			zval zv;
			ZVAL_UNDEF(&zv);
			ZVAL_STRINGL(&zv, (char *)dat.d_blob.data, dat.d_blob.len);
			vbs::create_Blob(zz, &zv TSRMLS_CC);
			Z_DELREF_P(&zv);
		}
	}
	else if (dat.kind == VBS_BOOL)
	{
		ZVAL_BOOL(zz, dat.d_bool);
	}
	else if (dat.kind == VBS_FLOATING)
	{
		ZVAL_DOUBLE(zz, dat.d_floating);
	}
	else if (dat.kind == VBS_DECIMAL)
	{
		zval zv;
		char buf[DECIMAL64_STRING_MAX];
		int len = decimal64_to_cstr(dat.d_decimal64, buf);

		ZVAL_UNDEF(&zv);
		ZVAL_STRINGL(&zv, buf, len);
		vbs::create_DecimalNoCheck(zz, &zv TSRMLS_CC);
		Z_DELREF_P(&zv);
	}
	else if (dat.kind == VBS_LIST)
	{
		if (!v_decode_array(job, false, zz TSRMLS_CC))
			goto error;
	}
	else if (dat.kind == VBS_DICT)
	{
		if (!v_decode_array(job, true, zz TSRMLS_CC))
			goto error;
	}
	else if (dat.kind == VBS_NULL)
	{
		ZVAL_NULL(zz);
	}
	else
	{
		goto error;
	}

	return true;

error:
	return false;
}

bool v_unpack(vbs_unpacker_t *job, long num, zval *zz TSRMLS_DC)
{
	long i;

	if (num <= 0)
		num = LONG_MAX;

	array_init(zz);
	for (i = 0; i < num; ++i)
	{
		if (job->cur >= job->end)
			break;

		zval ev;
		if (!v_decode_r(job, &ev TSRMLS_CC))
		{
			goto error;
		}

		add_next_index_zval(zz, &ev);
	}
	return true;

error:
	zval_dtor(zz);
	return false;
}

};

