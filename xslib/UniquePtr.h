#ifndef UniquePtr_h_
#define UniquePtr_h_

#include <memory>
#include <algorithm>
#include <utility>


#if __cplusplus < 201103L
#define UniquePtr std::auto_ptr
#else
#define UniquePtr std::unique_ptr
#endif


#endif
