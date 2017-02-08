#ifndef BLOB_H_
#define BLOB_H_ 1

#include "Python.h"

#if PY_MAJOR_VERSION < 3

#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
	PyStringObject s;
} VbsBlobObject;

extern PyTypeObject VbsBlob_Type;

void init_blob(PyObject* module);

PyObject *VbsBlob_FromStringAndSize(const unsigned char *data, size_t len);
int VbsBlob_AsStringAndSize(PyObject *, unsigned char **data, ssize_t *len);


#ifdef __cplusplus
}
#endif

#endif /* PY_MAJOR_VERSION < 3 */

#endif
