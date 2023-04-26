#include <zephyr/kernel.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <zephyr/net/socket.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>


#define MG_PUTCHAR(x) printk("%c", x)
#ifndef strdup
#define strdup(s) ((char *) mg_strdup(mg_str(s)).ptr)
#endif
#define strerror(x) zsock_gai_strerror(x)
#ifndef FD_CLOEXEC
#define FD_CLOEXEC 0
#endif
#ifndef F_SETFD
#define F_SETFD 0
#endif
#define MG_ENABLE_SSI 0

int rand(void);
int sscanf(const char *, const char *, ...);

#if defined(__unix) || defined(__unix__) || defined(__APPLE__)
#include <signal.h>
#endif

