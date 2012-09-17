/* common.c - common useful functions

   Copyright (C) 2000  Russell Kroll <rkroll@exploits.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "common.h"

#include <ctype.h>
#ifndef WIN32
#include <syslog.h>
#include <pwd.h>
#include <grp.h>
#else
#include <wincompat.h>
#endif

/* the reason we define UPS_VERSION as a static string, rather than a
	macro, is to make dependency tracking easier (only common.o depends
	on nut_version_macro.h), and also to prevent all sources from
	having to be recompiled each time the version changes (they only
	need to be re-linked). */
#include "nut_version.h"
const char *UPS_VERSION = NUT_VERSION_MACRO;

	int	nut_debug_level = 0;
	int	nut_log_level = 0;
	static	int	upslog_flags = UPSLOG_STDERR;

static void xbit_set(int *val, int flag)
{
	*val |= flag;
}

static void xbit_clear(int *val, int flag)
{
	*val ^= (*val & flag);
}

static int xbit_test(int val, int flag)
{
	return ((val & flag) == flag);
}

/* enable writing upslog_with_errno() and upslogx() type messages to 
   the syslog */
void syslogbit_set(void)
{
	xbit_set(&upslog_flags, UPSLOG_SYSLOG);
}

/* get the syslog ready for us */
void open_syslog(const char *progname)
{
#ifndef WIN32
	int	opt;

	opt = LOG_PID;

	/* we need this to grab /dev/log before chroot */
#ifdef LOG_NDELAY
	opt |= LOG_NDELAY;
#endif

	openlog(progname, opt, LOG_FACILITY);

	switch (nut_log_level)
	{
#if HAVE_SETLOGMASK && HAVE_DECL_LOG_UPTO
	case 7:
		setlogmask(LOG_UPTO(LOG_EMERG));	/* system is unusable */
		break;
	case 6:
		setlogmask(LOG_UPTO(LOG_ALERT));	/* action must be taken immediately */
		break;
	case 5:
		setlogmask(LOG_UPTO(LOG_CRIT));		/* critical conditions */
		break;
	case 4:
		setlogmask(LOG_UPTO(LOG_ERR));		/* error conditions */
		break;
	case 3:
		setlogmask(LOG_UPTO(LOG_WARNING));	/* warning conditions */
		break;
	case 2:
		setlogmask(LOG_UPTO(LOG_NOTICE));	/* normal but significant condition */
		break;
	case 1:
		setlogmask(LOG_UPTO(LOG_INFO));		/* informational */
		break;
	case 0:
		setlogmask(LOG_UPTO(LOG_DEBUG));	/* debug-level messages */
		break;
	default:
                fatalx(EXIT_FAILURE, "Invalid log level threshold");
#else
	case 0:
		break;
	default:
		upslogx(LOG_INFO, "Changing log level threshold not possible");
		break;
#endif
	}
#else
	EventLogName = progname;
#endif
}

/* close ttys and become a daemon */
void background(void)
{
#ifndef WIN32
	int	pid;

	if ((pid = fork()) < 0)
		fatal_with_errno(EXIT_FAILURE, "Unable to enter background");

	xbit_set(&upslog_flags, UPSLOG_SYSLOG);
	xbit_clear(&upslog_flags, UPSLOG_STDERR);

	close(0);
	close(1);
	close(2);

	if (pid != 0) 
		_exit(EXIT_SUCCESS);		/* parent */

	/* child */

	/* make fds 0-2 point somewhere defined */
	if (open("/dev/null", O_RDWR) != 0)
		fatal_with_errno(EXIT_FAILURE, "open /dev/null");

	if (dup(0) == -1)
		fatal_with_errno(EXIT_FAILURE, "dup");

	if (dup(0) == -1)
		fatal_with_errno(EXIT_FAILURE, "dup");

#ifdef HAVE_SETSID
	setsid();		/* make a new session to dodge signals */
#endif
#else /* WIN32 */
	xbit_set(&upslog_flags, UPSLOG_SYSLOG);
	xbit_clear(&upslog_flags, UPSLOG_STDERR);
	upslogx(LOG_INFO, "Startup successful");
#endif
}

/* do this here to keep pwd/grp stuff out of the main files */
struct passwd *get_user_pwent(const char *name)
{
#ifndef WIN32
	struct passwd *r;
	errno = 0;
	if ((r = getpwnam(name)))
		return r;

	/* POSIX does not specify that "user not found" is an error, so
	   some implementations of getpwnam() do not set errno when this
	   happens. */
	if (errno == 0)
		fatalx(EXIT_FAILURE, "user %s not found", name);
	else
		fatal_with_errno(EXIT_FAILURE, "getpwnam(%s)", name);
#endif	
	return NULL;  /* to make the compiler happy */
}

