#ifndef VData_h_
#define VData_h_

#include "XicException.h"
#include "xslib/vbs.h"
#include <stdint.h>
#include <vector>
#include <set>

namespace xic
{

#define	VDATA_NOT_TYPE(V, TYPE) 	((V)->type != VBS_##TYPE)

class VList;
class VDict;

void _vdata_throw_TypeException(const vbs_data_t* v, const char *type);
void _vdata_throw_DataException();


class VList
{
	const vbs_list_t* _list;
public:
	VList(const vbs_list_t *ls);

	class Node
	{
		friend class VList;
		vbs_litem_t *_ent;
		Node(vbs_litem_t *ent): _ent(ent) {}
	public:
		Node()				: _ent(0) {}
		typedef vbs_litem_t* Node::*my_bool;

		vbs_litem_t *item() const	{ return _ent; }
		operator my_bool() const	{ return _ent ? &Node::_ent : 0; }
		Node next() const		{ return Node(_ent ? _ent->next : 0); }
		Node& operator++()		{ if (_ent) _ent = _ent->next; return *this; }
		Node operator++(int)		{ Node tmp = *this; if (_ent) _ent = _ent->next; return tmp; }

		vbs_type_t valueType() const	{ return _ent->value.type; }

		bool isNullValue() const 	{ return _ent->value.type == VBS_NULL; }
		void expectNullValue() const;

		intmax_t intValue() const;
		const xstr_t& xstrValue() const;
		const xstr_t& blobValue() const;	// VBS_BLOB or VBS_STRING
		bool boolValue() const;
		double floatingValue() const;
		decimal64_t decimal64Value() const;
		VList vlistValue() const;
		VDict vdictValue() const;
		const vbs_list_t *listValue() const;
		const vbs_dict_t *dictValue() const;
		const vbs_data_t *dataValue() const;
	};

	Node first() const			{ return VList::Node(_list->first); }
	size_t count() const 			{ return _list->count; }
	size_t size() const 			{ return _list->count; }
	const vbs_list_t *list() const 		{ return _list; }
};


class VDict
{
	const vbs_dict_t* _dict;
public:
	VDict(const vbs_dict_t* dict);

	class Node
	{
		friend class VDict;
		vbs_ditem_t *_ent;
		Node(vbs_ditem_t *ent): _ent(ent) {}
	public:
		Node()				: _ent(0) {}
		typedef vbs_ditem_t* Node::*my_bool;

		vbs_ditem_t *item() const	{ return _ent; }
		operator my_bool() const	{ return _ent ? &Node::_ent : 0; }
		Node next() const		{ return Node(_ent ? _ent->next : 0); }
		Node& operator++()		{ if (_ent) _ent = _ent->next; return *this; }
		Node operator++(int)		{ Node tmp = *this; if (_ent) _ent = _ent->next; return tmp; }

		vbs_type_t keyType() const	{ return _ent->key.type; }
		vbs_type_t valueType() const	{ return _ent->value.type; }

		intmax_t intKey() const;
		const xstr_t& xstrKey() const;
		const vbs_data_t *dataKey() const;

		bool isNullValue() const 	{ return _ent->value.type == VBS_NULL; }
		void expectNullValue() const;

		intmax_t intValue() const;
		const xstr_t& xstrValue() const;
		const xstr_t& blobValue() const;	// VBS_BLOB or VBS_STRING
		bool boolValue() const;
		double floatingValue() const;
		decimal64_t decimal64Value() const;
		VList vlistValue() const;
		VDict vdictValue() const;
		const vbs_list_t *listValue() const;
		const vbs_dict_t *dictValue() const;
		const vbs_data_t *dataValue() const;
	};

	Node first() const			{ return VDict::Node(_dict->first); }
	size_t count() const 			{ return _dict->count; }
	size_t size() const 			{ return _dict->count; }
	const vbs_dict_t *dict() const 		{ return _dict; }

	Node getNode(const char *key) const;
	bool getNode(const char *key, Node& node) const;

	intmax_t getInt(const char *key, intmax_t dft) const;
	const xstr_t& getXstr(const char *key, const xstr_t& dft) const;
	const xstr_t& getBlob(const char *key, const xstr_t& dft) const;	// VBS_BLOB or VBS_STRING
	bool getBool(const char *key, bool dft) const;
	double getFloating(const char *key, double dft) const;
	decimal64_t getDecimal64(const char *key, decimal64_t dft) const;

