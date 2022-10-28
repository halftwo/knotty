#include "Python.h"
#include "helper.h"
#include "blob.h"
#include "xslib/xsdef.h"
#include "xslib/vbs_pack.h"
#include "xslib/ostk.h"
#include "xslib/rope.h"
#include "xslib/xstr.h"
#include "xslib/cstr.h"
#include "xslib/strbuf.h"
#include "xslib/ScopeGuard.h"
#include <assert.h>
#include <map>
#include <vector>

#define VBS_V_EDITION	181115
#define VBS_V_REVISION	22102813
#define VBS_VERSION	XS_TOSTR(VBS_V_REVISION) "." XS_TOSTR(VBS_V_RELEASE)

#define BUF_SIZE	4096
#define ITEM_NUM	(BUF_SIZE/sizeof(void *))

static inline int _encode_one(vbs_packer_t *job, PyObject *obj, const char *encoding, const char *errors);
static PyObject* _decode_one(vbs_unpacker_t *job, const char *encoding, const char *errors);

static void pyobj_decref(PyObject *obj)
{
	Py_XDECREF(obj);
}

static PyObject* _unicode_encode(PyObject *o, const char *encoding, const char *errors)
{
	Py_UNICODE *unicode = PyUnicode_AS_UNICODE(o);
	ssize_t size = PyUnicode_GET_SIZE(o);

	if (!errors || !errors[0])
		errors = "strict";

	if (!encoding || !encoding[0])
	{
		return PyUnicode_EncodeUTF8(unicode, size, errors);
	}

        switch (encoding[0])
        {
        case 'u':
        case 'U':
		if (cstr_casecmp(encoding, "utf-8") == 0)
		{
			return PyUnicode_EncodeUTF8(unicode, size, errors);
		}
		else if (cstr_casecmp(encoding, "utf-16") == 0)
		{
			return PyUnicode_EncodeUTF16(unicode, size, errors, -1);
		}
#if PY_VERSION_HEX >= 0x02060000
		else if (cstr_casecmp(encoding, "utf-32") == 0)
		{
			return PyUnicode_EncodeUTF32(unicode, size, errors, -1);
		}
#endif
                break;

        case 'a':
        case 'A':
                if (cstr_casecmp(encoding, "ascii") == 0)
		{
			return PyUnicode_EncodeASCII(unicode, size, errors);
		}
                break;

        case 'l':
        case 'L':
                if (cstr_casecmp(encoding, "latin-1") == 0)
                {
                        return PyUnicode_EncodeLatin1(unicode, size, errors);
                }
                break;
#if MS_WIN32
        case 'm':
        case 'M':
                if (cstr_casecmp(encoding, "mbcs") == 0)
                {
                        return PyUnicode_EncodeMBCS(unicode, size, errors);
                }
                break;
#endif
        }
        
        return PyUnicode_Encode(unicode, size, encoding, errors);
}

static PyObject* _unicode_decode(const xstr_t *xs, const char *encoding, const char *errors)
{
	const char *data = (char *)xs->data;
	size_t len = xs->len;

	if (!errors || !errors[0])
		errors = "strict";

	if (!encoding || !encoding[0])
	{
		return PyUnicode_DecodeUTF8(data, len, errors);
	}

	switch (encoding[0])
	{
	case 'u':
	case 'U':
		if (cstr_casecmp(encoding, "utf-8") == 0)
		{
			return PyUnicode_DecodeUTF8(data, len, errors);
		}
		else if (cstr_casecmp(encoding, "utf-16") == 0)
		{
			int byteOrder = -1;
			return PyUnicode_DecodeUTF16(data, len, errors, &byteOrder);
		}
#if PY_VERSION_HEX >= 0x02060000
		else if (cstr_casecmp(encoding, "utf-32") == 0)
		{
			int byteOrder = -1;
			return PyUnicode_DecodeUTF32(data, len, errors, &byteOrder);
		}
#endif
		break;

	case 'a':
	case 'A':
		if (cstr_casecmp(encoding, "ascii") == 0)
		{
			return PyUnicode_DecodeASCII(data, len, errors);
		}
		break;

	case 'l':
	case 'L':
		if (cstr_casecmp(encoding, "latin-1") == 0)
		{
			return PyUnicode_DecodeLatin1(data, len, errors);
		}
		break;

	#if MS_WIN32
	case 'm':
	case 'M':
		if (cstr_casecmp(encoding, "mbcs") == 0)
		{
			return PyUnicode_DecodeMBCS(data, len, errors);
		}
		break;
	#endif
	}

	return PyUnicode_Decode(data, len, encoding, errors);
}

