#ifndef VWriter_h_
#define VWriter_h_

#include "VData.h"
#include "xslib/oref.h"
#include "xslib/XRefCount.h"
#include "xslib/vbs_pack.h"
#include "xslib/XError.h"
#include "xslib/xformat.h"
#include <stdarg.h>
#include <vector>
#include <map>


namespace xic
{
class VDictWriter;
class VListWriter;
}

/*
   Specialize the follwing template function
	vlist_write_one_item()
	vdict_write_one_item()
   to make your special data types work with VDictWriter or VListWriter.
 */

template <typename ValueType>
void vlist_write_one_item(xic::VListWriter& lw, const ValueType& v);

template <typename KeyType, typename ValueType>
void vdict_write_one_item(xic::VDictWriter& dw, const KeyType& k, const ValueType& v);


template <typename ListType>
void vlist_write_all(xic::VListWriter& lw, const ListType& values);

template <typename DictType>
void vdict_write_all(xic::VDictWriter& dw, const DictType& dict);



namespace xic
{

class VComWriter
{
public:
	/* Must be called before taking actions on vbs-encoded data.
	 */
	void close() 				{ if (_entity) _close(); }

protected:
	struct Entity 
	{
		OREF_DECLARE();
		int _closed;
		vbs_packer_t *_pk;
		Entity *child;

	public:
		static Entity *create(vbs_packer_t *pk);
		static void destroy(Entity *ent);

		void close();
		void finish_child();

		void writeDescriptor(int descriptor) { vbs_pack_descriptor(_pk, descriptor); }

		void raw(const void *buf, size_t size)	{ vbs_pack_raw(_pk, buf, size); }

		void write(short v) 			{ vbs_pack_integer(_pk, v); }
		void write(int v) 			{ vbs_pack_integer(_pk, v); }
		void write(long v) 			{ vbs_pack_integer(_pk, v); }
		void write(long long v) 		{ vbs_pack_integer(_pk, v); }

		void write(unsigned short v) 		{ vbs_pack_uinteger(_pk, v); }
		void write(unsigned int v) 		{ vbs_pack_uinteger(_pk, v); }
		void write(unsigned long v) 		{ vbs_pack_uinteger(_pk, v); }
		void write(unsigned long long v) 	{ vbs_pack_uinteger(_pk, v); }

		void write(bool v) 			{ vbs_pack_bool(_pk, v); }
		void write(float v) 			{ vbs_pack_floating(_pk, v); }
		void write(double v) 			{ vbs_pack_floating(_pk, v); }
		void write(decimal64_t v) 		{ vbs_pack_decimal64(_pk, v); }

		void write(const std::string &str) 	{ vbs_pack_lstr(_pk, str.c_str(), str.length()); }
		void write(const xstr_t *xstr) 		{ vbs_pack_xstr(_pk, xstr); }
		void write(const xstr_t &xstr) 		{ vbs_pack_xstr(_pk, &xstr); }
		void write(const char *str) 		{ vbs_pack_cstr(_pk, str); }
		void write(char *str) 			{ vbs_pack_cstr(_pk, str); }
		void write(const vbs_list_t *vl)	{ vbs_pack_list(_pk, vl); }
		void write(const vbs_list_t &vl)	{ vbs_pack_list(_pk, &vl); }
		void write(const vbs_dict_t *vd)	{ vbs_pack_dict(_pk, vd); }
		void write(const vbs_dict_t &vd)	{ vbs_pack_dict(_pk, &vd); }
		void write(const vbs_data_t *data)	{ vbs_pack_data(_pk, data); }
		void write(const vbs_data_t &data)	{ vbs_pack_data(_pk, &data); }
		void write(const VList &vl)		{ vbs_pack_list(_pk, vl.list()); }
		void write(const VDict &vd)		{ vbs_pack_dict(_pk, vd.dict()); }

		void writeFormat(xfmt_callback_function cb/*NULL*/, const char *fmt, va_list ap)
		{
			vbs_pack_string_vprintf(_pk, cb, fmt, ap);
		}

		void writeString(const char *str, size_t len) 	{ vbs_pack_lstr(_pk, str, len); }
		void writeString(const struct iovec *iov, int count);
		void writeString(const rope_t *rope);

		void writeBlob(const void *data, size_t len) 	{ vbs_pack_blob(_pk, data, len); }
		void writeBlob(const struct iovec *iov, int count);
		void writeBlob(const rope_t *rope);

		void writeStrHead(size_t len)			{ vbs_pack_head_of_string(_pk, len); }
		void writeBlobHead(size_t len)			{ vbs_pack_head_of_blob(_pk, len); }
		void writeRaw(const void *buf, size_t size) 	{ vbs_pack_raw(_pk, buf, size); }

