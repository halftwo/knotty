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


#define DLOG_V_EDITION 		200226
#define DLOG_V_REVISION 	200226
#define DLOG_V_RELEASE 		15

#define DLOG_VERSION		XS_TOSTR(DLOG_V_EDITION) "." XS_TOSTR(DLOG_V_REVISION) "." XS_TOSTR(DLOG_V_RELEASE)


#define DLOG_CENTER_PORT		6108
#define DLOGD_PORT			6109

#define DLOG_RECORD_VERSION		5
#define DLOG_PACKET_VERSION		5

#define DLOG_TYPE_RAW 			0
#define DLOG_TYPE_COOKED		1
#define DLOG_TYPE_SYS			2
#define DLOG_TYPE_ALERT			3

#define DLOG_PACKET_FLAG_BIG_ENDIAN	0x01
#define DLOG_PACKET_FLAG_LZO		0x02
#define DLOG_PACKET_FLAG_LZ4		0x04
#define DLOG_PACKET_FLAG_IPV6		0x08


#define DLOG_IDENTITY_MAX		63
#define DLOG_TAG_MAX			63
#define DLOG_LOCUS_MAX			127

#define DLOG_RECORD_HEAD_SIZE		offsetof(struct dlog_record, str)
#define DLOG_PACKET_HEAD_SIZE		offsetof(struct dlog_packet, buf)

#define DLOG_RECORD_MAX_SIZE		(4000)
#define DLOG_PACKET_MAX_SIZE		(65536-256)


#define dlog_record 			dlog_record_v5
#define dlog_packet			dlog_packet_v5



struct dlog_timeval		/* Obsolete */ 
{
	int32_t tv_sec;
	int32_t tv_usec;
};

struct dlog_record_v3
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
	int16_t pid;
	struct dlog_timeval time;
	char str[];
};

struct dlog_record_v4
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
	int64_t usec;		/* microseconds from unix epoch */
	char str[];
};

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


struct dlog_packet_v3
{
	uint32_t size;		/* include the size itself */
	uint8_t version;
	uint8_t flag;
	uint8_t _reserved1;
	uint8_t _reserved2;
	struct dlog_timeval time;
	uint8_t ip64[16];
	char buf[];
};

struct dlog_packet_v4
{
	uint32_t size;		/* include the size itself */
	uint8_t version;
	uint8_t flag;
	uint8_t _reserved1;
	uint8_t _reserved2;
	int64_t usec;		/* microseconds from unix epoch */
	uint8_t ip64[16];
	char buf[];
};

struct dlog_packet_v5
{
	uint32_t size;		/* include the size itself */
	uint8_t version;
	uint8_t flag;
	uint8_t _reserved1;
	uint8_t _reserved2;
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

