/* $Id: vbs_pack.h,v 1.48 2015/06/11 02:38:58 gremlin Exp $ */
#ifndef VBS_PACK_H_
#define VBS_PACK_H_ 1

#include "xsdef.h"
#include "xstr.h"
#include "xmem.h"
#include "decimal64.h"
#include <stddef.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VBS_DESCRIPTOR_MAX	0x7fff
#define VBS_SPECIAL_DESCRIPTOR	0x8000

extern const xstr_t vbs_packed_empty_list;
extern const xstr_t vbs_packed_empty_dict;


typedef enum
{
	/* These are NOT real vbs types. */
	VBS_ERR_TOOBIG		= -3,	/* The size or length is too big. */
	VBS_ERR_INCOMPLETE	= -2,	/* No sufficient bytes to unpack. */
	VBS_ERR_INVALID		= -1,	/* Invalid sequence of bytes encountered when unpacking. */

	/* vbs types */
	VBS_TAIL	= 0x01,		/* Used to terminate list or dict. */

	VBS_LIST	= 0x02,

	VBS_DICT	= 0x03,

	/* RESERVED	  0x04 */
	/* RESERVED	  0x05 */
	/* RESERVED	  0x06 */

	/* DONT USE	  0x07 */
	/* DONT USE	  .... */
	/* DONT USE	  0x0D */

	/* RESERVED	  0x0E */

	VBS_NULL	= 0x0F,

	VBS_DESCRIPTOR	= 0x10,		/* 0001 0xxx */

	VBS_BOOL	= 0x18,		/* 0001 100x */ 	/* 0=F, 1=T */

	/* DONT USE	  0x1A */
	VBS_BLOB	= 0x1B,

	VBS_DECIMAL	= 0x1C,		/* 0001 110x */ 	/* 0=+ 1=- */

	VBS_FLOATING	= 0x1E,		/* 0001 111x */		/* 0=+ 1=- */

	VBS_STRING 	= 0x20,		/* 001x xxxx */

	VBS_INTEGER 	= 0x40,		/* 010x xxxx + */
					/* 011x xxxx - */
} vbs_type_t;



typedef struct vbs_data_t vbs_data_t;
typedef struct vbs_list_t vbs_list_t;
typedef struct vbs_dict_t vbs_dict_t;
typedef struct vbs_litem_t vbs_litem_t;
typedef struct vbs_ditem_t vbs_ditem_t;


struct vbs_list_t
{
	vbs_litem_t *first;
	vbs_litem_t **last;
	size_t count;
	int kind;
	xstr_t _raw;		/* Including the VBS_LIST and VBS_TAIL bytes */
};


struct vbs_dict_t
{
	vbs_ditem_t *first;
	vbs_ditem_t **last;
	size_t count;
	int kind;
	xstr_t _raw; 		/* Including the VBS_DICT and VBS_TAIL bytes */
};


struct vbs_data_t
{
	vbs_type_t 		type;

	unsigned int		is_owner:1;	/* Only applicable to STRING or BLOB */
	/* The d_xstr.data or d_blob.data memory is owned by myself.
	   If it is allocated, don't forget to free it when appropriate.
 	 */

	uint16_t		descriptor;

	union
	{
		intmax_t 	d_int;
		xstr_t 		d_xstr;
		xstr_t		d_blob;
		bool 		d_bool;
		double 		d_floating;
		decimal64_t	d_decimal64;
		vbs_list_t 	*d_list;
		vbs_dict_t 	*d_dict;
	};
};


struct vbs_litem_t
{
	vbs_litem_t *next;
	vbs_data_t value;
};


struct vbs_ditem_t
{
	vbs_ditem_t *next;
	vbs_data_t key;
	vbs_data_t value;
};


const char *vbs_type_name(vbs_type_t type);


static inline void vbs_data_init(vbs_data_t *dat)
{
	memset(dat, 0, sizeof(*dat));
}

static inline void vbs_data_set_integer(vbs_data_t *dat, intmax_t v)
{
	dat->type = VBS_INTEGER;
	dat->d_int = v;
}

static inline void vbs_data_set_lstr(vbs_data_t *dat, const char *str, size_t len, bool is_owner)
{
	dat->type = VBS_STRING;
	dat->is_owner = is_owner;
	dat->d_xstr.data = (unsigned char *)str;
	dat->d_xstr.len = len;
}

static inline void vbs_data_set_xstr(vbs_data_t *dat, const xstr_t *xs, bool is_owner)
{
	dat->type = VBS_STRING;
	dat->is_owner = is_owner;
	dat->d_xstr = *xs;
}

