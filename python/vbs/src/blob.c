#include "blob.h"

#if PY_MAJOR_VERSION < 3

#include "helper.h"


static PyObject *createBlob;

PyObject *VbsBlob_FromStringAndSize(const unsigned char *data, size_t len)
{
	/* XXX: I don't know how to do it in pure C, so I do it in python.
	   Make a python str first, then call python function:
		return vbs.blob(s)
	 */
	PyObject *result;
	PyObject *s = PyString_FromStringAndSize((char *)data, len);

	PyObject *args = PyTuple_New(1);
	PyTuple_SET_ITEM(args, 0, s);

	result = PyObject_Call(createBlob, args, NULL);
	Py_DECREF(args);
	return result;
}

int VbsBlob_AsStringAndSize(PyObject *obj, unsigned char **data, ssize_t *len)
{
	char *v;
	Py_ssize_t n;
	int rc;
	if (obj->ob_type != &VbsBlob_Type)
	{
		PyErr_SetString(PyExc_TypeError, "Object should be vbs.blob");
		return -1;
	}
	rc = PyString_AsStringAndSize(obj, &v, &n);
	*data = (unsigned char *)v;
	*len = n;
	return rc;
}

static int _blob_init(VbsBlobObject *self, PyObject *args, PyObject *kwds)
{
	if (PyString_Type.tp_init((PyObject *)self, args, kwds) < 0)
		return -1;
	return 0;
}


PyTypeObject VbsBlob_Type =
{
	PyObject_HEAD_INIT(NULL)
	0,                       /* ob_size */
	"vbs.blob",              /* tp_name */
	sizeof(VbsBlobObject),   /* tp_basicsize */
	0,                       /* tp_itemsize */
	0,                       /* tp_dealloc */
	0,                       /* tp_print */
	0,                       /* tp_getattr */
	0,                       /* tp_setattr */
	0,                       /* tp_compare */
	0,                       /* tp_repr */
	0,                       /* tp_as_number */
	0,                       /* tp_as_sequence */
	0,                       /* tp_as_mapping */
	0,                       /* tp_hash */
	0,                       /* tp_call */
	0,                       /* tp_str */
	0,                       /* tp_getattro */
	0,                       /* tp_setattro */
	0,                       /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT |
	  Py_TPFLAGS_BASETYPE,   /* tp_flags */
	0,                       /* tp_doc */
	0,                       /* tp_traverse */
	0,                       /* tp_clear */
	0,                       /* tp_richcompare */
	0,                       /* tp_weaklistoffset */
	0,                       /* tp_iter */
	0,                       /* tp_iternext */
	0,                       /* tp_methods */
	0,                       /* tp_members */
	0,                       /* tp_getset */
	0,                       /* tp_base */
	0,                       /* tp_dict */
	0,                       /* tp_descr_get */
	0,                       /* tp_descr_set */
	0,                       /* tp_dictoffset */
	(initproc)_blob_init,    /* tp_init */
	0,                       /* tp_alloc */
	0,                       /* tp_new */
};


void init_blob(PyObject* module)
{
	VbsBlob_Type.tp_base = &PyString_Type;
	if (PyType_Ready(&VbsBlob_Type) < 0)
		return;

	Py_INCREF((PyObject *)&VbsBlob_Type);
	PyModule_AddObject(module, "blob", (PyObject *)&VbsBlob_Type);

	createBlob = PyObject_GetAttrString(module, "blob");
	if (!PyCallable_Check(createBlob))
	{
		abort();
	}
}


#endif