static int _encode_unicode(vbs_packer_t *job, PyObject *obj, const char *encoding, const char *errors)
{
	PyObject *bs = _unicode_encode(obj, encoding, errors);
	if (!bs)
		return -1;

	ON_BLOCK_EXIT(pyobj_decref, bs);

	char *data;
	Py_ssize_t len;
#if PY_MAJOR_VERSION >= 3
	if (PyBytes_AsStringAndSize(bs, &data, &len) < 0)
		return -1;  
#else
	if (PyString_AsStringAndSize(bs, &data, &len) < 0)
		return -1;
#endif
	return vbs_pack_lstr(job, data, len);
}

static int _pack_tuple(vbs_packer_t *job, PyObject *obj, bool enclosed, const char *encoding, const char *errors)
{
	int rc;

	if (enclosed && (rc = vbs_pack_head_of_list0(job)) < 0)
		return -1;

	Py_ssize_t num = PyTuple_GET_SIZE(obj);
	for (Py_ssize_t i = 0; i < num; ++i)
	{
		PyObject *element = PyTuple_GET_ITEM(obj, i);
		if ((rc = _encode_one(job, element, encoding, errors)) < 0)
			return -1;
	}

	if (enclosed && (rc = vbs_pack_tail(job)) < 0)
		return -1;

	return 0;
}

static inline int _encode_tuple(vbs_packer_t *job, PyObject *obj, const char *encoding, const char *errors)
{
	return _pack_tuple(job, obj, true, encoding, errors);
}

static int _pack_list(vbs_packer_t *job, PyObject *obj, bool enclosed, const char *encoding, const char *errors)
{
	int rc;

	if (enclosed && (rc = vbs_pack_head_of_list0(job)) < 0)
		return -1;

	Py_ssize_t num = PyList_GET_SIZE(obj);
	for (Py_ssize_t i = 0; i < num; ++i)
	{
		PyObject *element = PyList_GET_ITEM(obj, i);
		if ((rc = _encode_one(job, element, encoding, errors)) < 0)
			return -1;
	}

	if (enclosed && (rc = vbs_pack_tail(job)) < 0)
		return -1;

	return 0;
}

static int _encode_list(vbs_packer_t *job, PyObject *obj, const char *encoding, const char *errors)
{
	return _pack_list(job, obj, true, encoding, errors);
}

static int _encode_set(vbs_packer_t *job, PyObject *obj, const char *encoding, const char *errors)
{
	int rc;
	PyObject *element;
	PyObject *iter = PyObject_GetIter(obj);
	if (!iter)
		return -1;

	ON_BLOCK_EXIT(pyobj_decref, iter);

	if ((rc = vbs_pack_head_of_list0(job)) < 0)
		return -1;

	while ((element = PyIter_Next(iter)) != NULL)
	{
		if (_encode_one(job, element, encoding, errors) < 0)
		{
			PyErr_Format(PyExc_Exception, "encode failed, %d", job->error);
			return -1;
		}
	}

	if ((rc = vbs_pack_tail(job)) < 0)
		return -1;

	return 0;
}

static int _encode_dict(vbs_packer_t *job, PyObject *obj, const char *encoding, const char *errors)
{
	int rc;

	if ((rc = vbs_pack_head_of_dict0(job)) < 0)
		return -1;

	PyObject *key = NULL;
	PyObject *value = NULL;
	Py_ssize_t pos = 0;
	while (PyDict_Next(obj, &pos, &key, &value))
	{
		if ((rc = _encode_one(job, key, encoding, errors)) < 0)
			return -1;
		if ((rc = _encode_one(job, value, encoding, errors)) < 0)
			return -1;
	}

	if ((rc = vbs_pack_tail(job)) < 0)
		return -1;

	return 0;
}

static int _encode_others(vbs_packer_t *job, PyObject *obj, const char *encoding, const char *errors)
{
	PyObject *dict = PyObject_GetAttrString(obj, "__dict__");
	if (!dict)
	{
		PyErr_Format(PyExc_Exception, "Can't encode object with type (%s)", obj->ob_type->tp_name);
		return -1;
	}

	ON_BLOCK_EXIT(pyobj_decref, dict);
	return _encode_dict(job, dict, encoding, errors);
}