		void writeNull()				{ vbs_pack_null(_pk); }
		void writeStanza(const unsigned char *buf, size_t size) 	{ vbs_pack_raw(_pk, buf, size); }

	private:
		void write(const void *dummy);		// These private functions should NOT be implemented
		void write(const XRefCountPtr& dummy);	// and called
	};

	VComWriter();
	VComWriter(vbs_packer_t* pk);
	VComWriter(const VComWriter& r);
	VComWriter& operator=(const VComWriter& r);
	~VComWriter();

	void _close();

	Entity *_entity;
};

class VListWriter: public VComWriter
{
	friend class VDictWriter;
	VListWriter(Entity *entity, int kind);
public:
	VListWriter() {}
	VListWriter(vbs_packer_t* w, int kind);

	void setPacker(vbs_packer_t* pk, int kind);

	void flush();

	template <typename ValueType>
	void v(const ValueType& v, int descriptor=0);

	void vfmt(xfmt_callback_function cb, const char *fmt, ...);
	void vfmt(xfmt_callback_function cb, const char *fmt, va_list ap);

	void vstring(const xstr_t& xstr, int descriptor=0);
	void vstring(const char *str, size_t len, int descriptor=0);
	void vstring(const rope_t *rope, int descriptor=0);
	void vstring(const struct iovec *iov, int count, int descriptor=0);

	void vblob(const xstr_t& blob, int descriptor=0);
	void vblob(const void *data, size_t len, int descriptor=0);
	void vblob(const rope_t *rope, int descriptor=0);
	void vblob(const struct iovec *iov, int count, int descriptor=0);

	void vstanza(const unsigned char *buf, size_t size, int descriptor=0);

	void vnull();

	VListWriter vlist(int kind, int descriptor=0);

	template <typename ListType>
	VListWriter vlist(const ListType& values, int kind, int descriptor=0);

	VDictWriter vdict(int kind, int descriptor=0);

	template <typename DictType>
	VDictWriter vdict(const DictType& dict, int kind, int descriptor=0);

	void vstrhead(size_t len, int descriptor=0);
	void vblobhead(size_t len, int descriptor=0);

	// Called after vstrhead() or vblobhead();
	void raw(const void *buf, size_t size);
};

class VDictWriter: public VComWriter
{
	friend class VListWriter;
	VDictWriter(Entity *entity, int kind);
public:
	VDictWriter() {}
	VDictWriter(vbs_packer_t* w, int kind);

	void setPacker(vbs_packer_t* pk, int kind);

	void flush();

	template <typename KeyType, typename ValueType>
	void kv(const KeyType& key, const ValueType& value, int descriptor=0);

	template <typename KeyType>
	void kvfmt(const KeyType& key, xfmt_callback_function cb, const char *fmt, ...);

	template <typename KeyType>
	void kvfmt(const KeyType& key, xfmt_callback_function cb, const char *fmt, va_list ap);

	template <typename KeyType>
	void kvstring(const KeyType& key, const xstr_t& xstr, int descriptor=0);

	template <typename KeyType>
	void kvstring(const KeyType& key, const char *str, size_t len, int descriptor=0);

	template <typename KeyType>
	void kvstring(const KeyType& key, const rope_t *rope, int descriptor=0);

	template <typename KeyType>
	void kvstring(const KeyType& key, const struct iovec *iov, int count, int descriptor=0);

	template <typename KeyType>
	void kvblob(const KeyType& key, const xstr_t& blob, int descriptor=0);

	template <typename KeyType>
	void kvblob(const KeyType& key, const void *data, size_t len, int descriptor=0);

	template <typename KeyType>
	void kvblob(const KeyType& key, const rope_t *rope, int descriptor=0);

	template <typename KeyType>
	void kvblob(const KeyType& key, const struct iovec *iov, int count, int descriptor=0);

	template <typename KeyType>
	void kvstanza(const KeyType& key, const unsigned char *buf, size_t size, int descriptor=0);

	template <typename KeyType>
	void kvnull(const KeyType& key);

	template <typename KeyType>
	VListWriter kvlist(const KeyType& key, int kind, int descriptor=0);

	template <typename KeyType, typename ListType>
	VListWriter kvlist(const KeyType& key, const ListType& values, int kind, int descriptor=0);

	template <typename KeyType>
	VDictWriter kvdict(const KeyType& key, int kind, int descriptor=0);

	template <typename KeyType, typename DictType>
	VDictWriter kvdict(const KeyType& key, const DictType& dict, int kind, int descriptor=0);

	template <typename KeyType>
	void kvstrhead(const KeyType& key, size_t len, int descriptor=0);

	template <typename KeyType>
	void kvblobhead(const KeyType& key, size_t len, int descriptor=0);

