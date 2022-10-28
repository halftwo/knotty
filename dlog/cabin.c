#include "cabin.h"
#include "dlog.h"
#include "xslib/queue.h"
#include "xslib/hmap.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

/*
#define CABIN_BOX_SIZE		(1024*32)
#define MAX_OPEN_FILE		128
#define DEFAULT_LOG_MAX_SIZE	(1024*1024*256)
*/
#define CABIN_BOX_SIZE		(1024)
#define MAX_OPEN_FILE		2
#define DEFAULT_LOG_MAX_SIZE	(1024*16)

struct cabin_t 
{
	char *log_dir;
	int num_open_file;
	int num_box;
	hmap_t *hmap;
	TAILQ_HEAD(, box_t) open_queue;
	TAILQ_HEAD(, box_t) close_queue;
};

typedef struct box_t box_t;
struct box_t
{
	TAILQ_ENTRY(box_t) qlink;
	int fd;
	int file_size;
	char *label;
	char *timestamp;
	char *filename;
	char *begin;
	char *end;
	char *current;
	char _buf[];
};

static void box_flush(cabin_t *cab, box_t *box, const char *content, size_t len);
box_t *box_create(cabin_t *cab, const char *label);
void box_destroy(cabin_t *cab, box_t *box);

cabin_t *cabin_create(const char *log_dir)
{
	cabin_t *cab = (cabin_t *)calloc(1, sizeof(cab[0]));
	cab->hmap = hmap_create(4096, 0);
	TAILQ_INIT(&cab->open_queue);
	TAILQ_INIT(&cab->close_queue);
	cab->log_dir = strdup(log_dir);
	// TODO
	return cab;
}

static bool box_open_file(cabin_t *cab, box_t *box)
{
	if (cab->num_open_file >= MAX_OPEN_FILE)
	{
		box_t *b = TAILQ_FIRST(&cab->open_queue);
		assert(b->fd >= 0);
		box_flush(cab, b, NULL, 0);
		close(b->fd);
		b->fd = -1;
		TAILQ_REMOVE(&cab->open_queue, b, qlink);
		TAILQ_INSERT_TAIL(&cab->close_queue, b, qlink);
		cab->num_open_file--;
	}

	box->fd = open(box->filename, O_WRONLY | O_CREAT | O_APPEND, 0664);
	if (box->fd == -1)
		return false;

	cab->num_open_file++;
	TAILQ_REMOVE(&cab->close_queue, box, qlink);
	TAILQ_INSERT_TAIL(&cab->open_queue, box, qlink);
	return true;
}

static void box_flush(cabin_t *cab, box_t *box, const char *content, size_t len)
{
	if (box->current <= box->begin && len == 0)
		return;

	if (box->fd < 0)
	{
		if (!box_open_file(cab, box))
			return;
	}

	int size = box->current - box->begin;
	int n = write(box->fd, box->begin, size);
	box->current = box->begin;
	if (n <= 0)
	{
		fprintf(stderr, "write() failed, errno=%d, %s\n", errno, strerror(errno));
		return;
	}
	box->file_size += n;

	if (len > 0)
	{
		n = write(box->fd, content, len);
		if (n <= 0)
		{
			fprintf(stderr, "write() failed, errno=%d, %s\n", errno, strerror(errno));
			return;
		}
		box->file_size += n;
		if (content[len-1] != '\n')
		{
			n = write(box->fd, "\n", 1);
			if (n <= 0)
			{
				fprintf(stderr, "write() failed, errno=%d, %s\n", errno, strerror(errno));
				return;
			}
			box->file_size++; 
		}
	}

	if (box->file_size >= DEFAULT_LOG_MAX_SIZE)
	{
		hmap_remove(cab->hmap, box->label);
		box_destroy(cab, box);
	}
}


box_t *box_create(cabin_t *cab, const char *label)
{
	size_t size;
	box_t *box = calloc(1, CABIN_BOX_SIZE);

	box->fd = -1;
	box->file_size = 0;
	box->begin = box->_buf;
	box->end = (char *)box + CABIN_BOX_SIZE;

	size = strlen(label) + 1;
	box->label = box->begin;
	memcpy(box->label, label, size);
	box->begin += size;

	box->timestamp = box->begin;
	dlog_local_time_str(box->timestamp, time(NULL), true);
	box->begin += strlen(box->timestamp) + 1;
	
	box->filename = box->begin;
	size = sprintf(box->filename, "%s/st.%s.%s", cab->log_dir, box->label, box->timestamp);
	box->begin += size + 1;
	
	box->current = box->begin;
	TAILQ_INSERT_TAIL(&cab->close_queue, box, qlink);
	cab->num_box++;
	return box;
}

void box_destroy(cabin_t *cab, box_t *box)
{
	if (box)
	{
		box_flush(cab, box, NULL, 0);
		if (box->fd < 0)
			TAILQ_REMOVE(&cab->close_queue, box, qlink);
		else
		{
			TAILQ_REMOVE(&cab->open_queue, box, qlink);
			cab->num_open_file--;
			close(box->fd);
		}
		cab->num_box--;
		free(box);
	}
}

void cabin_destroy(cabin_t *cab)
{
	if (cab)
	{
		box_t *box;
		while ((box = TAILQ_FIRST(&cab->open_queue)) != NULL)
		{
			box_destroy(cab, box);
		}
		while ((box = TAILQ_FIRST(&cab->close_queue)) != NULL)
		{
			box_destroy(cab, box);
		}
		hmap_destroy(cab->hmap, 0);
		free(cab->log_dir);
		free(cab);
	}
}

void cabin_flush(cabin_t *cab)
{
	box_t *box;
	TAILQ_FOREACH(box, &cab->open_queue, qlink)
	{
		box_flush(cab, box, NULL, 0);
	}
	TAILQ_FOREACH(box, &cab->close_queue, qlink)
	{
		box_flush(cab, box, NULL, 0);
	}
}

void cabin_put(cabin_t *cab, const char *label, const char *content, size_t len)
{
	if (len == 0)
		return;

	box_t *box = (box_t *)hmap_find(cab->hmap, label);
	if (!box)
	{
		box = box_create(cab, label);
		hmap_insert(cab->hmap, label, box);
	}
	
	if (box->current + len < box->end)
	{
		memcpy(box->current, content, len);
		box->current += len;
		if (content[len-1] != '\n')
			*box->current++ = '\n';
		return;
	}

	box_flush(cab, box, content, len);
}

#ifdef TEST_CABIN

int main()
{
	cabin_t *cab = cabin_create("/tmp/ddd");
	int i;
	for (i = 0; i < 100; ++i)
	{
		char label[32];
		char content[256];
		int len;
		sprintf(label, "label_%d", i);
		len = sprintf(content, "hello, world! %d", i);
		cabin_put(cab, label, content, len);
	}
	cabin_destroy(cab);
	return 0;
}

#endif