/* 
 * return <0 on error
 * return =0 on success
 * return >0 if unknown type
 */
static int _encode_by_type(vbs_packer_t *job, PyObject *obj, PyTypeObject *typeObj, const char *encoding, const char *errors)
{
	int rc = -1;

	if (typeObj == &PyLong_Type)
	{
		intmax_t v = PyLong_AsLongLong(obj);
		if (v == -1 && PyErr_Occurred())
		{
			rc = -1;
		}
		else
		{
			rc = vbs_pack_integer(job, v);
		}
	}
#if PY_MAJOR_VERSION < 3
	else if (typeObj == &PyInt_Type)
	{
		long v = PyInt_AsLong(obj);                
		if (v == -1 && PyErr_Occurred())
		{
			rc = -1;
		}
		else
		{
			rc = vbs_pack_integer(job, v);
		}
	}
#endif
	else if (typeObj == &PyUnicode_Type)
	{
		rc = _encode_unicode(job, obj, encoding, errors);
	}
	else if (typeObj == &PyBool_Type)
	{
		bool v = (obj == Py_True);
		rc = vbs_pack_bool(job, v);
	}
	else if (typeObj == &PyFloat_Type)
	{
		double v = PyFloat_AS_DOUBLE(obj);
		rc = vbs_pack_floating(job, v);
	}

#if PY_MAJOR_VERSION >= 3
	else if (typeObj == &PyBytes_Type)
	{
		char *v = PyBytes_AS_STRING(obj);
		Py_ssize_t len = PyBytes_GET_SIZE(obj);
		rc = vbs_pack_blob(job, v, len);
	}
#else /* PY_MAJOR_VERSION >=3 */
	else if (typeObj == &VbsBlob_Type)
	{
		unsigned char *data;
		ssize_t len;
		VbsBlob_AsStringAndSize(obj, &data, &len);
		rc = vbs_pack_blob(job, data, len);
	}
	else if (typeObj == &PyString_Type)
	{
		char *v = PyString_AS_STRING(obj);
		Py_ssize_t len = PyString_GET_SIZE(obj);
		rc = vbs_pack_lstr(job, v, len);
	}
	else if (typeObj == &PyBuffer_Type)
	{
		PyBufferProcs *pb = obj->ob_type->tp_as_buffer;
		void *buf = NULL;
		size_t len = (*pb->bf_getreadbuffer)(obj, 0, &buf);
		rc = vbs_pack_blob(job, buf, len);
	}
#endif /* PY_MAJOR_VERSION >=3 */

#if PY_VERSION_HEX >= 0x02060000
	else if (typeObj == &PyByteArray_Type)
	{
		char *v = PyByteArray_AS_STRING(obj); 
		Py_ssize_t len = PyByteArray_GET_SIZE(obj);
		rc = vbs_pack_blob(job, v, len);
	}
#endif 
	else if (typeObj == &PyList_Type)
	{
		rc = _encode_list(job, obj, encoding, errors);
	}
	else if (typeObj == &PyTuple_Type)
	{
		rc = _encode_tuple(job, obj, encoding, errors);
	}
	else if (typeObj == &PyDict_Type)
	{
		rc = _encode_dict(job, obj, encoding, errors);
	}
	else if (typeObj == &PySet_Type)
	{
		rc = _encode_set(job, obj, encoding, errors);
	}
	else if (typeObj == &PyFrozenSet_Type)
	{
		rc = _encode_set(job, obj, encoding, errors);
	}
	else if (typeObj == Py_None->ob_type)
	{
		rc = vbs_pack_null(job);
	}
	else 
	{
		rc = 1;
	}

	return rc;
}

static inline int _encode_one(vbs_packer_t *job, PyObject *obj, const char *encoding, const char *errors)
{
	PyTypeObject *type = obj->ob_type;
	int rc = _encode_by_type(job, obj, type, encoding, errors);
	if (rc <= 0)
		return rc;

	PyObject *baseTypes = type->tp_bases;
	if (baseTypes)
	{
		ssize_t num = PyTuple_GET_SIZE(baseTypes);
		for (ssize_t i = 0; i < num; ++i)
		{
			type = (PyTypeObject*)PyTuple_GetItem(baseTypes, i);
			rc = _encode_by_type(job, obj, type, encoding, errors);
			if (rc <= 0)
				return rc;
		}
	}

	return _encode_others(job, obj, encoding, errors);
}

