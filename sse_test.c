#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

int main()
{
	int sse_ctl[] = { CTL_MACHDEP, 2 };
	size_t size;

	if(sysctl(sse_ctl, 2, NULL, &size, NULL, 0) < 0) {
		perror("sysctl: error getting buffer size");
		return EXIT_FAILURE;
	}

	return 0;
}
