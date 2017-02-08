#include "diskspace.h"
#include "xslib/unixfs.h"
#include "xslib/iobuf.h"
#include "xslib/cstr.h"
#include "xslib/ScopeGuard.h"
#include <sys/vfs.h>
#include <sys/statfs.h>
#include <string.h>
#include <errno.h>
#include <mntent.h>

void diskspace(std::ostream& oss)
{
	FILE *fp = setmntent("/etc/mtab", "rb");
	if (!fp)
		return;
        ON_BLOCK_EXIT(endmntent, fp);

	bool first = true;
	iobuf_t ob = make_iobuf(oss, NULL, 0);
        struct mntent *me;
        while ((me = getmntent(fp)) != NULL)
        {
                const char *path = me->mnt_dir;
		const char *type = me->mnt_type;
		if (type == NULL)
			continue;
		if (cstr_start_with(path, "/proc") && (path[5] == 0 || path[5] == '/'))
			continue;
		if (strcmp(path, "/sys") == 0)
			continue;
		if (cstr_start_with(path, "/dev/") && strcmp(&path[5], "shm") != 0)
			continue;

		struct statfs fs;
		int rc;
		do
		{
			rc = statfs(path, &fs);
		} while (rc != 0 && errno == EINTR);

		if (rc == 0 && fs.f_blocks <= 0)
			continue;

		if (first)
			first = !first;
		else
			iobuf_putc(&ob, ',');

		if (rc != 0)
		{
			iobuf_printf(&ob, "%s:%s~?~?", path, type);
		}
		else
		{
			static const char *unit = "kMGTPE";
			int64_t block_reserved = fs.f_bfree - fs.f_bavail;
			int64_t block_total = fs.f_blocks - block_reserved;
			int64_t block_used = block_total - fs.f_bavail;

			double total = (double)(block_total * fs.f_bsize / 1024);
			int tu = 0;
			while (total > 9999.0)
			{
				total /= 1024;
				if (++tu >= 5)
					break;
			}

			int thousandth = 0;
			if (block_total != 0)
				thousandth = (block_used * 1000 + block_total - 1) / block_total;

			iobuf_printf(&ob, "%s:%s~%.*g%c~%.1f%%", path, type, 
					total<999.5 ? 3 : 4, total, unit[tu],
					(thousandth / 10.0));
		}
	}
}

