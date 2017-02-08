/* $Id: CarpSequence.h,v 1.3 2013/03/21 03:10:26 gremlin Exp $ */
#ifndef CarpSequence_h_
#define CarpSequence_h_

#include "carp.h"
#include <stdint.h>

class CarpSequence
{
public:
	CarpSequence(const uint64_t *members, size_t num, uint32_t keymask);
	CarpSequence(const uint64_t *members, const uint32_t *weights/*NULL*/, size_t num, uint32_t keymask);
	~CarpSequence();

	bool enable_cache();

	size_t size() const 			{ return carp_total(_carp); }
	uint32_t mask() const			{ return _mask; }

	int which(uint32_t key);
	size_t sequence(uint32_t key, int seqs[], size_t num);

private:
	carp_t *_carp;
	uint32_t _mask;
	bool _cache_u8;
	union
	{
		void *ptr;
		uint8_t *u8;
		uint16_t *u16;
	} _cache;
};

#endif
