#include "VWriter.h"
#include "XicException.h"
#include "xslib/xformat.h"
#include "xslib/escape.h"
#include <stdio.h>
#include <stdlib.h>

namespace xic
{

void VComWriter::Entity::writeString(const rope_t *rope)
{
	if (vbs_pack_head_of_string(_pk, rope->length) < 0)
		return;

	if (vbs_pack_raw_rope(_pk, rope) < 0)
		return;
}

void VComWriter::Entity::writeString(const struct iovec *iov, int count)
{
	size_t size = 0;

	for (int i = 0; i < count; ++i)
		size += iov[i].iov_len;

	if (vbs_pack_head_of_string(_pk, size) < 0)
		return;

	for (int i = 0; i < count; ++i)
	{
		void *buf = iov[i].iov_base;
		int len = iov[i].iov_len;
		if (_pk->write(_pk->cookie, buf, len) != (ssize_t)len)
		{
			_pk->error = -1;
			return;
		}
	}
}

void VComWriter::Entity::writeBlob(const rope_t *rope)
{
	if (vbs_pack_head_of_blob(_pk, rope->length) < 0)
		return;

	if (vbs_pack_raw_rope(_pk, rope) < 0)
		return;
}

void VComWriter::Entity::writeBlob(const struct iovec *iov, int count)
{
	size_t size = 0;

	for (int i = 0; i < count; ++i)
		size += iov[i].iov_len;

	if (vbs_pack_head_of_blob(_pk, size) < 0)
		return;

	for (int i = 0; i < count; ++i)
	{
		void *buf = iov[i].iov_base;
		int len = iov[i].iov_len;
		if (_pk->write(_pk->cookie, buf, len) != (ssize_t)len)
		{
			_pk->error = -1;
			return;
		}
	}
}

VComWriter::Entity *VComWriter::Entity::create(vbs_packer_t *pk)
{
	VComWriter::Entity *ent = (VComWriter::Entity *)calloc(1, sizeof(*ent));
	if (ent)
	{
		OREF_INIT(ent);
		ent->_pk = pk;
	}
	return ent;
}

void VComWriter::Entity::destroy(VComWriter::Entity *ent)
{
	if (!ent->_closed)
		ent->close();
	free(ent);
}

void VComWriter::Entity::finish_child()
{
	if (child)
	{
		child->close();
		OREF_DEC(child, Entity::destroy);
		child = NULL;
	}
}

void VComWriter::Entity::close()
{
	if (!_closed)
	{
		_closed = true;
		if (child)
		{
			child->close();
			OREF_DEC(child, Entity::destroy);
			child = NULL;
		}
		vbs_pack_tail(_pk);
		_pk = NULL;
	}
}


VComWriter::VComWriter()
{
	_entity = NULL;
}

VComWriter::VComWriter(vbs_packer_t* pk)
{
	if (!pk)
		throw XERROR_MSG(XArgumentError, "VComWriter() vbs_packer_t is given to NULL");

	_entity = Entity::create(pk);
}

VComWriter::VComWriter(const VComWriter& r)
	: _entity(r._entity)
{
	if (_entity)
	{
		OREF_INC(_entity);
	}
} 

VComWriter& VComWriter::operator=(const VComWriter& r)
{
	if (_entity != r._entity)
	{
		if (r._entity) OREF_INC(r._entity);
		if (_entity) OREF_DEC(_entity, Entity::destroy);
		_entity = r._entity;
	}
	return *this;
}

VComWriter::~VComWriter()
{
	if (_entity)
	{
		OREF_DEC(_entity, Entity::destroy);
	}
}

void VComWriter::_close()
{
	if (_entity)
	{
		int err = _entity->_pk->error;
		_entity->close();
		OREF_DEC(_entity, Entity::destroy);
		_entity = NULL;
		if (err)
			throw XERROR_FMT(xic::VWriterException, "VComWriter::_close, err=%d", err);
	}
}

VListWriter::VListWriter(Entity *entity, int kind)
	: VComWriter(entity->_pk)
{
	entity->child = _entity;
	OREF_INC(_entity);
	vbs_pack_head_of_list(_entity->_pk, kind);
}

VListWriter::VListWriter(vbs_packer_t* pk, int kind)
	: VComWriter(pk)
{
	vbs_pack_head_of_list(pk, kind);
}

void VListWriter::setPacker(vbs_packer_t* pk, int kind)
{
	if (_entity)
		_close();
	_entity = Entity::create(pk);
	vbs_pack_head_of_list(pk, kind);
}

VDictWriter::VDictWriter(Entity *entity, int kind)
	: VComWriter(entity->_pk)
{
	entity->child = _entity;
	OREF_INC(_entity);
	vbs_pack_head_of_dict(entity->_pk, kind);
}

VDictWriter::VDictWriter(vbs_packer_t* pk, int kind)
	: VComWriter(pk)
{
	vbs_pack_head_of_dict(pk, kind);
}

void VDictWriter::setPacker(vbs_packer_t* pk, int kind)
{
	if (_entity)
		_close();
	_entity = Entity::create(pk);
	vbs_pack_head_of_dict(pk, kind);
}

} // namespace xic