static inline void vbs_data_set_blob(vbs_data_t *dat, const void *blob, size_t len, bool is_owner)
{
	dat->type = VBS_BLOB;
	dat->is_owner = is_owner;
	dat->d_xstr.data = (unsigned char *)blob;
	dat->d_xstr.len = len;
	
}

static inline void vbs_data_set_bool(vbs_data_t *dat, bool v)
{
	dat->type = VBS_BOOL;
	dat->d_bool = v;
}

static inline void vbs_data_set_floating(vbs_data_t *dat, double v)
{
	dat->type = VBS_FLOATING;
	dat->d_floating = v;
}

static inline void vbs_data_set_decimal64(vbs_data_t *dat, decimal64_t v)
{
	dat->type = VBS_DECIMAL;
	dat->d_decimal64 = v;
}

static inline void vbs_data_set_null(vbs_data_t *dat)
{
	dat->type = VBS_NULL;
}

static inline void vbs_data_set_list(vbs_data_t *dat, vbs_list_t *list)
{
	dat->type = VBS_LIST;
	dat->d_list = list;
}

static inline void vbs_data_set_dict(vbs_data_t *dat, vbs_dict_t *dict)
{
	dat->type = VBS_DICT;
	dat->d_dict = dict;
}

static inline void vbs_list_init(vbs_list_t *list, int kind)
{
	list->first = NULL;
	list->last = &list->first;
	list->count = 0;
	list->kind = kind > 0 ? kind : 0;
	list->_raw = xstr_null;
}

static inline void vbs_list_push_back(vbs_list_t *list, vbs_litem_t *entry)
{
	entry->next = NULL;
	*list->last = entry;
	list->last = &entry->next;
	list->count++;
}

static inline void vbs_list_push_front(vbs_list_t *list, vbs_litem_t *entry)
{
	if ((entry->next = list->first) == NULL)
		list->last = &entry->next;
	list->first = entry;
	list->count++;
}

static inline vbs_litem_t *vbs_list_pop_front(vbs_list_t *list)
{
	vbs_litem_t *entry = list->first;
	if (entry)
	{
		if ((list->first = entry->next) == NULL)
			list->last = &list->first;
		list->count--;
	}
	return entry;
}


static inline void vbs_dict_init(vbs_dict_t *dict, int kind)
{
	dict->first = NULL;
	dict->last = &dict->first;
	dict->count = 0;
	dict->kind = kind > 0 ? kind : 0;
	dict->_raw = xstr_null;
}

static inline void vbs_dict_push_back(vbs_dict_t *dict, vbs_ditem_t *entry)
{
	entry->next = NULL;
	*dict->last = entry;
	dict->last = &entry->next;
	dict->count++;
}

static inline void vbs_dict_push_front(vbs_dict_t *dict, vbs_ditem_t *entry)
{
	if ((entry->next = dict->first) == NULL)
		dict->last = &entry->next;
	dict->first = entry;
	dict->count++;
}

static inline vbs_ditem_t *vbs_dict_pop_front(vbs_dict_t *dict)
{
	vbs_ditem_t *entry = dict->first;
	if (entry)
	{
		if ((dict->first = entry->next) == NULL)
			dict->last = &dict->first;
		dict->count--;
	}
	return entry;
}


static inline void vbs_ditem_init(vbs_ditem_t *item)
{
	memset(item, 0, sizeof(*item));
}

static inline intmax_t vbs_ditem_key_integer(const vbs_ditem_t *item, intmax_t dft)
{
	return (item->key.type == VBS_INTEGER) ? item->key.d_int : dft;
}

static inline xstr_t vbs_ditem_key_xstr(const vbs_ditem_t *item)
{
	return (item->key.type == VBS_STRING) ? item->key.d_xstr : xstr_null;
}

static inline bool vbs_ditem_value_is_null(const vbs_ditem_t *item)
{
	return (item->value.type == VBS_NULL);
}

static inline intmax_t vbs_ditem_value_integer(const vbs_ditem_t *item, intmax_t dft)
{
	return (item->value.type == VBS_INTEGER) ? item->value.d_int : dft;
}

static inline bool vbs_ditem_value_bool(const vbs_ditem_t *item, bool dft)
{
	return (item->value.type == VBS_BOOL) ? item->value.d_bool : dft;
}

static inline double vbs_ditem_value_floating(const vbs_ditem_t *item, double dft)
{
	return (item->value.type == VBS_FLOATING) ? item->value.d_floating
		: (item->value.type == VBS_INTEGER) ? item->value.d_int
		: dft;
}

