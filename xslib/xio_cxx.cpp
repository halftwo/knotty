#include "xio.h"
#include <iostream>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: xio_cxx.cpp,v 1.4 2010/10/28 02:36:39 jiagui Exp $";
#endif

static ssize_t _istream_read(std::istream *ism, void *data, size_t size)
{
	if (size > 1)
		ism->read((char *)data, size);
	else if (size == 1)
		ism->get(*(char *)data);

	return ism->fail() ? -1 : ism->gcount();
}

static int _istream_seek(std::istream *ism, int64_t *position, int whence)
{
	std::streamoff offset = *position;
	std::ios_base::seekdir dir = (whence == SEEK_CUR) ? std::ios_base::cur
			: (whence == SEEK_END) ? std::ios_base::end : std::ios_base::beg;

	ism->seekg(offset, dir);
	if (ism->fail())
		return -1;
	
	std::streampos pos = ism->tellg();
	if (pos == (std::streampos)-1)
		return -1;

	*position = pos;
	return 0;
}

const xio_t istream_xio = {
	(xio_read_function)_istream_read,
	NULL,
	(xio_seek_function)_istream_seek,
	NULL,
};


static ssize_t _ostream_write(std::ostream *osm, const void *data, size_t size)
{
	if (size > 1)
		osm->write((const char *)data, size);
	else if (size == 1)
		osm->put(*(const char *)data);

	return osm->fail() ? -1 : size;
}

static int _ostream_seek(std::ostream *osm, int64_t *position, int whence)
{
	std::streamoff offset = *position;
	std::ios_base::seekdir dir = (whence == SEEK_CUR) ? std::ios_base::cur
			: (whence == SEEK_END) ? std::ios_base::end : std::ios_base::beg;

	osm->seekp(offset, dir);
	if (osm->fail())
		return -1;
	
	std::streampos pos = osm->tellp();
	if (pos == (std::streampos)-1)
		return -1;

	*position = pos;
	return 0;
}

const xio_t ostream_xio = {
	NULL,
	(xio_write_function)_ostream_write,
	(xio_seek_function)_ostream_seek,
	NULL,
};