/* change to the user defined in the struct */
void become_user(struct passwd *pw)
{
#ifndef WIN32
	/* if we can't switch users, then don't even try */
	if ((geteuid() != 0) && (getuid() != 0))
		return;

	if (getuid() == 0)
		if (seteuid(0))
			fatal_with_errno(EXIT_FAILURE, "getuid gave 0, but seteuid(0) failed");

	if (initgroups(pw->pw_name, pw->pw_gid) == -1)
		fatal_with_errno(EXIT_FAILURE, "initgroups");

	if (setgid(pw->pw_gid) == -1)
		fatal_with_errno(EXIT_FAILURE, "setgid");

	if (setuid(pw->pw_uid) == -1)
		fatal_with_errno(EXIT_FAILURE, "setuid");
#endif
}

/* drop down into a directory and throw away pointers to the old path */
void chroot_start(const char *path)
{
	if (chdir(path))
		fatal_with_errno(EXIT_FAILURE, "chdir(%s)", path);
#ifndef WIN32
	if (chroot(path))
		fatal_with_errno(EXIT_FAILURE, "chroot(%s)", path);
#endif
	if (chdir("/"))
		fatal_with_errno(EXIT_FAILURE, "chdir(/)");

	upsdebugx(1, "chrooted into %s", path);
}

#ifdef WIN32
/* In WIN32 all non binaries files (namely configuration and PID files)
   are retrieved relative to the path of the binary itself.
   So this function fill "dest" with the full path to "relative_path" 
   depending on the .exe path */
char * getfullpath(char * relative_path)
{
	char buf[SMALLBUF];
	if ( GetModuleFileName(NULL,buf,SMALLBUF) == 0 ) {
		return NULL;
	}

	/* remove trailing executable name and its preceeding slash*/
	char * last_slash = strrchr(buf,'\\');
	*last_slash = 0;

	if( relative_path ) {
		strncat(buf,relative_path,SMALLBUF);
	}

	return(xstrdup(buf));
}
#endif

/* drop off a pidfile for this process */
void writepid(const char *name)
{
#ifndef WIN32
	char	fn[SMALLBUF];
	FILE	*pidf;
	int	mask;

	/* use full path if present, else build filename in PIDPATH */
	if (*name == '/')
		snprintf(fn, sizeof(fn), "%s", name);
	else {
		snprintf(fn, sizeof(fn), "%s/%s.pid", PIDPATH, name);
	}

	mask = umask(022);
	pidf = fopen(fn, "w");

	if (pidf) {
		fprintf(pidf, "%d\n", (int) getpid());
		fclose(pidf);
	} else {
		upslog_with_errno(LOG_NOTICE, "writepid: fopen %s", fn);
	}

	umask(mask);
#endif
}

/* open pidfn, get the pid, then send it sig */
#ifndef WIN32
int sendsignalfn(const char *pidfn, int sig)
{
	char	buf[SMALLBUF];
	FILE	*pidf;
	int	pid;
	int	ret;

	pidf = fopen(pidfn, "r");

	if (!pidf) {
		upslog_with_errno(LOG_NOTICE, "fopen %s", pidfn);
		return -1;
	}

	if (fgets(buf, sizeof(buf), pidf) == NULL) {
		upslogx(LOG_NOTICE, "Failed to read pid from %s", pidfn);
		fclose(pidf);
		return -1;
	}	

	pid = strtol(buf, (char **)NULL, 10);

	if (pid < 2) {
		upslogx(LOG_NOTICE, "Ignoring invalid pid number %d", pid);
		fclose(pidf);
		return -1;
	}

	/* see if this is going to work first */
	ret = kill(pid, 0);

	if (ret < 0) {
		perror("kill");
		fclose(pidf);
		return -1;
	}

	/* now actually send it */
	ret = kill(pid, sig);

	if (ret < 0) {
		perror("kill");
		fclose(pidf);
		return -1;
	}

	return 0;
}
#else
int sendsignalfn(const char *pidfn, const char * sig)
{
	BOOL	ret;

	ret = send_to_named_pipe(pidfn,sig);

	if (ret != 0) {
		return -1;
	}

	return 0;
}
#endif

int snprintfcat(char *dst, size_t size, const char *fmt, ...)
{
	va_list ap;
	size_t len = strlen(dst);
	int ret;

	size--;
	assert(len <= size);

	va_start(ap, fmt);
	ret = vsnprintf(dst + len, size - len, fmt, ap);
	va_end(ap);

	dst[size] = '\0';
	return len + ret;
}