static inline decimal64_t vbs_ditem_value_decimal64(const vbs_ditem_t *item, decimal64_t dft)
{
	return (item->value.type == VBS_DECIMAL) ? item->value.d_decimal64 : dft;
}

static inline xstr_t vbs_ditem_value_xstr(const vbs_ditem_t *item)
{
	return (item->value.type == VBS_STRING) ? item->value.d_xstr : xstr_null;
}

static inline xstr_t vbs_ditem_value_blob(const vbs_ditem_t *item)
{
	return (item->value.type == VBS_BLOB) ? item->value.d_blob
		: (item->value.type == VBS_STRING) ? item->value.d_xstr
		: xstr_null;
}

static inline vbs_list_t *vbs_ditem_value_list(const vbs_ditem_t *item)
{
	return (item->value.type == VBS_LIST) ? item->value.d_list : NULL;
}

static inline vbs_dict_t *vbs_ditem_value_dict(const vbs_ditem_t *item)
{
	return (item->value.type == VBS_DICT) ? item->value.d_dict : NULL;
}


static inline void vbs_litem_init(vbs_litem_t *item)
{
	memset(item, 0, sizeof(*item));
}

static inline bool vbs_litem_value_is_null(const vbs_litem_t *item)
{
	return (item->value.type == VBS_NULL);
}

static inline intmax_t vbs_litem_value_integer(const vbs_litem_t *item, intmax_t dft)
{
	return (item->value.type == VBS_INTEGER) ? item->value.d_int : dft;
}

static inline bool vbs_litem_value_bool(const vbs_litem_t *item, bool dft)
{
	return (item->value.type == VBS_BOOL) ? item->value.d_bool : dft;
}

static inline double vbs_litem_value_floating(const vbs_litem_t *item, double dft)
{
	return (item->value.type == VBS_FLOATING) ? item->value.d_floating
		: (item->value.type == VBS_INTEGER) ? item->value.d_int
		: dft;
}

static inline decimal64_t vbs_litem_value_decimal64(const vbs_litem_t *item, decimal64_t dft)
{
	return (item->value.type == VBS_DECIMAL) ? item->value.d_decimal64 : dft;
}

static inline xstr_t vbs_litem_value_xstr(const vbs_litem_t *item)
{
	return (item->value.type == VBS_STRING) ? item->value.d_xstr : xstr_null;
}

static inline xstr_t vbs_litem_value_blob(const vbs_litem_t *item)
{
	return (item->value.type == VBS_BLOB) ? item->value.d_blob
		: (item->value.type == VBS_STRING) ? item->value.d_xstr
		: xstr_null;
}

static inline vbs_list_t *vbs_litem_value_list(const vbs_litem_t *item)
{
	return (item->value.type == VBS_LIST) ? item->value.d_list : NULL;
}

static inline vbs_dict_t *vbs_litem_value_dict(const vbs_litem_t *item)
{
	return (item->value.type == VBS_DICT) ? item->value.d_dict : NULL;
}


intmax_t vbs_dict_get_integer(const vbs_dict_t *d, const char *key, intmax_t dft);
bool vbs_dict_get_bool(const vbs_dict_t *d, const char *key, bool dft);
double vbs_dict_get_floating(const vbs_dict_t *d, const char *key, double dft);	/* VBS_INTEGER or VBS_FLOATING */
decimal64_t vbs_dict_get_decimal64(const vbs_dict_t *d, const char *key, decimal64_t dft);
xstr_t vbs_dict_get_xstr(const vbs_dict_t *d, const char *key);
xstr_t vbs_dict_get_blob(const vbs_dict_t *d, const char *key);			/* VBS_STRING or VBS_BLOB */

vbs_list_t *vbs_dict_get_list(const vbs_dict_t *d, const char *key);
vbs_dict_t *vbs_dict_get_dict(const vbs_dict_t *d, const char *key);
vbs_data_t *vbs_dict_get_data(const vbs_dict_t *d, const char *key);



size_t vbs_buffer_of_descriptor(unsigned char *buf, int descriptor);

size_t vbs_buffer_of_integer(unsigned char *buf, intmax_t value);
size_t vbs_buffer_of_floating(unsigned char *buf, double value);
size_t vbs_buffer_of_decimal64(unsigned char *buf, decimal64_t value);

size_t vbs_head_buffer_of_string(unsigned char *buf, size_t strlen);
size_t vbs_head_buffer_of_blob(unsigned char *buf, size_t bloblen);

size_t vbs_head_buffer_of_list(unsigned char *buf, int kind);
size_t vbs_head_buffer_of_dict(unsigned char *buf, int kind);