static PyObject *_gen_obj(strbuf_t *sb)
{
	PyObject *obj = NULL;
	char *data = strbuf_buf(sb);
	size_t len = strbuf_rlen(sb);

#if PY_MAJOR_VERSION >= 3
	obj = PyBytes_FromStringAndSize(data, len);
#else
	//obj = PyString_FromStringAndSize(data, len);
	obj = VbsBlob_FromStringAndSize((unsigned char *)data, len);
#endif

	return obj;
}

PyDoc_STRVAR(v_encode_doc, 
"encode(obj, encoding=\"utf-8\", errors=\"strict\") -> bytes\n\
\n\
Encode object to bytes or vbs.blob (in python 2.x)\n\
If the obj is an unicode, encode the unicode obj using encoding specified\n\
on the second arguments (default utf-8).");

static PyObject *v_encode(PyObject *self, PyObject *args, PyObject *kwargs)
{
	static char *kwlist[] = {"obj", "encoding", "errors", 0};
	PyObject *obj = NULL;
	const char *encoding = NULL;
	const char *errors = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|ss:encode", kwlist, &obj, &encoding, &errors))
		return NULL;

	char buf[BUF_SIZE];
	strbuf_t sb;
	strbuf_init(&sb, buf, sizeof(buf), false);
	ON_BLOCK_EXIT(strbuf_finish, &sb);
	vbs_packer_t job;
	vbs_packer_init(&job, strbuf_xio.write, &sb, -1);

	if (_encode_one(&job, obj, encoding, errors) < 0)
	{
		PyErr_Format(PyExc_Exception, "encode failed, %d", job.error);
		return NULL;
	}

	return _gen_obj(&sb);
}

PyDoc_STRVAR(v_pack_doc, 
"pack(sequence, encoding=\"utf-8\", errors=\"strict\") -> bytes\n\
\n\
Pack objects in the sequence to bytes or vbs.blob (in python 2.x)\n\
The first argument should be a tuple or a list.\n\
The objects in the tuple (or list) is encoded one by one to the bytes.\n\
This is unlike encoding a tuple or list, which is encoding just one object\n\
(of type tuple or list)");

static PyObject *v_pack(PyObject *self, PyObject *args, PyObject *kwargs)
{
	static char *kwlist[] = {"sequence", "encoding", "errors", 0};
	PyObject *sequence = NULL;
	const char *encoding = NULL;
	const char *errors = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|ss:pack", kwlist, &sequence, &encoding, &errors))
		return NULL;

	char buf[BUF_SIZE];
	strbuf_t sb;
	strbuf_init(&sb, buf, sizeof(buf), false);
	ON_BLOCK_EXIT(strbuf_finish, &sb);
	vbs_packer_t job;
	vbs_packer_init(&job, strbuf_xio.write, &sb, -1);
	int rc = -1;

	if (PyTuple_Check(sequence))
	{
		rc = _pack_tuple(&job, sequence, false, encoding, errors);
	}
	else if (PyList_Check(sequence))
	{
		rc = _pack_list(&job, sequence, false, encoding, errors);
	}
	else
	{
		PyErr_SetString(PyExc_TypeError, "The first argument should be a tuple or list");
		return NULL;
	}

	if (rc < 0)
	{
		PyErr_Format(PyExc_Exception, "encode failed, %d", job.error);
		return NULL;
	}

	return _gen_obj(&sb);
}

static PyObject* _decode_list(vbs_unpacker_t *job, const char *encoding, const char *errors)
{
	PyObject *obj = PyList_New(0);

	while (true)
	{
		if (vbs_unpack_if_tail(job))
			return obj;

		PyObject *element = _decode_one(job, encoding, errors);
		if (!element)
			break;

		int rc = PyList_Append(obj, element);
		Py_DECREF(element);

		if (rc < 0)
			break;
	}

	Py_DECREF(obj);
	return NULL;
}

static PyObject* _decode_dict(vbs_unpacker_t *job, const char *encoding, const char *errors)
{
	PyObject *obj = PyDict_New();

	while(true)
	{
		if (vbs_unpack_if_tail(job))
			return obj;

		PyObject *key = _decode_one(job, encoding, errors);
		if (!key)
			break;

		PyObject *value = _decode_one(job, encoding, errors);
		if (!value)
		{
			Py_DECREF(key);
			break;
		}

		int rc = PyDict_SetItem(obj, key, value);
		Py_DECREF(key);
		Py_DECREF(value);

		if (rc < 0)
			break;
	}

	Py_DECREF(obj);
	return NULL;
}

