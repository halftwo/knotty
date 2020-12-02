#include "vbs_codec.h"
#include "vbs_Blob.h"
#include "vbs_Dict.h"
#include "vbs_Decimal.h"
#include "vbs_Data.h"

namespace vbs
{

static int array_is_dict(zval *val TSRMLS_DC)
{
	int i;
	HashTable *myht = HASH_OF(val);

	i = myht ? zend_hash_num_elements(myht) : 0;
	if (i > 0)
	{
		char *key;
		ulong index, idx;
		uint key_len;
		HashPosition pos;

		zend_hash_internal_pointer_reset_ex(myht, &pos);
		idx = 0;
		for (;; zend_hash_move_forward_ex(myht, &pos)) {
			i = zend_hash_get_current_key_ex(myht, &key, &key_len, &index, 0, &pos);
			if (i == HASH_KEY_NON_EXISTANT)
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
	HashTableRef(HashTable *ht)
		: _ht(ht)
	{
		if (_ht)
			_ht->nApplyCount++;
	}

	~HashTableRef()
	{
		if (_ht)
			_ht->nApplyCount--;
	}
	
};

static void v_encode_list_without_headtail(vbs_packer_t *job, zval *arr TSRMLS_DC)
{
	HashTable *ht = Z_ARRVAL_P(arr);

	if (ht->nApplyCount > 1)
	{
		throw XERROR_MSG(EncodeError, "circular references is not supported");
	}

	HashTableRef dummy(ht);
	int num = ht ? zend_hash_num_elements(ht) : 0;

	if (num > 0)
	{
		ulong idx;
		char *key;
		uint key_len;
		HashPosition pos;
		HashTable *tmp_ht;
		zval **data;

		zend_hash_internal_pointer_reset_ex(ht, &pos);
		for (; true; zend_hash_move_forward_ex(ht, &pos))
		{
			if (zend_hash_get_current_data_ex(ht, (void **) &data, &pos) != SUCCESS)
				break;

			tmp_ht = HASH_OF(*data);
			HashTableRef dummy(tmp_ht);

			v_encode_r(job, *data TSRMLS_CC);
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

	if (ht && ht->nApplyCount > 1)
	{
		throw XERROR_MSG(EncodeError, "circular references is not supported");
	}

	HashTableRef dummy(ht);
	int num = ht ? zend_hash_num_elements(ht) : 0;
	vbs_pack_head_of_dict0(job);

	if (num > 0)
	{
		ulong idx;
		char *key;
		uint key_len;
		HashPosition pos;
		HashTable *tmp_ht;
		zval **data;

		zend_hash_internal_pointer_reset_ex(ht, &pos);
		for (; true; zend_hash_move_forward_ex(ht, &pos))
		{
			int x = zend_hash_get_current_key_ex(ht, &key, &key_len, &idx, 0, &pos);
			if (x == HASH_KEY_NON_EXISTANT)
				break;

			if (zend_hash_get_current_data_ex(ht, (void **) &data, &pos) != SUCCESS)
				continue;

			tmp_ht = HASH_OF(*data);
			HashTableRef dummy(tmp_ht);

			if (x == HASH_KEY_IS_STRING)
			{
				if (key[0] == '\0' && Z_TYPE_P(arr) == IS_OBJECT)
				{
					/* Skip protected and private members. */
					continue;
				}
				/* The key length including the trailing NUL character. */
				vbs_pack_lstr(job, key, (key_len - 1));
			}
			else
			{
				vbs_pack_integer(job, idx);
			}

			v_encode_r(job, *data TSRMLS_CC);
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

	case IS_BOOL:
		vbs_pack_bool(job, Z_BVAL_P(val));
		break;

	case IS_NULL:
		vbs_pack_null(job);
		break;
	
	case IS_DOUBLE:
		vbs_pack_floating(job, Z_DVAL_P(val));
		break;

	case IS_OBJECT:
		if (zend_get_class_entry(val TSRMLS_CC) == vbs::classEntry_Blob)
		{
			zval *zv = zend_read_property(vbs::classEntry_Blob, val, "s", sizeof("s") - 1, 0 TSRMLS_CC);
			vbs_pack_blob(job, Z_STRVAL_P(zv), Z_STRLEN_P(zv));
			break;
		}
		else if (zend_get_class_entry(val TSRMLS_CC) == vbs::classEntry_Dict)
		{
			zval *zv = zend_read_property(vbs::classEntry_Dict, val, "a", sizeof("a") - 1, 0 TSRMLS_CC);
			return v_encode_dict(job, zv TSRMLS_CC);
			break;
		}
		else if (Z_OBJCE_P(val) == vbs::classEntry_Decimal)
		{
			zval *zv = zend_read_property(vbs::classEntry_Decimal, val, "s", sizeof("s") - 1, 0 TSRMLS_CC);
			decimal64_t dec;
			decimal64_from_cstr(&dec, Z_STRVAL_P(zv), NULL);
			vbs_pack_decimal64(job, dec);
			break;
		}
		else if (Z_OBJCE_P(val) == vbs::classEntry_Data)
		{
			zval *zv1 = zend_read_property(vbs::classEntry_Data, val, "r", sizeof("r") - 1, 0 TSRMLS_CC);
			int descriptor = Z_LVAL_P(zv1);
			vbs_pack_descriptor(job, descriptor);

			zval *zv2 = zend_read_property(vbs::classEntry_Data, val, "d", sizeof("d") - 1, 0 TSRMLS_CC);
			v_encode_r(job, zv2);
			break;
		}
		/* fall through */
	case IS_ARRAY:
		v_encode_array(job, val TSRMLS_CC);
		break;

	default:
		throw XERROR_FMT(EncodeError, "Can't encode php type(%d:%s)", Z_TYPE_P(val), zend_get_type_by_const(Z_TYPE_P(val)));
	}
}

void v_pack(vbs_packer_t *job, zval *arr TSRMLS_DC)
{
	v_encode_list_without_headtail(job, arr TSRMLS_CC);
}

void v_encode_args(vbs_packer_t *job, zval *args TSRMLS_DC)
{
	if (Z_TYPE_P(args) == IS_OBJECT && zend_get_class_entry(args TSRMLS_CC) == vbs::classEntry_Dict)
	{
		zval *zv = zend_read_property(vbs::classEntry_Dict, args, "a", sizeof("a") - 1, 0 TSRMLS_CC);
		args = zv;
	}
	else if (Z_TYPE_P(args) != IS_ARRAY)
	{
		throw XERROR_FMT(EncodeError, "Parameters should be contained in an array or vbs_dict object");
	}

	HashTable *ht = Z_ARRVAL_P(args);

	HashTableRef dummy(ht);

	int num = ht ? zend_hash_num_elements(ht) : 0;
	vbs_pack_head_of_dict0(job);
	if (num > 0)
	{
		ulong idx;
		char *key;
		uint key_len;
		HashPosition pos;
		HashTable *tmp_ht;
		zval **data;

		zend_hash_internal_pointer_reset_ex(ht, &pos);
		for (; true; zend_hash_move_forward_ex(ht, &pos))
		{
			int x = zend_hash_get_current_key_ex(ht, &key, &key_len, &idx, 0, &pos);
			if (x == HASH_KEY_NON_EXISTANT)
				break;

			if (zend_hash_get_current_data_ex(ht, (void **) &data, &pos) != SUCCESS)
				continue;

			tmp_ht = HASH_OF(*data);
			HashTableRef dummy(tmp_ht);

			if (x == HASH_KEY_IS_STRING)
			{
				/* The key length including the trailing NUL character. */
				vbs_pack_lstr(job, key, (key_len - 1));
				v_encode_r(job, *data TSRMLS_CC);
			}
			else
			{
				throw XERROR_MSG(EncodeError, "parameter dict should only have string key");
			}
		}
	}
	vbs_pack_tail(job);
}

void v_encode_ctx(vbs_packer_t *job, zval *ctx TSRMLS_DC)
{
	v_encode_args(job, ctx TSRMLS_CC);
}

static bool v_decode_array(vbs_unpacker_t *job, bool is_dict, zval *z TSRMLS_DC)
{
	size_t i;

	array_init(z);
	for (i = 0; true; ++i)
	{
		zval *ev;

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
					add_index_zval(z, dat.d_int, ev);
				}
				else
				{
					char key[40];
					snprintf(key, sizeof(key), "%jd", dat.d_int);
					add_assoc_zval(z, key, ev);
				}
			}
			else if (dat.kind == VBS_STRING)
			{
				xstr_t *key = &dat.d_xstr;
				char saved_ch = key->data[key->len];

				if (!v_decode_r(job, &ev TSRMLS_CC))
				{
					goto error;
				}

				key->data[key->len] = 0;
				add_assoc_zval(z, (char *)key->data, ev);
				key->data[key->len] = saved_ch;
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

			add_next_index_zval(z, ev);
		}
	}
	return true;

error:
	zval_dtor(z);
	return false;
}

bool v_decode_r(vbs_unpacker_t *job, zval **zz TSRMLS_DC)
{
	vbs_data_t dat;
	int kind;

	if (vbs_unpack_primitive(job, &dat, &kind) < 0)
		return false;

	ALLOC_INIT_ZVAL(*zz);

	if (dat.kind == VBS_INTEGER)
	{
		if (dat.d_int >= LONG_MIN && dat.d_int <= LONG_MAX)
		{
			ZVAL_LONG(*zz, dat.d_int);
		}
		else
		{
			char tmp[40];
			int len = snprintf(tmp, sizeof(tmp), "%jd", dat.d_int);
			ZVAL_STRINGL(*zz, tmp, len, 1);
		}
	}
	else if (dat.kind == VBS_STRING)
	{
		ZVAL_STRINGL(*zz, (char *)dat.d_xstr.data, dat.d_xstr.len, 1);
	}
	else if (dat.kind == VBS_BLOB)
	{
		zval *zv;
		MAKE_STD_ZVAL(zv);
		ZVAL_STRINGL(zv, (char *)dat.d_blob.data, dat.d_blob.len, 1);
		vbs::create_Blob(*zz, zv TSRMLS_CC);
		ZVAL_DELREF(zv);
	}
	else if (dat.kind == VBS_BOOL)
	{
		ZVAL_BOOL(*zz, dat.d_bool);
	}
	else if (dat.kind == VBS_FLOATING)
	{
		ZVAL_DOUBLE(*zz, dat.d_floating);
	}
	else if (dat.kind == VBS_DECIMAL)
	{
		zval *zv;
		char buf[DECIMAL64_STRING_MAX];
		int len = decimal64_to_cstr(dat.d_decimal64, buf);

		MAKE_STD_ZVAL(zv);
		ZVAL_STRINGL(zv, buf, len, 1);
		vbs::create_DecimalNoCheck(*zz, zv TSRMLS_CC);
		ZVAL_DELREF(zv);
	}
	else if (dat.kind == VBS_LIST)
	{
		if (!v_decode_array(job, false, *zz TSRMLS_CC))
			goto error;
	}
	else if (dat.kind == VBS_DICT)
	{
		if (!v_decode_array(job, true, *zz TSRMLS_CC))
			goto error;
	}
	else if (dat.kind == VBS_NULL)
	{
		ZVAL_NULL(*zz);
	}
	else
	{
		goto error;
	}

	return true;

error:
	FREE_ZVAL(*zz);
	return false;
}

bool v_unpack(vbs_unpacker_t *job, long num, zval **zz TSRMLS_DC)
{
	long i;

	if (num <= 0)
		num = LONG_MAX;

	ALLOC_INIT_ZVAL(*zz);
	array_init(*zz);
	for (i = 0; i < num; ++i)
	{
		zval *ev;
		if (job->cur >= job->end)
			break;

		if (!v_decode_r(job, &ev TSRMLS_CC))
		{
			goto error;
		}

		add_next_index_zval(*zz, ev);
	}
	return true;

error:
	zval_dtor(*zz);
	FREE_ZVAL(*zz);
	return false;
}

};

