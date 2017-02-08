/* $Id: vbs_stream.h,v 1.5 2015/05/13 04:21:43 gremlin Exp $ */
#ifndef VBS_STREAM_H_
#define VBS_STREAM_H_ 1

#include "xsdef.h"
#include "vbs_pack.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct 
{
	vbs_unpacker_t uk;
	ssize_t (*read)(void *cookie, void *data, size_t size);
	void *cookie;
	unsigned char _buffer[512];
} vbs_stream_unpacker_t;



/* Return the type if successfully unpacked or
 * return a negative number if failed.
 * NB: This function will not adjust job->uk.depth. Do it yourself. Be careful.
 */
vbs_type_t vbs_stream_unpack_type(vbs_stream_unpacker_t *job, intmax_t *p_number);


bool vbs_stream_unpack_if_tail(vbs_stream_unpacker_t *job);

/* Return 0 if success. 
   Return a negative number if error.
*/
ssize_t vbs_stream_unpack_read(vbs_stream_unpacker_t *job, void *data, size_t size);
ssize_t vbs_stream_unpack_skip(vbs_stream_unpacker_t *job, size_t size);


/* Return 0 if success. 
   Return a negative number if error.
*/
int vbs_stream_unpack_integer(vbs_stream_unpacker_t *job, intmax_t *p_value);
int vbs_stream_unpack_floating(vbs_stream_unpacker_t *job, double *p_value);
int vbs_stream_unpack_decimal64(vbs_stream_unpacker_t *job, decimal64_t *p_value);
int vbs_stream_unpack_bool(vbs_stream_unpacker_t *job, bool *p_value);
int vbs_stream_unpack_null(vbs_stream_unpacker_t *job);

int vbs_stream_unpack_head_of_string(vbs_stream_unpacker_t *job, ssize_t *p_len);
int vbs_stream_unpack_head_of_blob(vbs_stream_unpacker_t *job, ssize_t *p_len);

int vbs_stream_unpack_head_of_list_with_length(vbs_stream_unpacker_t *job, ssize_t *p_len);
int vbs_stream_unpack_head_of_dict_with_length(vbs_stream_unpacker_t *job, ssize_t *p_len);

int vbs_stream_unpack_head_of_list(vbs_stream_unpacker_t *job);
int vbs_stream_unpack_head_of_dict(vbs_stream_unpacker_t *job);
int vbs_stream_unpack_tail(vbs_stream_unpacker_t *job);



#ifdef __cplusplus
}
#endif

#endif