	// Called after kvstrhead() or kvblobhead();
	void raw(const void *buf, size_t size);
};



inline void VListWriter::flush()
{
	if (_entity->child)
		_entity->finish_child();
}

template <typename ValueType>
inline void VListWriter::v(const ValueType& v, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();

	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->write(v);
}

inline void VListWriter::vfmt(xfmt_callback_function cb, const char *fmt, ...)
{
	va_list ap;
	if (_entity->child)
		_entity->finish_child();
	va_start(ap, fmt);
	_entity->writeFormat(cb, fmt, ap);
	va_end(ap);
}

inline void VListWriter::vfmt(xfmt_callback_function cb, const char *fmt, va_list ap)
{
	if (_entity->child)
		_entity->finish_child();
	_entity->writeFormat(cb, fmt, ap);
}

inline void VListWriter::vstring(const xstr_t& xstr, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->write(xstr);
}

inline void VListWriter::vstring(const char *str, size_t len, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->writeString(str, len);
}

inline void VListWriter::vstring(const rope_t *rope, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->writeString(rope);
}

inline void VListWriter::vstring(const struct iovec *iov, int count, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->writeString(iov, count);
}

inline void VListWriter::vblob(const xstr_t& blob, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->writeBlob(blob.data, blob.len);
}

inline void VListWriter::vblob(const void *data, size_t len, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->writeBlob(data, len);
}

inline void VListWriter::vblob(const rope_t *rope, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->writeBlob(rope);
}

inline void VListWriter::vblob(const struct iovec *iov, int count, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->writeBlob(iov, count);
}

inline void VListWriter::vstanza(const unsigned char *buf, size_t size, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->writeStanza(buf, size);
}

inline void VListWriter::vnull()
{
	if (_entity->child)
		_entity->finish_child();
	return _entity->writeNull();
}

inline VListWriter VListWriter::vlist(int kind, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	return VListWriter(_entity, kind);
}

template <typename ListType>
inline VListWriter VListWriter::vlist(const ListType& values, int kind, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();

	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	VListWriter lw(_entity, kind);
	::vlist_write_all(lw, values);
	return lw;
}

inline VDictWriter VListWriter::vdict(int kind, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	return VDictWriter(_entity, kind);
}

template <typename DictType>
inline VDictWriter VListWriter::vdict(const DictType& dict, int kind, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();

	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	VDictWriter dw(_entity, kind);
	::vdict_write_all(dw, dict);
	return dw;
}


inline void VListWriter::vstrhead(size_t len, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->writeStrHead(len);
}

inline void VListWriter::vblobhead(size_t len, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->writeBlobHead(len);
}

inline void VListWriter::raw(const void *buf, size_t size)
{
	if (_entity->child)
		_entity->finish_child();
	_entity->raw(buf, size);
}



inline void VDictWriter::flush()
{
	if (_entity->child)
		_entity->finish_child();
}

template <typename KeyType, typename ValueType>
inline void VDictWriter::kv(const KeyType& key, const ValueType& value, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	_entity->write(key);
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->write(value);
}

template <typename KeyType>
inline void VDictWriter::kvfmt(const KeyType& key, xfmt_callback_function cb, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	kvfmt(key, cb, fmt, ap);
	va_end(ap);
}

template <typename KeyType>
inline void VDictWriter::kvfmt(const KeyType& key, xfmt_callback_function cb, const char *fmt, va_list ap)
{
	if (_entity->child)
		_entity->finish_child();
	_entity->write(key);
	_entity->writeFormat(cb, fmt, ap);
}

template <typename KeyType>
inline void VDictWriter::kvstring(const KeyType& key, const xstr_t& xstr, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	_entity->write(key);
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->write(xstr);
}

template <typename KeyType>
inline void VDictWriter::kvstring(const KeyType& key, const char *str, size_t len, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	_entity->write(key);
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->writeString(str, len);
}

template <typename KeyType>
inline void VDictWriter::kvstring(const KeyType& key, const rope_t *rope, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	_entity->write(key);
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->writeString(rope);
}

template <typename KeyType>
inline void VDictWriter::kvstring(const KeyType& key, const struct iovec *iov, int count, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	_entity->write(key);
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->writeString(iov, count);
}

template <typename KeyType>
inline void VDictWriter::kvblob(const KeyType& key, const xstr_t& blob, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	_entity->write(key);
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->writeBlob(blob.data, blob.len);
}

template <typename KeyType>
inline void VDictWriter::kvblob(const KeyType& key, const void *data, size_t len, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	_entity->write(key);
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->writeBlob(data, len);
}

template <typename KeyType>
inline void VDictWriter::kvblob(const KeyType& key, const rope_t *rope, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	_entity->write(key);
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->writeBlob(rope);
}

template <typename KeyType>
inline void VDictWriter::kvblob(const KeyType& key, const struct iovec *iov, int count, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	_entity->write(key);
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->writeBlob(iov, count);
}

template <typename KeyType>
inline void VDictWriter::kvstanza(const KeyType& key, const unsigned char *buf, size_t size, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	_entity->write(key);
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->writeStanza(buf, size);
}

template <typename KeyType>
inline void VDictWriter::kvnull(const KeyType& key)
{
	if (_entity->child)
		_entity->finish_child();
	_entity->write(key);
	_entity->writeNull();
}

template <typename KeyType>
inline VListWriter VDictWriter::kvlist(const KeyType& key, int kind, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	_entity->write(key);
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	return VListWriter(_entity, kind);
}

template <typename KeyType, typename ListType>
inline VListWriter VDictWriter::kvlist(const KeyType& key, const ListType& values, int kind, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	_entity->write(key);

	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	VListWriter lw(_entity, kind);
	::vlist_write_all(lw, values);
	return lw;
}


template <typename KeyType>
inline VDictWriter VDictWriter::kvdict(const KeyType& key, int kind, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	_entity->write(key);
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	return VDictWriter(_entity, kind);
}

template <typename KeyType, typename DictType>
inline VDictWriter VDictWriter::kvdict(const KeyType& key, const DictType& dict, int kind, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	_entity->write(key);
	
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	VDictWriter dw(_entity, kind);
	::vdict_write_all(dw, dict);
	return dw;
}

template <typename KeyType>
inline void VDictWriter::kvstrhead(const KeyType& key, size_t len, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	_entity->write(key);
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->writeStrHead(len);
}

template <typename KeyType>
inline void VDictWriter::kvblobhead(const KeyType& key, size_t len, int descriptor)
{
	if (_entity->child)
		_entity->finish_child();
	_entity->write(key);
	if (descriptor > 0)
		_entity->writeDescriptor(descriptor);
	_entity->writeBlobHead(len);
}

inline void VDictWriter::raw(const void *buf, size_t size)
{
	if (_entity->child)
		_entity->finish_child();
	_entity->raw(buf, size);
}

} // namespace xic


