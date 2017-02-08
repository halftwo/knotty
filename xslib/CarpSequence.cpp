#include "xsdef.h"
#include "CarpSequence.h"
#include "bit.h"

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: CarpSequence.cpp,v 1.3 2013/03/21 03:10:26 gremlin Exp $";
#endif

CarpSequence::CarpSequence(const uint64_t *members, size_t num, uint32_t keymask)
{
	_cache.ptr = NULL;
	_mask = keymask ? round_up_power_two(keymask) - 1 : UINT32_MAX;
	_carp = carp_create(members, num, NULL);
}

CarpSequence::CarpSequence(const uint64_t *members, const uint32_t *weights/*NULL*/, size_t num, uint32_t keymask)
{
	_cache.ptr = NULL;
	_mask = keymask ? round_up_power_two(keymask) - 1 : UINT32_MAX;
	_carp = carp_create_with_weight(members, weights, num, NULL);
}

CarpSequence::~CarpSequence()
{
	carp_destroy(_carp);
	if (_cache.ptr)
		free(_cache.ptr);
}

bool CarpSequence::enable_cache()
{
	if (!_cache.ptr && _mask <= INT32_MAX)
	{
		size_t size = carp_total(_carp);
		if (size < UINT8_MAX)
		{
			_cache_u8 = true;
			_cache.ptr = calloc((_mask + 1), sizeof(uint8_t));
		}
		else if (size < UINT16_MAX)
		{
			_cache_u8 = false;
			_cache.ptr = calloc((_mask + 1), sizeof(uint16_t));
		}
	}

	return bool(_cache.ptr);
}

int CarpSequence::which(uint32_t keyhash)
{
	uint32_t h = keyhash & _mask;
	if (_cache.ptr)
	{
		int x = _cache_u8 ? _cache.u8[h] : _cache.u16[h];
		if (!x)
		{
			x = carp_which(_carp, h) + 1;
			if (_cache_u8)
				_cache.u8[h] = x;
			else
				_cache.u16[h] = x;
		}
		return x - 1;
	}

	return carp_which(_carp, h);
}

size_t CarpSequence::sequence(uint32_t keyhash, int seqs[], size_t num)
{
	return carp_sequence(_carp, keyhash & _mask, seqs, num);
}

