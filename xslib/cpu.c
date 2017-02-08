#include "cpu.h"
#include <unistd.h>
#include <errno.h>


int cpu_count(void)
{
	static int n;

	if (n <= 0)
	{
	#if defined (_SC_NPROCESSORS_ONLN)
		n = (int) sysconf(_SC_NPROCESSORS_ONLN);
	#elif defined (_SC_NPROC_ONLN)
		n = (int) sysconf(_SC_NPROC_ONLN);
	#elif defined (HPUX)
	#include <sys/mpctl.h>
		n = mpctl(MPC_GETNUMSPUS, 0, 0);
	#else
		n = -1;
		errno = ENOSYS;
	#endif
	}

	return n;
}

void cpu_alignment_check(bool on)
{
#if defined(__x86_64)
	if (on)
	{
		__asm__("pushf\n"
			"orl $0x40000, (%rsp)\n"
			"popf");
	}
	else
	{
		__asm__("pushf\n"
			"andl $0xfffbffff, (%rsp)\n"
			"popf");
	}
#elif defined(__i386)
	if (on)
	{
		__asm__("pushf\n"
			"orl $0x40000, (%esp)\n"
			"popf");
	}
	else
	{
		__asm__("pushf\n"
			"andl $0xfffbffff, (%esp)\n"
			"popf");
	}
#endif
}