#define vbs_byte_of_bool(value)	((unsigned char)((value) ? VBS_BOOL + 1 : VBS_BOOL))
#define vbs_byte_of_null()	((unsigned char)VBS_NULL)
#define vbs_byte_of_tail()	((unsigned char)VBS_TAIL)


size_t vbs_size_of_descriptor(int descriptor);

size_t vbs_size_of_integer(intmax_t value);
size_t vbs_size_of_floating(double value);
size_t vbs_size_of_decimal64(decimal64_t value);

size_t vbs_head_size_of_string(size_t strlen);
size_t vbs_head_size_of_blob(size_t bloblen);

size_t vbs_size_of_string(size_t strlen);
size_t vbs_size_of_blob(size_t bloblen);

#define vbs_size_of_bool(value)		((size_t)1)
#define vbs_size_of_null()		((size_t)1)

size_t vbs_head_size_of_list(int kind);
size_t vbs_head_size_of_dict(int kind);

size_t vbs_size_of_list(const vbs_list_t *list);
size_t vbs_size_of_dict(const vbs_dict_t *dict);
size_t vbs_size_of_data(const vbs_data_t *data);



typedef struct 
{
	ssize_t (*write)(void *cookie, const void *data, size_t size);	/* xio_write_function */
	void *cookie;
	int16_t max_depth;
	int16_t depth;
	int16_t error;
} vbs_packer_t;

#define VBS_PACKER_INIT(WRITE, COOKIE, MAX_DEPTH)	{ (WRITE), (COOKIE), (MAX_DEPTH) }

static inline void vbs_packer_init(vbs_packer_t *job, ssize_t (*write)(void *, const void *, size_t), 
			void *cookie, int16_t max_depth)
{
	job->write = write;
	job->cookie = cookie;
	job->max_depth = max_depth;
	job->depth = 0;
	job->error = 0;
}


/* Return 0 if success. 
   Return a negative number if error.
*/
int vbs_pack_descriptor(vbs_packer_t *job, int descriptor);

int vbs_pack_integer(vbs_packer_t *job, intmax_t value);
int vbs_pack_uinteger(vbs_packer_t *job, uintmax_t value);
int vbs_pack_lstr(vbs_packer_t *job, const void *str, size_t len);
int vbs_pack_xstr(vbs_packer_t *job, const xstr_t *str);
int vbs_pack_cstr(vbs_packer_t *job, const char *str);
int vbs_pack_blob(vbs_packer_t *job, const void *data, size_t len);
int vbs_pack_floating(vbs_packer_t *job, double value);
int vbs_pack_decimal64(vbs_packer_t *job, decimal64_t value);
int vbs_pack_bool(vbs_packer_t *job, bool value);
int vbs_pack_null(vbs_packer_t *job);

int vbs_pack_head_of_list(vbs_packer_t *job, int kind);
int vbs_pack_head_of_dict(vbs_packer_t *job, int kind);
int vbs_pack_tail(vbs_packer_t *job);

#define vbs_pack_head_of_list0(job)	vbs_pack_head_of_list(job, 0)
#define vbs_pack_head_of_dict0(job)	vbs_pack_head_of_dict(job, 0)


int vbs_pack_data(vbs_packer_t *job, const vbs_data_t *data);
int vbs_pack_list(vbs_packer_t *job, const vbs_list_t *list);
int vbs_pack_dict(vbs_packer_t *job, const vbs_dict_t *dict);


int vbs_pack_head_of_string(vbs_packer_t *job, size_t strlen);
int vbs_pack_head_of_blob(vbs_packer_t *job, size_t bloblen);
int vbs_pack_raw(vbs_packer_t *job, const void *buf, size_t n);




typedef struct 
{
	unsigned char *buf;
	unsigned char *cur;
	unsigned char *end;
	int16_t max_depth;
	int16_t depth;
	uint16_t descriptor;
	int16_t error;
} vbs_unpacker_t;

#define VBS_UNPACKER_INIT(BUF, SIZE, MAX_DEPTH)		{ (BUF), (BUF), (BUF)+(SIZE), (MAX_DEPTH), }

static inline void vbs_unpacker_init(vbs_unpacker_t *job, const void *buf, size_t size, int16_t max_depth)
{
	job->buf = (unsigned char *)buf;
	job->cur = (unsigned char *)buf;
	job->end = (unsigned char *)buf + size;
	job->max_depth = max_depth;
	job->depth = 0;
	job->descriptor = 0;
	job->error = 0;
}


