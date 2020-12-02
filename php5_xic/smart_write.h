#ifndef smart_write_h_ 
#define smart_write_h_ 

#include "php.h"
#include "php_ini.h"
#include "zend_API.h"
#include "zend_variables.h"
#include "php_streams.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	FILE_COOKIE_BUF_SIZE = 1024*4,
};

struct file_cookie {
	php_stream *stream;
	size_t total;
	size_t blen;
	char buf[FILE_COOKIE_BUF_SIZE];
};

void file_cookie_init(struct file_cookie* fc);
ssize_t file_cookie_finish(struct file_cookie* fc);

ssize_t file_write(void *cookie, const void *data, size_t n);

ssize_t smart_write(void *cookie, const void *data, size_t n);


#ifdef __cplusplus
}
#endif

#endif
