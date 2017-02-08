/* $Id: vbs.h,v 1.40 2015/06/16 02:14:44 gremlin Exp $ */
#ifndef VBS_H_
#define VBS_H_ 1

#include "xsdef.h"
#include "vbs_pack.h"
#include "xstr.h"
#include "iobuf.h"
#include "rope.h"
#include "xmem.h"
#include "xformat.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Return 0 if success. 
   Return -1 if error.
*/

int vbs_pack_string_printf(vbs_packer_t *job, xfmt_callback_function cb/*NULL*/,
			const char *fmt, ...) XS_C_PRINTF(3, 4);

int vbs_pack_string_vprintf(vbs_packer_t *job, xfmt_callback_function cb/*NULL*/,
			const char *fmt, va_list ap) XS_C_PRINTF(3, 0);


int vbs_pack_raw_rope(vbs_packer_t *job, const rope_t *rope);



int vbs_copy_data(vbs_data_t *dst, const vbs_data_t *src, const xmem_t *xm, void *xm_cookie);
int vbs_copy_list(vbs_list_t *dst, const vbs_list_t *src, const xmem_t *xm, void *xm_cookie);
int vbs_copy_dict(vbs_dict_t *dst, const vbs_dict_t *src, const xmem_t *xm, void *xm_cookie);



extern const bset_t vbs_meta_bset;


typedef int (*vbs_string_payload_print_function)(iobuf_t *ob, const xstr_t *xs, bool is_blob);

int vbs_string_payload_default_print(iobuf_t *ob, const xstr_t *xs, bool is_blob);

void vbs_set_string_payload_print_function(vbs_string_payload_print_function print);


int vbs_unpack_print_one(vbs_unpacker_t *job, iobuf_t *ob);
int vbs_unpack_print_all(vbs_unpacker_t *job, iobuf_t *ob);


int vbs_print_list(const vbs_list_t *list, iobuf_t *ob);
int vbs_print_dict(const vbs_dict_t *dict, iobuf_t *ob);
int vbs_print_data(const vbs_data_t *pdata, iobuf_t *ob);
int vbs_print_string(const xstr_t *str, iobuf_t *ob);
int vbs_print_blob(const xstr_t *blob, iobuf_t *ob);
int vbs_print_decimal64(decimal64_t dec, iobuf_t *ob);

/*
   VBS_DICT	vbs_dict_t *
   VBS_LIST	vbs_list_t *
   VBS_DATA	vbs_data_t *
   VBS_STRING	xstr_t *
   VBS_BLOB	xstr_t *
   VBS_DECIMAL	decimal64_t *

   VBS_RAW	xstr_t *	## vbs_unpack_print_all()
 */
int vbs_xfmt(iobuf_t *ob, const xfmt_spec_t *spec, void *p);


#ifdef __cplusplus
}
#endif

#endif