/* lazy way to send a signal if the program uses the PIDPATH */
#ifndef WIN32
int sendsignal(const char *progname, int sig)
{
	char	fn[SMALLBUF];

	snprintf(fn, sizeof(fn), "%s/%s.pid", PIDPATH, progname);
	return sendsignalfn(fn, sig);
}
#else
int sendsignal(const char *progname, const char * sig)
{
	return sendsignalfn(progname, sig);
}
#endif

const char *xbasename(const char *file)
{
#ifndef WIN32
	const char *p = strrchr(file, '/');
#else
	const char *p = strrchr(file, '\\');
	const char *r = strrchr(file, '/');
	/* if not found, try '/' */
	if( r > p ) {
		p = r;
	}
#endif

	if (p == NULL)
		return file;
	return p + 1;
}

static void vupslog(int priority, const char *fmt, va_list va, int use_strerror)
{
	int	ret;
	char	buf[LARGEBUF];

	ret = vsnprintf(buf, sizeof(buf), fmt, va);

	if ((ret < 0) || (ret >= (int) sizeof(buf)))
		syslog(LOG_WARNING, "vupslog: vsnprintf needed more than %d bytes",
			LARGEBUF);
	

	if (use_strerror) {
		snprintfcat(buf, sizeof(buf), ": %s", strerror(errno));
#ifdef WIN32
		LPVOID WinBuf;
		DWORD WinErr = GetLastError();
		FormatMessage(
				FORMAT_MESSAGE_MAX_WIDTH_MASK |
				FORMAT_MESSAGE_ALLOCATE_BUFFER | 
				FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				WinErr,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPTSTR) &WinBuf,
				0, NULL );

		snprintfcat(buf, sizeof(buf), " [%s]", (char *)WinBuf);
		LocalFree(WinBuf);
#endif
	}

	if (nut_debug_level > 0) {
		static struct timeval	start = { 0 };
		struct timeval		now;
	
		gettimeofday(&now, NULL);
	
		if (start.tv_sec == 0) {
			start = now;
		}
	
		if (start.tv_usec > now.tv_usec) {
			now.tv_usec += 1000000;
			now.tv_sec -= 1;
		}
	
		fprintf(stderr, "%4.0f.%06ld\t", difftime(now.tv_sec, start.tv_sec), (long)(now.tv_usec - start.tv_usec));
	}

	if (xbit_test(upslog_flags, UPSLOG_STDERR))
		fprintf(stderr, "%s\n", buf);
	if (xbit_test(upslog_flags, UPSLOG_SYSLOG))
		syslog(priority, "%s", buf);
}


/* Return the default path for the directory containing configuration files */
const char * confpath(void)
{
#ifndef WIN32
	const char *path = getenv("NUT_CONFPATH");
#else
	static const char *path = NULL;
	if (path == NULL) {
		path = getfullpath(PATH_ETC);
	}
#endif
	return (path != NULL) ? path : CONFPATH;
}

/* Return the default path for the directory containing state files */
const char * dflt_statepath(void)
{
#ifndef WIN32
	const char *path = getenv("NUT_STATEPATH");
#else
	static const char *path = NULL;
	if (path == NULL) {
		path = getfullpath(PATH_VAR_RUN);
	}
#endif
	return (path != NULL) ? path : STATEPATH;
}

/* Return the alternate path for pid files */
const char * altpidpath(void) 
{
#ifdef ALTPIDPATH
	return ALTPIDPATH;
#else
	return dflt_statepath();
#endif
}

/* logs the formatted string to any configured logging devices + the output of strerror(errno) */
void upslog_with_errno(int priority, const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vupslog(priority, fmt, va, 1);
	va_end(va);
}

/* logs the formatted string to any configured logging devices */
void upslogx(int priority, const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vupslog(priority, fmt, va, 0);
	va_end(va);
}

void upsdebug_with_errno(int level, const char *fmt, ...)
{
	va_list va;
	
	if (nut_debug_level < level)
		return;

	va_start(va, fmt);
	vupslog(LOG_DEBUG, fmt, va, 1);
	va_end(va);
}

void upsdebugx(int level, const char *fmt, ...)
{
	va_list va;
	
	if (nut_debug_level < level)
		return;

	va_start(va, fmt);
	vupslog(LOG_DEBUG, fmt, va, 0);
	va_end(va);
}

/* dump message msg and len bytes from buf to upsdebugx(level) in
   hexadecimal. (This function replaces Philippe Marzouk's original
   dump_hex() function) */
