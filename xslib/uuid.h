#ifndef uuid_h_
#define uuid_h_

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Not including the trailing '\0' byte. */
#define UUID_STRING_LEN		36


typedef unsigned char uuid_t[16];


void uuid_generate(uuid_t uuid);

void uuid_generate_random(uuid_t uuid);

void uuid_generate_time(uuid_t uuid);
 

size_t uuid_string(const uuid_t uuid, char buf[], size_t len);



#ifdef __cplusplus
}
#endif

#endif
