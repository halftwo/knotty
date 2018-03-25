#include "VData.h"
#include "XicException.h"

namespace xic
{

void _vdata_throw_TypeException(const vbs_data_t* v, const char *kind)
{
	throw XERROR_FMT(xic::ParameterTypeException,
		"Data kind should be %s instead of %s",
		kind, vbs_kind_name(v->kind));
}

void _vdata_throw_DataException()
{
	throw XERROR(xic::ParameterDataException);
}


VList::VList(const vbs_list_t *ls)
	: _list(ls)
{
	if (!_list)
		throw XERROR_MSG(XArgumentError, "Null pointer for vbs_list_t");
}


static void throwListNodeException(vbs_litem_t *ent, const char *needType)
{
	if (ent)
	{
		throw XERROR_FMT(xic::ParameterTypeException, 
			"Kind of element in the list should be %s instead of %s",
			needType, vbs_kind_name(ent->value.kind));
	}
	else
	{
		throw XERROR_MSG(XLogicError, "Null pointer for vbs_litem_t");
	}
}

#define	CHECK(ENT, TYPE) 		\
	if (!(ENT) || (ENT)->value.kind != VBS_##TYPE) throwListNodeException((ENT), #TYPE)


void VList::Node::expectNullValue() const
{
	CHECK(_ent, NULL);
}

intmax_t VList::Node::intValue() const
{
	CHECK(_ent, INTEGER);
	return _ent->value.d_int;
}

const xstr_t& VList::Node::xstrValue() const
{
	CHECK(_ent, STRING);
	return _ent->value.d_xstr;
}

const xstr_t& VList::Node::blobValue() const
{
	if (!_ent || _ent->value.kind != VBS_BLOB)
	{
		if (_ent && _ent->value.kind == VBS_STRING)
			return _ent->value.d_xstr;

		throwListNodeException(_ent, "BLOB");
	}
	return _ent->value.d_blob;
}

bool VList::Node::boolValue() const
{
	CHECK(_ent, BOOL);
	return _ent->value.d_bool;
}

double VList::Node::floatingValue() const
{
	if (!_ent || _ent->value.kind != VBS_FLOATING)
	{
		if (_ent && _ent->value.kind == VBS_INTEGER)
			return (double)_ent->value.d_int;

		throwListNodeException(_ent, "FLOATING");
	}
	return _ent->value.d_floating;
}

decimal64_t VList::Node::decimal64Value() const
{
	if (!_ent || _ent->value.kind != VBS_DECIMAL)
	{
		if (_ent && _ent->value.kind == VBS_INTEGER)
		{
			decimal64_t d;
			if (decimal64_from_integer(&d, _ent->value.d_int) == 0)
				return d;
		}

		throwListNodeException(_ent, "DECIMAL");
	}
	return _ent->value.d_decimal64;
}

VList VList::Node::vlistValue() const
{
	CHECK(_ent, LIST);
	return VList(_ent->value.d_list);
}

VDict VList::Node::vdictValue() const
{
	CHECK(_ent, DICT);
	return VDict(_ent->value.d_dict);
}

const vbs_list_t *VList::Node::listValue() const
{
	if (_ent)
	{
		if (_ent->value.kind == VBS_LIST)
			return _ent->value.d_list;
	}

	throwListNodeException(_ent, "LIST");
	return NULL;
}

const vbs_dict_t *VList::Node::dictValue() const
{
	if (_ent)
	{
		if (_ent->value.kind == VBS_DICT)
			return _ent->value.d_dict;
	}

	throwListNodeException(_ent, "DICT");
	return NULL;
}

const vbs_data_t *VList::Node::dataValue() const
{
	if (!_ent)
		throwListNodeException(_ent, "");
	return &_ent->value;
}


static vbs_data_t *_find_key(const vbs_dict_t *dict, const char *key)
{
	vbs_ditem_t *ent;
	for (ent = dict->first; ent; ent = ent->next)
	{
		if (ent->key.kind != VBS_STRING)
			continue;

		if (xstr_equal_cstr(&ent->key.d_xstr, key))
			return &ent->value;
	}
	return NULL;
}

static vbs_data_t *_find_ikey(const vbs_dict_t *dict, intmax_t key)
{
	vbs_ditem_t *ent;
	for (ent = dict->first; ent; ent = ent->next)
	{
		if (ent->key.kind != VBS_INTEGER)
			continue;

		if (ent->key.d_int == key)
			return &ent->value;
	}
	return NULL;
}

VDict::VDict(const vbs_dict_t *dict)
	: _dict(dict)
{
	if (!_dict)
		throw XERROR_MSG(XArgumentError, "Null pointer for vbs_dict_t");
}


static void throwDictKeyException(vbs_ditem_t *ent, const char *needType)
{
	if (ent)
	{
		throw XERROR_FMT(xic::ParameterTypeException,
			"Kind of item key in the dict should be %s instead of %s",
			needType, vbs_kind_name(ent->key.kind));
	}
	else
	{
		throw XERROR_MSG(XLogicError, "Null pointer for vbs_ditem_t");
	}
}

static void throwDictValueException(vbs_ditem_t *ent, const char *needType)
{
	if (ent)
	{
		throw XERROR_FMT(xic::ParameterTypeException,
			"Kind of item value in the dict should be %s instead of %s",
			needType, vbs_kind_name(ent->value.kind));
	}
	else
	{
		throw XERROR_MSG(XLogicError, "Null pointer for vbs_ditem_t");
	}
}

#define	CHECK_KEY(ENT, TYPE) 			\
	if (!(ENT) || (ENT)->key.kind != VBS_##TYPE) throwDictKeyException((ENT), #TYPE)

#define	CHECK_VALUE(ENT, TYPE) 			\
	if (!(ENT) || (ENT)->value.kind != VBS_##TYPE) throwDictValueException((ENT), #TYPE)

intmax_t VDict::Node::intKey() const
{
	CHECK_KEY(_ent, INTEGER);
	return _ent->key.d_int;
}

const xstr_t& VDict::Node::xstrKey() const
{
	CHECK_KEY(_ent, STRING);
	return _ent->key.d_xstr;
}

const vbs_data_t *VDict::Node::dataKey() const
{
	if (!_ent)
		throwDictKeyException(_ent, "");
	return &_ent->key;
}

void VDict::Node::expectNullValue() const
{
	CHECK_VALUE(_ent, NULL);
}

intmax_t VDict::Node::intValue() const
{
	CHECK_VALUE(_ent, INTEGER);
	return _ent->value.d_int;
}

const xstr_t& VDict::Node::xstrValue() const
{
	CHECK_VALUE(_ent, STRING);
	return _ent->value.d_xstr;
}

const xstr_t& VDict::Node::blobValue() const
{
	if (!_ent || _ent->value.kind != VBS_BLOB)
	{
		if (_ent && _ent->value.kind == VBS_STRING)
			return _ent->value.d_xstr;

		throwDictValueException(_ent, "BLOB");
	}
	return _ent->value.d_blob;
}

bool VDict::Node::boolValue() const
{
	CHECK_VALUE(_ent, BOOL);
	return _ent->value.d_bool;
}

double VDict::Node::floatingValue() const
{
	if (!_ent || _ent->value.kind != VBS_FLOATING)
	{
		if (_ent && _ent->value.kind == VBS_INTEGER)
			return (double)_ent->value.d_int;

		throwDictValueException(_ent, "FLOATING");
	}
	return _ent->value.d_floating;
}

decimal64_t VDict::Node::decimal64Value() const
{
	if (!_ent || _ent->value.kind != VBS_DECIMAL)
	{
		if (_ent && _ent->value.kind == VBS_INTEGER)
		{
			decimal64_t d;
			if (decimal64_from_integer(&d, _ent->value.d_int) == 0)
				return d;
		}

		throwDictValueException(_ent, "DECIMAL");
	}
	return _ent->value.d_decimal64;
}

VList VDict::Node::vlistValue() const
{
	CHECK_VALUE(_ent, LIST);
	return VList(_ent->value.d_list);
}

VDict VDict::Node::vdictValue() const
{
	CHECK_VALUE(_ent, DICT);
	return VDict(_ent->value.d_dict);
}

const vbs_list_t *VDict::Node::listValue() const
{
	if (_ent)
	{
		if (_ent->value.kind == VBS_LIST)
			return _ent->value.d_list;
	}

	throwDictValueException(_ent, "LIST");
	return NULL;
}

const vbs_dict_t *VDict::Node::dictValue() const
{
	if (_ent)
	{
		if (_ent->value.kind == VBS_DICT)
			return _ent->value.d_dict;
	}

	throwDictValueException(_ent, "DICT");
	return NULL;
}

const vbs_data_t *VDict::Node::dataValue() const
{
	if (!_ent)
		throwDictValueException(_ent, "");
	return &_ent->value;
}

VDict::Node VDict::getNode(const char *key) const
{
	vbs_ditem_t *ent;
	for (ent = _dict->first; ent; ent = ent->next)
	{
		if (ent->key.kind != VBS_STRING)
			continue;

		if (xstr_equal_cstr(&ent->key.d_xstr, key))
			return Node(ent);
	}
	return Node(NULL);
}

bool VDict::getNode(const char *key, VDict::Node& node) const
{
	node = getNode(key);
	return node;
}

intmax_t VDict::getInt(const char *key, intmax_t dft) const
{
	vbs_data_t *v = _find_key(_dict, key);
	if (v && v->kind == VBS_INTEGER)
		return v->d_int;
	return dft;
}

const xstr_t& VDict::getXstr(const char *key, const xstr_t& dft) const
{
	vbs_data_t *v = _find_key(_dict, key);
	if (v && v->kind == VBS_STRING)
		return v->d_xstr;
	return dft;
}

const xstr_t& VDict::getBlob(const char *key, const xstr_t& dft) const
{
	vbs_data_t *v = _find_key(_dict, key);
	if (v && (v->kind == VBS_BLOB || v->kind == VBS_STRING))
		return v->d_blob;
	return dft;
}

bool VDict::getBool(const char *key, bool dft) const
{
	vbs_data_t *v = _find_key(_dict, key);
	if (v && v->kind == VBS_BOOL)
		return v->d_bool;
	return dft;
}

double VDict::getFloating(const char *key, double dft) const
{
	vbs_data_t *v = _find_key(_dict, key);
	if (v && v->kind == VBS_FLOATING)
		return v->d_floating;
	else if (v && v->kind == VBS_INTEGER)
		return (double)v->d_int;
	return dft;
}

decimal64_t VDict::getDecimal64(const char *key, decimal64_t dft) const
{
	vbs_data_t *v = _find_key(_dict, key);
	if (v && v->kind == VBS_DECIMAL)
		return v->d_decimal64;
	else if (v && v->kind == VBS_INTEGER)
	{
		decimal64_t d;
		if (decimal64_from_integer(&d, v->d_int) == 0)
			return d;
	}
	return dft;
}

const vbs_data_t *VDict::get_data(const char *key) const
{
	return _find_key(_dict, key);
}

const vbs_data_t *VDict::get_data(intmax_t key) const
{
	return _find_ikey(_dict, key);
}

const vbs_list_t *VDict::get_list(const char *key) const
{
	vbs_data_t *v = _find_key(_dict, key);
	if (v && v->kind == VBS_LIST)
		return v->d_list;
	return NULL;
}

const vbs_dict_t *VDict::get_dict(const char *key) const
{
	vbs_data_t *v = _find_key(_dict, key);
	if (v && v->kind == VBS_DICT)
		return v->d_dict;
	return NULL;
}

void VDict::getXstrSeq(const char *key, std::vector<xstr_t>& result) const
{
	const vbs_list_t *ls = get_list(key);
	if (ls)
	{
		result.reserve(ls->count);
		vbs_litem_t *ent;
		for (ent = ls->first; ent; ent = ent->next)
		{
			if (ent->value.kind == VBS_STRING)
			{
				result.push_back(ent->value.d_xstr);
			}
		}
	}
}

void VDict::getBlobSeq(const char *key, std::vector<xstr_t>& result) const
{
	const vbs_list_t *ls = get_list(key);
	if (ls)
	{
		result.reserve(ls->count);
		vbs_litem_t *ent;
		for (ent = ls->first; ent; ent = ent->next)
		{
			if (ent->value.kind == VBS_BLOB || ent->value.kind == VBS_STRING)
			{
				result.push_back(ent->value.d_xstr);
			}
		}
	}
}

static void throwTypeException(const char *key, vbs_data_t *d, const char *needType)
{
	if (d)
	{
		throw XERROR_FMT(xic::ParameterTypeException,
			"Kind of item value for key '%s' in the dict should be %s instead of %s",
			key, needType, vbs_kind_name(d->kind));
	}
	else if (needType[0])
	{
		throw XERROR_FMT(xic::ParameterMissingException,
			"No item key '%s' found in the dict, the kind of item value should be %s",
			key, needType);
	}
	else
	{
		throw XERROR_FMT(xic::ParameterMissingException,
			"No item key '%s' found in the dict",
			key);
	}
}

VDict::Node VDict::wantNode(const char *key) const
{
	vbs_ditem_t *ent;
	for (ent = _dict->first; ent; ent = ent->next)
	{
		if (ent->key.kind != VBS_STRING)
			continue;

		if (xstr_equal_cstr(&ent->key.d_xstr, key))
			return Node(ent);
	}
	throwTypeException(key, NULL, "");
	return Node(NULL);
}

intmax_t VDict::wantInt(const char *key) const
{
	vbs_data_t *v = _find_key(_dict, key);
	if (!v || v->kind != VBS_INTEGER)
		throwTypeException(key, v, "INTEGER");
	return v->d_int;
}

const xstr_t& VDict::wantXstr(const char *key) const
{
	vbs_data_t *v = _find_key(_dict, key);
	if (!v || v->kind != VBS_STRING)
		throwTypeException(key, v, "STRING");
	return v->d_xstr;
}

const xstr_t& VDict::wantBlob(const char *key) const
{
	vbs_data_t *v = _find_key(_dict, key);
	if (!v || (v->kind != VBS_BLOB && v->kind != VBS_STRING))
		throwTypeException(key, v, "BLOB");
	return v->d_blob;
}

bool VDict::wantBool(const char *key) const
{
	vbs_data_t *v = _find_key(_dict, key);
	if (!v || v->kind != VBS_BOOL)
		throwTypeException(key, v, "BOOL");
	return v->d_bool;
}

double VDict::wantFloating(const char *key) const
{
	vbs_data_t *v = _find_key(_dict, key);
	if (!v || v->kind != VBS_FLOATING)
	{
		if (v && v->kind == VBS_INTEGER)
			return (double)v->d_int;

		throwTypeException(key, v, "FLOATING");
	}
	return v->d_floating;
}

decimal64_t VDict::wantDecimal64(const char *key) const
{
	vbs_data_t *v = _find_key(_dict, key);
	if (!v || v->kind != VBS_DECIMAL)
	{
		if (v && v->kind == VBS_INTEGER)
		{
			decimal64_t d;
			if (decimal64_from_integer(&d, v->d_int) == 0)
				return d;
		}

		throwTypeException(key, v, "DECIMAL");
	}
	return v->d_decimal64;
}

const vbs_data_t *VDict::want_data(const char *key) const
{
	vbs_data_t *v = _find_key(_dict, key);
	if (!v)
		throwTypeException(key, v, "");
	return v;
}

const vbs_data_t *VDict::want_data(intmax_t key) const
{
	vbs_data_t *v = _find_ikey(_dict, key);
	if (!v)
	{
		throw XERROR_FMT(xic::ParameterMissingException,
			"No item key [%jd] found in the dict",
			key);
	}
	return v;
}

const vbs_list_t *VDict::want_list(const char *key) const
{
	vbs_data_t *v = _find_key(_dict, key);
	if (!v || v->kind != VBS_LIST)
		throwTypeException(key, v, "LIST");
	return v->d_list;
}

const vbs_dict_t *VDict::want_dict(const char *key) const
{
	vbs_data_t *v = _find_key(_dict, key);
	if (!v || v->kind != VBS_DICT)
		throwTypeException(key, v, "DICT");
	return v->d_dict;
}

static void throwListItemException(const char *key, vbs_data_t *d, const char *needType)
{
	throw XERROR_FMT(xic::ParameterTypeException,
		"Kind of element in the list of item value (for key '%s') in the dict should be %s instead of %s",
		key, needType, vbs_kind_name(d->kind));
}

void VDict::wantXstrSeq(const char *key, std::vector<xstr_t>& result) const
{
	vbs_data_t *v = _find_key(_dict, key);
	if (!v || v->kind != VBS_LIST)
		throwTypeException(key, v, "LIST");

	vbs_list_t *ls = v->d_list;
	result.reserve(ls->count);
	for (vbs_litem_t *ent = ls->first; ent; ent = ent->next)
	{
		if (ent->value.kind != VBS_STRING)
			throwListItemException(key, &ent->value, "STRING");

		result.push_back(ent->value.d_xstr);
	}
}

void VDict::wantBlobSeq(const char *key, std::vector<xstr_t>& result) const
{
	vbs_data_t *v = _find_key(_dict, key);
	if (!v || v->kind != VBS_LIST)
		throwTypeException(key, v, "LIST");

	vbs_list_t *ls = v->d_list;
	result.reserve(ls->count);
	for (vbs_litem_t *ent = ls->first; ent; ent = ent->next)
	{
		if (ent->value.kind != VBS_BLOB && ent->value.kind != VBS_STRING)
			throwListItemException(key, &ent->value, "BLOB");

		result.push_back(ent->value.d_blob);
	}
}

VList VDict::wantVList(const char *key) const
{
	return VList(want_list(key));
}

VDict VDict::wantVDict(const char *key) const
{
	return VDict(want_dict(key));
}

} // namespace xic