int vbs_unpack_integer(vbs_unpacker_t *job, intmax_t *p_value);
int vbs_unpack_lstr(vbs_unpacker_t *job, unsigned char **p_str, ssize_t *p_len);
int vbs_unpack_xstr(vbs_unpacker_t *job, xstr_t *str);
int vbs_unpack_blob(vbs_unpacker_t *job, unsigned char **p_data, ssize_t *p_len);
int vbs_unpack_floating(vbs_unpacker_t *job, double *p_value);
int vbs_unpack_decimal64(vbs_unpacker_t *job, decimal64_t *p_value);
int vbs_unpack_bool(vbs_unpacker_t *job, bool *p_value);
int vbs_unpack_null(vbs_unpacker_t *job);

int vbs_unpack_head_of_list(vbs_unpacker_t *job, int *kind/*NULL*/);
int vbs_unpack_head_of_dict(vbs_unpacker_t *job, int *kind/*NULL*/);
int vbs_unpack_tail(vbs_unpacker_t *job);

int vbs_unpack_int8(vbs_unpacker_t *job, int8_t *p_value);
int vbs_unpack_int16(vbs_unpacker_t *job, int16_t *p_value);
int vbs_unpack_int32(vbs_unpacker_t *job, int32_t *p_value);
int vbs_unpack_int64(vbs_unpacker_t *job, int64_t *p_value);

int vbs_unpack_uint8(vbs_unpacker_t *job, uint8_t *p_value);
int vbs_unpack_uint16(vbs_unpacker_t *job, uint16_t *p_value);
int vbs_unpack_uint32(vbs_unpacker_t *job, uint32_t *p_value);
int vbs_unpack_uint64(vbs_unpacker_t *job, uint64_t *p_value);


/* 
 * The content of list or dict is decoded too. Need to call vbs_release_data()
 * with the same arguments of xm and xm_cookie, if the unpack function return 
 * success and the data value is list or dict.
 */
int vbs_unpack_data(vbs_unpacker_t *job, vbs_data_t *data, const xmem_t *xm, void *xm_cookie);
int vbs_unpack_list(vbs_unpacker_t *job, vbs_list_t *list, const xmem_t *xm, void *xm_cookie);
int vbs_unpack_dict(vbs_unpacker_t *job, vbs_dict_t *dict, const xmem_t *xm, void *xm_cookie);

void vbs_release_data(vbs_data_t *data, const xmem_t *xm, void *xm_cookie);
void vbs_release_list(vbs_list_t *list, const xmem_t *xm, void *xm_cookie);
void vbs_release_dict(vbs_dict_t *dict, const xmem_t *xm, void *xm_cookie);


 
/* Return the type if successfully unpacked or
 * return a negative number if failed.
 * If failed to unpack, the job->cur is not updated.
 * NB: This function will not adjust job->depth. Do it yourself. Be careful.
 */
vbs_type_t vbs_unpack_type(vbs_unpacker_t *job, intmax_t *p_number);


/* Return the data type.
   Or a negative number on error.
   The contents of list or dict are not unpacked, 
   only the tag of list or dict.
 */
int vbs_unpack_primitive(vbs_unpacker_t *job, vbs_data_t *p_data, int *kind/*NULL*/);

/* Call following functions after vbs_unpack_primitive() */
int vbs_unpack_body_of_list(vbs_unpacker_t *job, vbs_list_t *list, const xmem_t *xm, void *xm_cookie);
int vbs_unpack_body_of_dict(vbs_unpacker_t *job, vbs_dict_t *dict, const xmem_t *xm, void *xm_cookie);
int vbs_skip_body_of_list(vbs_unpacker_t *job);
int vbs_skip_body_of_dict(vbs_unpacker_t *job);



/* Return the data type.
   Or a negative number on error.
 */
int vbs_unpack_raw(vbs_unpacker_t *job, unsigned char **pbuf/*NULL*/, ssize_t *plen/*NULL*/);


/* Return the data type of next element.
   The returnted type may be not a valid vbs_type_t.
   The caller should check this itself.
 */
vbs_type_t vbs_peek_type(vbs_unpacker_t *job);


/* If the next type is a correct VBS_TAIL, unpack it and return true.
   Otherwise, return false.
 */
#define vbs_unpack_if_tail(JOB)						\
	(	((JOB)->cur < (JOB)->end && (JOB)->depth > 0 		\
		&& (JOB)->cur[0] == VBS_TAIL)				\
			? ((JOB)->cur++, (JOB)->depth--, true)		\
			: false						\
	)

bool (vbs_unpack_if_tail)(vbs_unpacker_t *job);



int vbs_make_double_value(double *value, intmax_t significant, int expo);

int vbs_make_decimal64_value(decimal64_t *value, intmax_t significant, int expo);



#ifdef __cplusplus
}
#endif

#endif

