/* linux */
#ifdef __ARCH_LINUX__

#define SYSCONFDIR	"/etc"
#define SBINDIR		"/sbin"
#define LIBEXECDIR	"/usr/lib/dhcpcd"
#define DBDIR		"/var/lib/dhcpcd"
#define RUNDIR		"/var/run"

#else /* for Android */

#define SYSCONFDIR	"/system/etc/dhcpcd"
#define SBINDIR		"/system/etc/dhcpcd"
#define LIBEXECDIR	"/system/etc/dhcpcd"
#define DBDIR		"/data/misc/dhcp"
#define RUNDIR		"/data/misc/dhcp"

#endif

#include "compat/arc4random.h"
#include "compat/closefrom.h"
#include "compat/strlcpy.h"
#include "compat/getline.h"

#ifndef MAX
#define MAX(a,b)	((a) >= (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b)	((a) <= (b) ? (a) : (b))
#endif