	intmax_t getInt(const char *key) const  	{ return getInt(key, 0); }
	const xstr_t& getXstr(const char *key) const  	{ return getXstr(key, xstr_null); }
	const xstr_t& getBlob(const char *key) const  	{ return getBlob(key, xstr_null); }
	bool getBool(const char *key) const 		{ return getBool(key, false); }
	double getFloating(const char *key) const 	{ return getFloating(key, 0.0); }
	decimal64_t getDecimal64(const char *key) const { return getDecimal64(key, decimal64_zero); }

	const vbs_list_t *get_list(const char *key) const;
	const vbs_dict_t *get_dict(const char *key) const;

	const vbs_data_t *get_data(const char *key) const;
	const vbs_data_t *get_data(intmax_t key) const;

	void getXstrSeq(const char *key, std::vector<xstr_t>& value) const;
	void getBlobSeq(const char *key, std::vector<xstr_t>& value) const;

	template<typename INTEGER>
	void getIntSeq(const char *key, std::vector<INTEGER>& value) const;

	template<typename INTEGER>
	void getIntSeq(const char *key, std::set<INTEGER>& value) const;


	// The following function will throw exceptions
	Node wantNode(const char *key) const;

	intmax_t wantInt(const char *key) const;
	const xstr_t& wantXstr(const char *key) const;
	const xstr_t& wantBlob(const char *key) const;
	bool wantBool(const char *key) const;
	double wantFloating(const char *key) const;
	decimal64_t wantDecimal64(const char *key) const;

	const vbs_list_t *want_list(const char *key) const;
	const vbs_dict_t *want_dict(const char *key) const;

	const vbs_data_t *want_data(const char *key) const;
	const vbs_data_t *want_data(intmax_t key) const;

	void wantXstrSeq(const char *key, std::vector<xstr_t>& value) const;
	void wantBlobSeq(const char *key, std::vector<xstr_t>& value) const;

	template<typename INTEGER>
	void wantIntSeq(const char *key, std::vector<INTEGER>& value) const;

	template<typename INTEGER>
	void wantIntSeq(const char *key, std::set<INTEGER>& value) const;