template <typename ValueType>
inline void vlist_write_one_item(xic::VListWriter& lw, const ValueType& v)
{
	lw.v(v);
}

template <typename ListType>
inline void vlist_write_all(xic::VListWriter& lw, const ListType& values)
{
	for (typename ListType::const_iterator iter = values.begin(); iter != values.end(); ++iter)
	{
		vlist_write_one_item(lw, *iter);
	}
}

template <>
inline void vlist_write_all(xic::VListWriter& lw, const vbs_list_t *const& vl)
{
	if (vl)
	{
		if (vl->_raw.data && vl->_raw.len)
		{
			const xstr_t& raw = vl->_raw;
			assert(raw.data[0] == VBS_LIST && raw.data[raw.len - 1] == VBS_TAIL);
			lw.raw(raw.data + 1, raw.len - 2);
		}
		else
		{
			for (const vbs_litem_t *ent = vl->first; ent; ent = ent->next)
			{
				lw.v(ent->value);
			}
		}
	}
}

template <>
inline void vlist_write_all(xic::VListWriter& lw, vbs_list_t *const& vl)
{
	vlist_write_all(lw, (const vbs_list_t*)vl);
}

template <>
inline void vlist_write_all(xic::VListWriter& lw, const xic::VList& vlist)
{
	vlist_write_all(lw, vlist.list());
}


template <typename KeyType, typename ValueType>
inline void vdict_write_one_item(xic::VDictWriter& dw, const KeyType& k, const ValueType& v)
{
	dw.kv(k, v);
}

template <typename DictType>
inline void vdict_write_all(xic::VDictWriter& dw, const DictType& dict)
{
	for (typename DictType::const_iterator iter = dict.begin(); iter != dict.end(); ++iter)
	{
		vdict_write_one_item(dw, iter->first, iter->second);
	}
}

template <>
inline void vdict_write_all(xic::VDictWriter& dw, const vbs_dict_t *const& vd)
{
	if (vd)
	{
		if (vd->_raw.data && vd->_raw.len)
		{
			const xstr_t& raw = vd->_raw;
			assert(raw.data[0] == VBS_DICT && raw.data[raw.len - 1] == VBS_TAIL);
			dw.raw(raw.data + 1, raw.len - 2);
		}
		else
		{
			for (const vbs_ditem_t *ent = vd->first; ent; ent = ent->next)
			{
				dw.kv(ent->key, ent->value);
			}
		}
	}
}

template <>
inline void vdict_write_all(xic::VDictWriter& dw, vbs_dict_t *const& vd)
{
	vdict_write_all(dw, (const vbs_dict_t*)vd);
}

template <>
inline void vdict_write_all(xic::VDictWriter& dw, const xic::VDict& vdict)
{
	vdict_write_all(dw, vdict.dict());
}


#endif
