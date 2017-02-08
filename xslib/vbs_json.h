/* $Id: vbs_json.h,v 1.4 2015/05/27 12:46:54 gremlin Exp $ */
#ifndef VBS_JSON_H_
#define VBS_JSON_H_

#include "vbs_pack.h"
#include "xio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* NB:
 *
 * Duplicate keys in vbs dict or json object are not detected.
 *
 * VBS_BLOB value will be encoded as "\u0000" + Base64(value) in JSON.
 * And any JSON string begin with "\u0000" will be treated as binary
 * data. The string after the initial "\u0000" will be base64 decoded.
 * If the base64 decoding process fails, the function fails.
 *
 */

/* Assuming the json string is encoded using UTF-8. 
 * flags is not used now, should be 0.
 * Return consumed bytes of the JSON string (a positive number) on success.
 * Return a negative number on failure.
 */
ssize_t json_to_vbs(const void *json, size_t size, xio_write_function xio_write, void *xio_ctx, int flags);


/* 
 * Decode JSON string to vbs_data_t, vbs_list_t, or vbs_dict_t data structure.
 * Need to call vbs_release_data(), vbs_release_list(), or vbs_list_dict() 
 * with the same arguments of xm and xm_cookie, if these function return 
 * success.
 * Return consumed bytes of the JSON string (a positive number) on success.
 * Return a negative number on failure.
 */
ssize_t json_unpack_vbs_data(const void *json, size_t size, vbs_data_t *data, const xmem_t *xm, void *xm_cookie);
ssize_t json_unpack_vbs_list(const void *json, size_t size, vbs_list_t *list, const xmem_t *xm, void *xm_cookie);
ssize_t json_unpack_vbs_dict(const void *json, size_t size, vbs_dict_t *dict, const xmem_t *xm, void *xm_cookie);



enum
{
	VBS_JSON_ESCAPE_UNICODE = 0x01,
};

/* Assuming the string characters in the vbs is encoded using UTF-8. 
 * Return consumed bytes of the VBS binary string (a positive number) on success.
 * Return a negative number on failure.
 */
ssize_t vbs_to_json(const void *vbs, size_t size, xio_write_function xio_write, void *xio_ctx, int flags);


/* Encode vbs_data_t, vbs_list_t, or vbs_dict_t as JSON string.
 * Return 0 on success.
 * Return a negative number on failure.
 */
int json_pack_vbs_data(const vbs_data_t *data, xio_write_function xio_write, void *xio_ctx, int flags);
int json_pack_vbs_list(const vbs_list_t *list, xio_write_function xio_write, void *xio_ctx, int flags);
int json_pack_vbs_dict(const vbs_dict_t *dict, xio_write_function xio_write, void *xio_ctx, int flags);



#ifdef __cplusplus
}
#endif

#endif