	VList wantVList(const char *key) const;
	VDict wantVDict(const char *key) const;
};


inline bool isNull(const vbs_data_t* v)
{
	return v->type == VBS_NULL;
}

inline bool isNull(const vbs_data_t& v)
{
	return isNull(&v);
}

inline void expectNull(const vbs_data_t* v)
{
	if (VDATA_NOT_TYPE(v, NULL))
		_vdata_throw_TypeException(v, "NULL");
}

inline void expectNull(const vbs_data_t& v)
{
	return expectNull(&v);
}

inline intmax_t intValue(const vbs_data_t* v)
{
	if (VDATA_NOT_TYPE(v, INTEGER))
		_vdata_throw_TypeException(v, "INT");
	return v->d_int;
}

inline intmax_t intValue(const vbs_data_t& v)
{
	return intValue(&v);
}

inline const xstr_t& xstrValue(const vbs_data_t* v)
{
	if (VDATA_NOT_TYPE(v, STRING))
		_vdata_throw_TypeException(v, "STRING");
	return v->d_xstr;
}

inline const xstr_t& xstrValue(const vbs_data_t& v)
{
	return xstrValue(&v);
}


inline const xstr_t& blobValue(const vbs_data_t* v)
{
	if (VDATA_NOT_TYPE(v, BLOB))
	{
		if (v->type == VBS_STRING)
			return v->d_xstr;

		_vdata_throw_TypeException(v, "BLOB");
	}
	return v->d_blob;
}

inline const xstr_t& blobValue(const vbs_data_t& v)
{
	return blobValue(&v);
}


inline bool boolValue(const vbs_data_t* v)
{
	if (VDATA_NOT_TYPE(v, BOOL))
		_vdata_throw_TypeException(v, "BOOL");
	return v->d_bool;
}

inline bool boolValue(const vbs_data_t& v)
{
	return boolValue(&v);
}


inline double floatingValue(const vbs_data_t* v)
{
	if (VDATA_NOT_TYPE(v, FLOATING))
	{
		if (v->type == VBS_INTEGER)
			return (double)v->d_int;

		_vdata_throw_TypeException(v, "FLOATING");
	}
	return v->d_floating;
}

inline double floatingValue(const vbs_data_t& v)
{
	return floatingValue(&v);
}

inline decimal64_t decimal64Value(const vbs_data_t* v)
{
	if (VDATA_NOT_TYPE(v, DECIMAL))
	{
		if (v->type == VBS_INTEGER)
		{
			decimal64_t d;
			if (decimal64_from_integer(&d, v->d_int) == 0)
				return d;
		}

		_vdata_throw_TypeException(v, "DECIMAL");
	}
	return v->d_decimal64;
}

inline decimal64_t decimal64Value(const vbs_data_t& v)
{
	return decimal64Value(&v);
}

inline const vbs_list_t* listValue(const vbs_data_t* v) 
{
	if (VDATA_NOT_TYPE(v, LIST))
		_vdata_throw_TypeException(v, "LIST");
	return v->d_list;
}

inline const vbs_list_t* listValue(const vbs_data_t& v) 
{
	return listValue(&v);
}


inline const vbs_dict_t* dictValue(const vbs_data_t* v)
{
	if (VDATA_NOT_TYPE(v, LIST))
		_vdata_throw_TypeException(v, "DICT");
	return v->d_dict;
}

inline const vbs_dict_t* dictValue(const vbs_data_t& v)
{
	return dictValue(&v);
}


inline VList vlistValue(const vbs_data_t* v) 
{
	return VList(listValue(v));
}

inline VList vlistValue(const vbs_data_t& v) 
{
	return VList(listValue(v));
}


inline VDict vdictValue(const vbs_data_t* v)
{
	return VDict(dictValue(v));
}

inline VDict vdictValue(const vbs_data_t& v)
{
	return VDict(dictValue(v));
}


template<typename INTEGER>
void VDict::getIntSeq(const char *key, std::vector<INTEGER>& v) const
{
	const vbs_list_t *ls = get_list(key);
	if (ls)
	{
		v.reserve(ls->count);
		vbs_litem_t *ent;
		for (ent = ls->first; ent; ent = ent->next)
		{
			if (ent->value.type == VBS_INTEGER)
			{
				v.push_back(ent->value.d_int);
			}
		}
	}
}

template<typename INTEGER>
void VDict::getIntSeq(const char *key, std::set<INTEGER>& v) const
{
	const vbs_list_t *ls = get_list(key);
	if (ls)
	{
		vbs_litem_t *ent;
		for (ent = ls->first; ent; ent = ent->next)
		{
			if (ent->value.type == VBS_INTEGER)
			{
				v.insert(ent->value.d_int);
			}
		}
	}
}

template<typename INTEGER>
void VDict::wantIntSeq(const char *key, std::vector<INTEGER>& v) const
{
	const vbs_list_t *ls = want_list(key);
	if (ls)
	{
		v.reserve(ls->count);
		vbs_litem_t *ent;
		for (ent = ls->first; ent; ent = ent->next)
		{
			if (ent->value.type == VBS_INTEGER)
			{
				v.push_back(ent->value.d_int);
			}
			else
			{
				throw XERROR_FMT(xic::ParameterTypeException,
					"Type of list element of VDict value (for key '%s') should be %s instead of %s",
					key, "INTEGER", vbs_type_name(ent->value.type));
			}
		}
	}
}

template<typename INTEGER>
void VDict::wantIntSeq(const char *key, std::set<INTEGER>& v) const
{
	const vbs_list_t *ls = want_list(key);
	if (ls)
	{
		vbs_litem_t *ent;
		for (ent = ls->first; ent; ent = ent->next)
		{
			if (ent->value.type == VBS_INTEGER)
			{
				v.insert(ent->value.d_int);
			}
			else
			{
				throw XERROR_FMT(xic::ParameterTypeException,
					"Type of list element of VDict value (for key '%s') should be %s instead of %s",
					key, "INTEGER", vbs_type_name(ent->value.type));
			}
		}
	}
}


} // namespace xic

#endif