static PyObject* _decode_one(vbs_unpacker_t *job, const char *encoding, const char *errors)
{
	vbs_data_t dat;
	int variety;
	int rc = vbs_unpack_primitive(job, &dat, &variety);
	if (rc < 0)
	{
		PyErr_SetString(PyExc_Exception, "vbs_unpack_primitive() failed");
		return NULL;
	}

	switch(dat.kind)
	{
	case VBS_INTEGER:
		return PyLong_FromLongLong(dat.d_int);
		break;

	case VBS_STRING:
#if PY_MAJOR_VERSION >= 3
		return _unicode_decode(&dat.d_xstr, encoding, errors);
#else
		if (encoding)
			return _unicode_decode(&dat.d_xstr, encoding, errors);
		else
			return PyString_FromStringAndSize((char *)dat.d_xstr.data, dat.d_xstr.len);
#endif
		break;

	case VBS_BOOL:
		return PyBool_FromLong(dat.d_bool);
		break;

	case VBS_BLOB:        
#if PY_MAJOR_VERSION >= 3
		return PyBytes_FromStringAndSize((char *)dat.d_blob.data, dat.d_blob.len);
#else
		return VbsBlob_FromStringAndSize(dat.d_blob.data, dat.d_blob.len);
#endif
		break;

	case VBS_LIST:
		return _decode_list(job, encoding, errors);
		break;

	case VBS_DICT:
		return _decode_dict(job, encoding, errors);
		break;

	case VBS_NULL:
		Py_INCREF(Py_None);
		return Py_None;
		break;

	case VBS_DECIMAL:
		/* FIXME
		   No decimal type, fall through
		 */
	case VBS_FLOATING:
		return PyFloat_FromDouble(dat.d_floating);
		break;

	case VBS_TAIL:
	default:
		assert(!"Can't reach here!");
		break;
	}
	return NULL;	
}

static bool _get_buf(PyObject *bufobj, xstr_t *buf)
{
#if PY_MAJOR_VERSION >= 3
	if (PyBytes_Check(bufobj))
	{
		buf->data = (unsigned char *)PyBytes_AS_STRING(bufobj);
		buf->len = PyBytes_GET_SIZE(bufobj);
		return true;
	}
#else
	if (PyObject_TypeCheck(bufobj, &VbsBlob_Type))
	{
		VbsBlob_AsStringAndSize(bufobj, &buf->data, &buf->len);
		return true;
	}
	else if (PyString_Check(bufobj))
	{
		buf->data = (unsigned char *)PyString_AS_STRING(bufobj);
		buf->len = PyString_GET_SIZE(bufobj);
		return true;
	}
#endif

#if PY_VERSION_HEX >= 0x02060000
	else if (PyByteArray_Check(bufobj))
	{
		buf->data = (unsigned char *)PyByteArray_AsString(bufobj);
		buf->len = PyByteArray_Size(bufobj);
		return true;
	}

#endif

	PyErr_Format(PyExc_TypeError, "Can't decode type (%s)", bufobj->ob_type->tp_name);
	return false;
}

PyDoc_STRVAR(v_decode_doc,
"decode(buf, encoding=\"utf-8\", errors=\"strict\") -> object\n\
\n\
Decode buf to a python object, the type of buf should be bytes (in python 3.x)\n\
or vbs.blob (or str in python 2.x)");

static PyObject *v_decode(PyObject *self, PyObject *args, PyObject *kwargs)
{
	static char *kwlist[] = {"buf", "encoding", "errors", 0};
	PyObject *bufobj = NULL;
	const char *encoding = NULL;
	const char *errors = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|ss:decode", kwlist, &bufobj, &encoding, &errors))
		return NULL;

	xstr_t buf;
	if (!_get_buf(bufobj, &buf))
		return NULL;	

	vbs_unpacker_t job;
	vbs_unpacker_init(&job, buf.data, buf.len, -1);
	return _decode_one(&job, encoding, errors);
}

static void _free_objs(PyObject **objs, long num)
{
	for (long i = 0; i < num; ++i)
	{
		Py_DECREF(objs[i]);
	}
}

