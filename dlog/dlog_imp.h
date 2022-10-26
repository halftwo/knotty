#ifndef DLOGD_IMP_H_
#define DLOGD_IMP_H_ 1

#include "xslib/xsdef.h"
#include "xslib/xformat.h"
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif


#define DLOG_V_EDITION 		221026
#define DLOG_V_REVISION 	221026
#define DLOG_V_RELEASE 		21

#define DLOG_VERSION		XS_TOSTR(DLOG_V_EDITION) "." XS_TOSTR(DLOG_V_REVISION) "." XS_TOSTR(DLOG_V_RELEASE)


#define DLOG_CENTER_PORT		6108
#define DLOGD_PORT			6109

#define DLOG_RECORD_VERSION		6
#define DLOG_PACKET_VERSION		6

#define DLOG_TYPE_RAW 			0
#define DLOG_TYPE_COOKED		1
#define DLOG_TYPE_SYS			2
#define DLOG_TYPE_ALERT			3

#define DLOG_PACKET_FLAG_BIG_ENDIAN	0x01
#define DLOG_PACKET_FLAG_LZO		0x02	// Deprecated
#define DLOG_PACKET_FLAG_LZ4		0x04
#define DLOG_PACKET_FLAG_IPV6		0x08


#define DLOG_IDENTITY_MAX		63
#define DLOG_TAG_MAX			63
#define DLOG_LOCUS_MAX			127

#define DLOG_RECORD_HEAD_SIZE		offsetof(struct dlog_record, str)
#define DLOG_PACKET_HEAD_SIZE		offsetof(struct dlog_packet, buf)


/* The string length of the whole log line should be less than 4096.
   The first 3 items of the log line
		@datetime ' ' ex_ip6/in_ip6 ' ' pid+port ' '
   have a total length of 112 (1+13+1+39+1+39+1+10+1+5+1).
   The length of the log line trailing ("\a ...\r\n") may be as great as 7.
   And the DLOG_RECORD_HEAD_SIZE is 18.
  
   So the DLOG_RECORD_MAX_SIZE should be less than 4096 - (112 + 7 - 18) = 3995
 */
#define DLOG_LOGLINE_MAX_SIZE		4096
#define DLOG_RECORD_MAX_SIZE		3992
#define DLOG_PACKET_MAX_SIZE		(65536-256)


#define dlog_record 			dlog_record_v6


struct dlog_record_v5
{
	uint16_t size;          /* include the size itself and trailing '\0' */

	/* If bigendian is 1, the byte order is big endian.
	   Otherwise, the byte order is native order.
	 */
#if __BYTE_ORDER == __LITTLE_ENDIAN
	uint8_t version:3, bigendian:1, type:3, truncated:1;
#elif __BYTE_ORDER == __BIG_ENDIAN
	uint8_t truncated:1, type:3, bigendian:1, version:3;
#else
# error "unsupported endian"
#endif 
	uint8_t locus_end;
	uint16_t port;
	uint16_t pid;
	int64_t msec;		/* milliseconds from unix epoch */
	char str[];
};

struct dlog_record_v6
{
	uint16_t size;          /* include the size itself and trailing '\0' */

	/* If bigendian is 1, the byte order is big endian.
	   Otherwise, the byte order is native order.
	 */
#if __BYTE_ORDER == __LITTLE_ENDIAN
	uint8_t version:3, bigendian:1, type:3, truncated:1;
#elif __BYTE_ORDER == __BIG_ENDIAN
	uint8_t truncated:1, type:3, bigendian:1, version:3;
#else
# error "unsupported endian"
#endif 
	uint8_t locus_end;
	uint32_t pid;
	int64_t msec;		/* milliseconds from unix epoch */
	uint16_t port;
	char str[];
};


struct dlog_packet
{
	uint32_t size;		/* include the size itself */
	uint8_t version;
	uint8_t flag;
	uint16_t _reserved;
	int64_t msec;		/* milliseconds from unix epoch */
	uint8_t ip64[16];
	char buf[];
};


void dlog_compose(struct dlog_record *rec, const xstr_t *identity,
		const xstr_t *tag, const xstr_t *locus, const xstr_t *content);

void dlog_vmake(struct dlog_record *rec, xfmt_callback_function callback,
		const char *identity, const char *tag, const char *locus,
		const char *format, va_list ap) XS_C_PRINTF(6, 0);

void dlog_make(struct dlog_record *rec, xfmt_callback_function callback,
		const char *identity, const char *tag, const char *locus,
		const char *format, ...) XS_C_PRINTF(6, 7);


#ifdef __cplusplus
}
#endif

#endif

