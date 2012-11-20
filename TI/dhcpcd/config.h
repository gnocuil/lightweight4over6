/* linux */
#define SYSCONFDIR	"/home/gnocuil/network/system/etc/dhcpcd"
#define SBINDIR		"/home/gnocuil/network/system/etc/dhcpcd"
#define LIBEXECDIR	"/home/gnocuil/network/system/etc/dhcpcd"
#define DBDIR		"/home/gnocuil/network/data/misc/dhcp"
#define RUNDIR		"/home/gnocuil/network/data/misc/dhcp"
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