void upsdebug_hex(int level, const char *msg, const void *buf, int len)
{
	char line[100];
	int n;	/* number of characters currently in line */
	int i;	/* number of bytes output from buffer */

	n = snprintf(line, sizeof(line), "%s: (%d bytes) =>", msg, len); 

	for (i = 0; i < len; i++) {

		if (n > 72) {
			upsdebugx(level, "%s", line);
			line[0] = 0;
		}

		n = snprintfcat(line, sizeof(line), n ? " %02x" : "%02x",
			((unsigned char *)buf)[i]);
	}
	upsdebugx(level, "%s", line);
}

static void vfatal(const char *fmt, va_list va, int use_strerror)
{
	if (xbit_test(upslog_flags, UPSLOG_STDERR_ON_FATAL))
		xbit_set(&upslog_flags, UPSLOG_STDERR);
	if (xbit_test(upslog_flags, UPSLOG_SYSLOG_ON_FATAL))
		xbit_set(&upslog_flags, UPSLOG_SYSLOG);

	vupslog(LOG_ERR, fmt, va, use_strerror);
}

void fatal_with_errno(int status, const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vfatal(fmt, va, (errno > 0) ? 1 : 0);
	va_end(va);

	exit(status);
}

void fatalx(int status, const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vfatal(fmt, va, 0);
	va_end(va);

	exit(status);
}

static const char *oom_msg = "Out of memory";

void *xmalloc(size_t size)
{
	void *p = malloc(size);

	if (p == NULL)
		fatal_with_errno(EXIT_FAILURE, "%s", oom_msg);
	memset(p,0,size);
	return p;
}

void *xcalloc(size_t number, size_t size)
{
	void *p = calloc(number, size);

	if (p == NULL)
		fatal_with_errno(EXIT_FAILURE, "%s", oom_msg);
	memset(p,0,size*number);
	return p;
}

void *xrealloc(void *ptr, size_t size)
{
	void *p = realloc(ptr, size);

	if (p == NULL)
		fatal_with_errno(EXIT_FAILURE, "%s", oom_msg);
	return p;
}

char *xstrdup(const char *string)
{
	char *p = strdup(string);

	if (p == NULL)
		fatal_with_errno(EXIT_FAILURE, "%s", oom_msg);
	return p;
}

/* modify in - strip all trailing instances of <sep> */
char *rtrim(char *in, const char sep)
{
	char	*p;

	if (in) {
		p = &in[strlen(in) - 1];

		while ((p >= in) && (*p == sep))
			*p-- = '\0';
	}
	return in;
}

/* modify in - strip all leading instances of <sep> */
char* ltrim(char *in, const char sep)
{
	char *p;

	if (in) {
		p = in;

		while ((*p != '\0') && (*p == sep))
			*p++ = *in++;

		p = '\0';
	}
	return in;
}

/* Read up to buflen bytes from fd and return the number of bytes
   read. If no data is available within d_sec + d_usec, return 0.
   On error, a value < 0 is returned (errno indicates error). */
#ifndef WIN32
int select_read(const int fd, void *buf, const size_t buflen, const long d_sec, const long d_usec)
{
	int		ret;
	fd_set		fds;
	struct timeval	tv;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	tv.tv_sec = d_sec;
	tv.tv_usec = d_usec;

	ret = select(fd + 1, &fds, NULL, NULL, &tv);

	if (ret < 1) {
		return ret;
	}

	return read(fd, buf, buflen);
}
#else
int select_read(serial_handler_t * fd, void *buf, const size_t buflen, const long d_sec, const long d_usec)
{
	/* This function is only called by serial drivers right now */
	int res;
	DWORD timeout;
	COMMTIMEOUTS TOut;

	timeout = (d_sec*1000) + ((d_usec+999)/1000);

	GetCommTimeouts(fd->handle,&TOut);
	TOut.ReadIntervalTimeout = MAXDWORD;
	TOut.ReadTotalTimeoutMultiplier = 0;
	TOut.ReadTotalTimeoutConstant = timeout;
	SetCommTimeouts(fd->handle,&TOut);

	res = w32_serial_read(fd,buf,buflen,timeout);

	return res;
}
#endif

/* Write up to buflen bytes to fd and return the number of bytes
   written. If no data is available within d_sec + d_usec, return 0.
   On error, a value < 0 is returned (errno indicates error). */
int select_write(const int fd, const void *buf, const size_t buflen, const long d_sec, const long d_usec)
{
#ifndef WIN32
	int		ret;
	fd_set		fds;
	struct timeval	tv;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	tv.tv_sec = d_sec;
	tv.tv_usec = d_usec;

	ret = select(fd + 1, NULL, &fds, NULL, &tv);

	if (ret < 1) {
		return ret;
	}

	return write(fd, buf, buflen);
#else
	return 0;
#endif
}