PyDoc_STRVAR(v_unpack_doc,
"unpack(buf, offset=0, num=0, encoding=\"utf-8\", errors=\"strict\")\n\
        -> (consumed, o1, ...)\n\
\n\
Unpack buf (from offset) to python objects.\n\
num specify the number of unpacked objects, 0 means all objects available.\n\
Return a tuple in which the first item is number of bytes comsumed to unpack\n\
the objects, and other items follow the first is the unpacked objects.");

static PyObject *v_unpack(PyObject *self, PyObject *args, PyObject *kwargs)
{
	static char *kwlist[] = {"buf", "offset", "num", "encoding", "errors", 0};
	PyObject *bufobj = NULL;
	long offset = 0, num = 0;
	const char *encoding = NULL;
	const char *errors = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|llss:unpack", kwlist, &bufobj, &offset, &num, &encoding, &errors))
		return NULL;

	xstr_t buf;
	if (!_get_buf(bufobj, &buf))
		return NULL;	

	if (offset >= (ssize_t)buf.len || (offset < 0 && (ssize_t)buf.len + offset < 0))
	{
		PyErr_SetString(PyExc_IndexError, "offset is out of range");
		return NULL;
	}

	if (offset < 0)
	{
		offset += buf.len;
	}

	if (num <= 0)
	{
		num = LONG_MAX;
	}

	vbs_unpacker_t job;
	vbs_unpacker_init(&job, buf.data + offset, buf.len - offset, -1);
	unsigned char *start = job.cur;

	long n = 0;
	long cap = ITEM_NUM;
	PyObject *_objs_[ITEM_NUM];
	PyObject **objs = _objs_;
	for (; (job.cur < job.end && n < num); ++n)
	{
		PyObject *obj = _decode_one(&job, encoding, errors);
		if (!obj)
		{
			_free_objs(objs, n);
			if (objs != _objs_)
				free(objs);
			PyErr_Format(PyExc_Exception, "decode failed (%d) at pos %ld/%ld",
				job.error, (long)(job.cur - job.buf), (long)(job.end - job.buf));
			return NULL;
		}

		if (n >= cap)
		{
			cap += cap;
			PyObject **oldptr = (objs != _objs_) ? objs : NULL;
			PyObject **newptr = (PyObject **)realloc(oldptr, cap * sizeof(objs[0]));
			if (!newptr)
			{
				_free_objs(objs, n);
				if (objs != _objs_)
					free(objs);
				PyErr_NoMemory();
				return NULL;
			}
			objs = newptr;
		}

		objs[n] = obj;
	}

	PyObject *tuple = PyTuple_New(1 + n);
	PyTuple_SET_ITEM(tuple, 0, PyLong_FromLong(job.cur - start));
	for (long i = 0; i < n; ++i)
	{
		PyTuple_SET_ITEM(tuple, i+1, objs[i]);
	}

	if (objs != _objs_)
		free(objs);
	return tuple;
}


static PyMethodDef module_functions[] =
{
	{"encode", (PyCFunction)v_encode, METH_VARARGS|METH_KEYWORDS, v_encode_doc},
	{"decode", (PyCFunction)v_decode, METH_VARARGS|METH_KEYWORDS, v_decode_doc},
	{"pack", (PyCFunction)v_pack, METH_VARARGS|METH_KEYWORDS, v_pack_doc},
	{"unpack", (PyCFunction)v_unpack, METH_VARARGS|METH_KEYWORDS, v_unpack_doc},
	{NULL, NULL}
};


PyDoc_STRVAR(module_doc,
"vbs module");


#if PY_MAJOR_VERSION >= 3

static struct PyModuleDef vbsmodule = 
{
	PyModuleDef_HEAD_INIT,
	"_vbs",   	/* name of module */
	module_doc,
	-1,       	/* size of per-interpreter state of the module,
			or -1 if the module keeps state in global variables. */
	module_functions,
};

PyMODINIT_FUNC PyInit__vbs()
{
	PyObject *module = PyModule_Create(&vbsmodule);
	if (module == NULL)
	{
		if(!PyErr_Occurred())
			PyErr_SetString(PyExc_Exception, "create module failed");
		return NULL;
	}
	return module;
}

#else /* PY_MAJOR_VERSION >= 3 */

PyMODINIT_FUNC init_vbs()
{
	PyObject *module = Py_InitModule3("_vbs", module_functions, module_doc);
	if (module == NULL)
		return;

	init_blob(module);

	PyModule_AddStringConstant(module, "__version__", VBS_VERSION);
}

#endif /* PY_MAJOR_VERSION >= 3 */

