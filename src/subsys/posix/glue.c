/*
 * Copyright (c) 2008 James Molloy, Jörg Pfähler, Matthew Iselin
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "syscallNumbers.h"

#include "errno.h"
#define errno (*__errno())
extern int *__errno (void);
int h_errno; // required by networking code

// Define errno before including syscall.h.
#include "syscall.h"

#include <stdarg.h>

typedef void (*_sig_func_ptr)(int);

extern void *malloc(int);
extern void free(void*);
extern void strcpy(char*,char*);

extern void printf(char*,...);
extern void sprintf(char*, char*,...);

#define MAXNAMLEN 255

#define STUBBED(str) syscall1(POSIX_STUBBED, (int)(str)); \
  errno = ENOSYS;

#define	F_DUPFD		0	/* Duplicate fildes */
#define	F_GETFD		1	/* Get fildes flags (close on exec) */
#define	F_SETFD		2	/* Set fildes flags (close on exec) */
#define	F_GETFL		3	/* Get file flags */
#define	F_SETFL		4	/* Set file flags */

struct dirent
{
  char d_name[MAXNAMLEN];
  int d_ino;
};

typedef struct ___DIR
{
  int fd;
  struct dirent ent;
} DIR;

struct timeval {
  unsigned int tv_sec;
  unsigned int tv_usec;
};

struct timezone_type {
  int tz_minuteswest;
  int tz_dsttime;
};

struct sigevent {
};

struct passwd {
	char	*pw_name;		/* user name */
	char	*pw_passwd;		/* encrypted password */
	int	pw_uid;			/* user uid */
	int	pw_gid;			/* user gid */
	char	*pw_comment;		/* comment */
	char	*pw_gecos;		/* Honeywell login info */
	char	*pw_dir;		/* home directory */
	char	*pw_shell;		/* default shell */
};

struct  stat
{
  int   st_dev;
  int   st_ino;
  int   st_mode;
  int   st_nlink;
  int   st_uid;
  int   st_gid;
  int   st_rdev;
  int   st_size;
  int   st_atime;
  int   st_mtime;
  int   st_ctime;
};

struct sockaddr
{
  unsigned int sa_family;
  char sa_data[14];
};

struct in_addr
{
  unsigned int s_addr;
};

struct utimbuf {};
struct termios {};
struct fd_set {};
struct msghdr {};
struct timeb {};

#define COMPILING_SUBSYS
#define SYS_SOCK_CONSTANTS_ONLY
#include "include/netdb.h"
#include "include/netinet/in.h"
#include "include/poll.h"

struct sigaction
{
  int sa_flags;
  unsigned long sa_mask;
  _sig_func_ptr sa_handler;
};

char *tzname[2] = { (char *)"GMT", (char *)"GMT" };
int daylight = 0;
long timezone = 0;
int altzone = 0;

int ftruncate(int a, int b)
{
  STUBBED("ftruncate");
  return -1;
}

char* getcwd(char *buf, unsigned long size)
{
  return (char *)syscall2(POSIX_GETCWD, (int) buf, (int) size);
}

int mkdir(const char *p, int mode)
{
  return (int)syscall2(POSIX_MKDIR, (int)p, mode);
}

int close(int file)
{
  return (int)syscall1(POSIX_CLOSE, file);
}

int _execve(char *name, char **argv, char **env)
{
  return (int)syscall3(POSIX_EXECVE, (int)name, (int)argv, (int)env);
}

void _exit(int val)
{
  syscall1(POSIX_EXIT, val);
}

int fork()
{
  return (int)syscall0(POSIX_FORK);
}

int vfork()
{
  return fork();
}

int fstat(int file, struct stat *st)
{
  return (int)syscall2(POSIX_FSTAT, (int)file, (int)st);
}

int getpid()
{
  return (int)syscall0(POSIX_GETPID);
}

int _isatty(int file)
{
  STUBBED("_isatty");
  return (file < 3) ? 1 : 0;
}

int link(char *old, char *_new)
{
  return (int)syscall2(POSIX_LINK, (int) old, (int) _new);
}

int lseek(int file, int ptr, int dir)
{
  return (int)syscall3(POSIX_LSEEK, file, ptr, dir);
}

int open(const char *name, int flags, int mode)
{
  return (int)syscall3(POSIX_OPEN, (int)name, flags, mode);
}

int read(int file, char *ptr, int len)
{
  return (int)syscall3(POSIX_READ, file, (int)ptr, len);
}

int sbrk(int incr)
{
  return (int)syscall1(POSIX_SBRK, incr);
}

int stat(const char *file, struct stat *st)
{
  return (int)syscall2(POSIX_STAT, (int)file, (int)st);
}

#ifndef PPC_COMMON
int times(void *buf)
{
  STUBBED("times");
  errno = ENOSYS;
  return -1;
}
#else
/* PPC has times() defined in terms of getrusage. */
int getrusage(int target, void *buf)
{
  STUBBED("getrusage");
  return -1;
}
#endif

int unlink(char *name)
{
  return (int)syscall1(POSIX_UNLINK, (int)name);
}

int wait(int *status)
{
  return waitpid(-1, status, 0);
}

int waitpid(int pid, int *status, int options)
{
  return (int)syscall3(POSIX_WAITPID, pid, (int)status, options);
}

int write(int file, char *ptr, int len)
{
  return (int)syscall3(POSIX_WRITE, file, (int)ptr, len);
}

int lstat(char *file, struct stat *st)
{
  return (int)syscall2(POSIX_LSTAT, (int)file, (int)st);
}

DIR *opendir(const char *dir)
{
  DIR *p = malloc(sizeof(DIR));
  p->fd = syscall2(POSIX_OPENDIR, (int)dir, (int)&p->ent);
  if(p->fd < 0)
  {
    free(p);
    return 0;
  }
  return p;
}

struct dirent *readdir(DIR *dir)
{
  if(!dir)
    return 0;

  if (syscall2(POSIX_READDIR, dir->fd, (int)&dir->ent) != -1)
    return &dir->ent;
  else
    return 0;
}

void rewinddir(DIR *dir)
{
  if(!dir)
    return;

  syscall2(POSIX_REWINDDIR, dir->fd, (int)&dir->ent);
}

int closedir(DIR *dir)
{
  if(!dir)
    return 0;

  syscall1(POSIX_CLOSEDIR, dir->fd);
  free(dir);
  return 0;
}

int rename(const char *old, const char *new)
{
  return (int)syscall2(POSIX_RENAME, (int) old, (int) new);
}

int tcgetattr(int fd, void *p)
{
  return (int)syscall2(POSIX_TCGETATTR, fd, (int)p);
}

int tcsetattr(int fd, int optional_actions, void *p)
{
  return (int)syscall3(POSIX_TCSETATTR, fd, optional_actions, (int)p);
}

int mkfifo(const char *_path, int __mode)
{
  STUBBED("mkfifo");
  return -1;
}

int gethostname(char *name, int len)
{
  STUBBED("gethostname");
  strcpy(name, "pedigree");
  return 0;
}

int sethostname(char *name, int len)
{
  STUBBED("sethostname");
  return 0;
}

int ioctl(int fd, int command, void *buf)
{
  return (int)syscall3(POSIX_IOCTL, fd, command, (int)buf);
}

int tcflow(int fd, int action)
{
  STUBBED("tcflow");
  return 0;
}

int tcflush(int fd, int queue_selector)
{
  STUBBED("tcflush");
  return 0;
}

int tcdrain(int fd)
{
  STUBBED("tcdrain");
  return -1;
}

int gettimeofday(struct timeval *tv, struct timezone_type *tz)
{
  syscall2(POSIX_GETTIMEOFDAY, (int)tv, (int)tz);

  return 0;
}

int getuid()
{
  return (int)syscall0(POSIX_GETUID);
}

int getgid()
{
  return (int)syscall0(POSIX_GETGID);
}

int geteuid()
{
  STUBBED("geteuid");
  return getuid();
}

int getegid()
{
  STUBBED("getegid");
  return getgid();
}

int getppid()
{
  STUBBED("getppid");
  return 0;
}

const char * const sys_siglist[] = {
  0,
  "Hangup",
  "Interrupt",
  "Quit",
  "Illegal instruction",
  "Trap",
  "IOT",
  "Abort",
  "EMT",
  "Floating point exception",
  "Kill",
  "Bus error",
  "Segmentation violation",
  "Bad argument to system call",
  "Pipe error",
  "Alarm",
  "Terminate" };

char *strsignal(int sig)
{
  if (sig < 16)
    return (char*)sys_siglist[sig];
  else
  return (char*)"Unknown";
}

int setuid(int uid)
{
  STUBBED("setuid");
  errno = EINVAL;
  return -1;
}

int setgid(int gid)
{
  STUBBED("setgid");
  errno = EINVAL;
  return -1;
}

unsigned int sleep(unsigned int seconds)
{
  return (unsigned int)syscall1(POSIX_SLEEP, seconds);
}

unsigned int alarm(unsigned int seconds)
{
  return (unsigned int)syscall1(POSIX_ALARM, seconds);
}

int umask(int mask)
{
  STUBBED("umask");
  return 0;
}

int chmod(const char *path, int mode)
{
  STUBBED("chmod");
  errno = ENOENT;
  return -1;
}

int chown(const char *path, int owner, int group)
{
  STUBBED("chown");
  errno = ENOENT;
  return -1;
}

int utime(const char *path,const struct utimbuf *times)
{
  STUBBED("utime");
  errno = ENOENT;
  return -1;
}

int access(const char *path, int amode)
{
  STUBBED("access");
  errno = ENOENT;
  return 0;
}

const char * const sys_errlist[] = {};
const int sys_nerr = 0;
long timezone;

long pathconf(const char *path, int name)
{
  STUBBED("pathconf");
  return 0;
}

long fpathconf(int filedes, int name)
{
  STUBBED("fpathconf");
  return 0;
}

int cfgetospeed(const struct termios *t)
{
  STUBBED("cfgetospeed");
  return 0;
}

int cfgetispeed(const struct termios *t)
{
  STUBBED("cfgetispeed");
  return 0;
}

int cfsetospeed(const struct termios *t, int speed)
{
  STUBBED("cfsetospeed");
  return 0;
}

int cfsetispeed(const struct termios *t, int speed)
{
  STUBBED("cfsetispeed");
  return 0;
}


int select(int nfds, struct fd_set * readfds,
                     struct fd_set * writefds, struct fd_set * errorfds,
                     struct timeval * timeout)
{
  return (int)syscall5(POSIX_SELECT, nfds, (int)readfds, (int)writefds, (int)errorfds, (int)timeout);
}

void setgrent()
{
  STUBBED("setgrent");
}

void endgrent()
{
  STUBBED("endgrent");
}

struct group *getgrent()
{
  STUBBED("getgrent");
  errno = ENOSYS;
  return 0;
}

static struct passwd g_passwd;
int g_passwd_num = 0;
char g_passwd_str[256];
void setpwent()
{
  g_passwd_num = 0;
}

void endpwent()
{
  g_passwd_num = 0;
}

struct passwd *getpwent()
{
  if (syscall3(POSIX_GETPWENT, (int)&g_passwd, g_passwd_num, (int)&g_passwd_str) != 0)
    return 0;
  g_passwd_num++;
  return &g_passwd;
}

struct passwd *getpwuid(int uid)
{
  if (syscall3(POSIX_GETPWENT, (int)&g_passwd, uid, (int)&g_passwd_str) != 0)
    return 0;
  return &g_passwd;
}

struct passwd *getpwnam(char *name)
{
  if (syscall3(POSIX_GETPWNAM, (int)&g_passwd, (int)name, (int)&g_passwd_str) != 0)
    return 0;
  return &g_passwd;
}

// Pedigree-specific function: login with given uid and password.
int login(int uid, char *password)
{
  return (int)syscall2(PEDIGREE_LOGIN, uid, (int)password);
}

int chdir(const char *path)
{
  return (int)syscall1(POSIX_CHDIR, (int)path);
}

int dup(int fileno)
{
  return (int)syscall1(POSIX_DUP, fileno);
}

int dup2(int fildes, int fildes2)
{
  return (int)syscall2(POSIX_DUP2, fildes, fildes2);
}

int pipe(int filedes[2])
{
  return (int)syscall1(POSIX_PIPE, (int) filedes);
}

int fcntl(int fildes, int cmd, ...)
{
  va_list ap;
  va_start(ap, cmd);

  int num = 0;
  int* args = 0;
  switch(cmd)
  {
    // only one argument for each of these
    case F_DUPFD:
    case F_SETFD:
    case F_SETFL:
      args = (int*) malloc(sizeof(int));
      args[0] = va_arg(ap, int);
      num = 1;
  };
  va_end(ap);

  int ret = syscall4(POSIX_FCNTL, fildes, cmd, num, (int) args);

  if(args)
    free(args);
  return ret;
}

int sigprocmask(int how, unsigned long* set, unsigned long* oset)
{
  return (int)syscall3(POSIX_SIGPROCMASK, how, (int) set, (int) oset);
}

int fchown(int fildes, int owner, int group)
{
  STUBBED("fchown");
  return -1;
}

int rmdir(const char *path)
{
  STUBBED("rmdir");
  return -1;
}

int socket(int domain, int type, int protocol)
{
  return (int)syscall3(POSIX_SOCKET, domain, type, protocol);
}

int connect(int sock, const struct sockaddr* address, unsigned long addrlen)
{
  return (int)syscall3(POSIX_CONNECT, sock, (int) address, (int) addrlen);
}

int send(int sock, const void * buff, unsigned long bufflen, int flags)
{
  return (int)syscall4(POSIX_SEND, sock, (int) buff, (int) bufflen, flags);
}

int recv(int sock, void * buff, unsigned long bufflen, int flags)
{
  return (int)syscall4(POSIX_RECV, sock, (int) buff, (int) bufflen, flags);
}

int accept(int sock, struct sockaddr* remote_addr, unsigned long* addrlen)
{
  return (int)syscall3(POSIX_ACCEPT, sock, (int) remote_addr, (int) addrlen);
}

int bind(int sock, const struct sockaddr* local_addr, unsigned long addrlen)
{
  return (int)syscall3(POSIX_BIND, sock, (int) local_addr, (int) addrlen);
}

int getpeername(int sock, struct sockaddr* addr, unsigned long* addrlen)
{
  STUBBED("getpeername");
  return -1;
}

int getsockname(int sock, struct sockaddr* addr, unsigned long* addrlen)
{
  STUBBED("getsockname");
  return -1;
}

int getsockopt(int sock, int level, int optname, void* optvalue, unsigned long* optlen)
{
  STUBBED("getsockopt");
  return -1;
}

int listen(int sock, int backlog)
{
  return (int)syscall2(POSIX_LISTEN, sock, backlog);
}

struct special_send_recv_data
{
  int sock;
  void* buff;
  unsigned long bufflen;
  int flags;
  struct sockaddr* remote_addr;
  unsigned long* addrlen;
} __attribute__((packed));

long recvfrom(int sock, void* buff, unsigned long bufflen, int flags, struct sockaddr* remote_addr, unsigned long* addrlen)
{
  struct special_send_recv_data* tmp = (struct special_send_recv_data*) malloc(sizeof(struct special_send_recv_data));
  tmp->sock = sock;
  tmp->buff = buff;
  tmp->bufflen = bufflen;
  tmp->flags = flags;
  tmp->remote_addr = remote_addr;
  tmp->addrlen = addrlen;

  int ret = syscall1(POSIX_RECVFROM, (int) tmp);

  free(tmp);

  return ret;
}

long recvmsg(int sock, struct msghdr* msg, int flags)
{
  STUBBED("recvmsg");
  return -1;
}

long sendmsg(int sock, const struct msghdr* msg, int flags)
{
  STUBBED("sendmsg");
  return -1;
}

long sendto(int sock, const void* buff, unsigned long bufflen, int flags, const struct sockaddr* remote_addr, unsigned long* addrlen)
{
  struct special_send_recv_data* tmp = (struct special_send_recv_data*) malloc(sizeof(struct special_send_recv_data));
  tmp->sock = sock;
  tmp->buff = (char *)buff;
  tmp->bufflen = bufflen;
  tmp->flags = flags;
  tmp->remote_addr = (struct sockaddr*)remote_addr;
  tmp->addrlen = addrlen;

  int ret = syscall1(POSIX_SENDTO, (int) tmp);

  free(tmp);

  return ret;
}

int setsockopt(int sock, int level, int optname, const void* optvalue, unsigned long optlen)
{
  STUBBED("setsockopt");
  return -1;
}

int shutdown(int sock, int how)
{
  STUBBED("shutdown");
  return -1;
}

int sockatmark(int sock)
{
  STUBBED("sockatmark");
  return -1;
}

int socketpair(int domain, int type, int protocol, int sock_vec[2])
{
  STUBBED("socketpair");
  return -1;
}

unsigned int inet_addr(const char *cp)
{
  /// \todo Support formats other than a.b.c.d
  /// \todo Rewrite!

  char* tmp = (char*) malloc(strlen(cp));
  char* tmp_ptr = tmp; // so we can free the memory
  strcpy(tmp, (char *)cp);

  // iterate through, removing decimals and taking the four pointers
  char* elements[4] = {tmp, 0, 0, 0};
  int num = 1;
  while(*tmp)
  {
    if(*tmp == '.')
    {
      *tmp = 0;
      elements[num++] = tmp + 1;
    }

    tmp++;
  }

  if(num != 4)
    return 0;

  unsigned int a = atoi(elements[0]);
  unsigned int b = atoi(elements[1]);
  unsigned int c = atoi(elements[2]);
  unsigned int d = atoi(elements[3]);

  unsigned int ret = (d << 24) | (c << 16) | (b << 8) | a;

  free(tmp_ptr);

  return ret;
}

char* inet_ntoa(struct in_addr addr)
{
  static char buff[16];
  sprintf(buff, "%u.%u.%u.%u", addr.s_addr & 0xff, (addr.s_addr & 0xff00) >> 8, (addr.s_addr & 0xff0000) >> 16, (addr.s_addr & 0xff000000) >> 24);
  return buff;
}

int inet_aton(const char *cp, struct in_addr *inp)
{
  return inet_addr(cp);
}

struct hostent* gethostbyaddr(const void *addr, unsigned long len, int type)
{
  static struct hostent ret;
  if(syscall4(POSIX_GETHOSTBYADDR, (int) addr, len, type, (int) &ret) != 0)
    return &ret;
  return 0;
}

struct hostent* gethostbyname(const char *name)
{
  static struct hostent* ret = 0;
  if(ret == 0)
    ret = (struct hostent*) malloc(512);
  if(ret == 0)
    return (struct hostent*) 0;

  int success = syscall3(POSIX_GETHOSTBYNAME, (int) name, (int) ret, 512);
  if(success == 0)
  {
    ret->h_addr = ret->h_addr_list[0];
    return ret;
  }
  else
    return (struct hostent*) 0;
}

struct servent* getservbyname(const char *name, const char *proto)
{
  STUBBED("getservbyname");
  return 0;
}

void endservent()
{
  STUBBED("endservent");
}

struct servent* getservbyport(int port, const char *proto)
{
  STUBBED("setservbyport");
  return 0;
}

struct servent* getservent()
{
  STUBBED("setservent");
  return 0;
}

void setservent(int stayopen)
{
  STUBBED("setservent");
}

void endprotoent()
{
  STUBBED("endprotoent");
}

struct protoent* getprotobyname(const char *name)
{
  static struct protoent* ent = 0;
  if(ent == 0)
  {
    ent = (struct protoent*) malloc(512);
    ent->p_name = 0;
  }
  if(ent->p_name)
    free(ent->p_name);

  ent->p_name = (char*) malloc(strlen(name) + 1);
  ent->p_aliases = 0;
  strcpy(ent->p_name, (char*)name);

  if(!strcmp(name, "icmp"))
    ent->p_proto = IPPROTO_ICMP;
  else if(!strcmp(name, "udp"))
    ent->p_proto = IPPROTO_UDP;
  else if(!strcmp(name, "tcp"))
    ent->p_proto = IPPROTO_TCP;
  else
    ent->p_proto = 0;

  return ent;
}

struct protoent* getprotobynumber(int proto)
{
  STUBBED("getprotobynumber");
  return 0;
}

struct protoent* getprotoent()
{
  STUBBED("getprotoent");
  return 0;
}

void setprotoent(int stayopen)
{
  STUBBED("setprotoent");
}

int getgrnam()
{
  STUBBED("getgrnam");
  return 0;
}

int getgrgid()
{
  STUBBED("getgrgid");
  return 0;
}

int symlink()
{
  STUBBED("symlink");
  return 0;
}

int mknod()
{
  STUBBED("mknod");
  return 0;
}

int fsync()
{
  STUBBED("fsync");
  return 0;
}

int inet_pton()
{
  STUBBED("inet_pton");
  return -1;
}

const char* inet_ntop(int af, const void* src, char* dst, unsigned long size)
{
  STUBBED("inet_ntop");
  return 0;
}

/*
void* popen(const char *command, const char *mode)
{
  STUBBED("popen");
  return 0;
}

int pclose(void* stream)
{
  STUBBED("pclose");
  return 0;
}
*/

int readlink(const char* path, char* buf, unsigned int bufsize)
{
  return (int) syscall3(POSIX_READLINK, (int) path, (int) buf, bufsize);
}

int ftime(struct timeb *tp)
{
  STUBBED("ftime");
  return -1;
}

int sigmask()
{
  STUBBED("sigmask");
  return -1;
}

int sigblock()
{
  STUBBED("sigblock");
  return -1;
}

int sigsetmask()
{
  STUBBED("sigsetmask");
  return -1;
}

/// \todo These could be better, more correct, etc...

#define SIGNAL_HANDLER_EXIT(name, errcode) void name(int s) { _exit(errcode); }
#define SIGNAL_HANDLER_EMPTY(name) void name(int s) {}
#define SIGNAL_HANDLER_EXITMSG(name, errcode, msg) void name(int s) { printf(msg); _exit(errcode); }

#if 1

SIGNAL_HANDLER_EXIT     (sigabrt, 1);
SIGNAL_HANDLER_EXIT     (sigalrm, 1);
SIGNAL_HANDLER_EXIT     (sigbus, 1);
SIGNAL_HANDLER_EMPTY    (sigchld);
SIGNAL_HANDLER_EMPTY    (sigcont); /// \todo Continue & Pause execution
SIGNAL_HANDLER_EXIT     (sigfpe, 1); // floating point exception signal
SIGNAL_HANDLER_EXIT     (sighup, 1);
SIGNAL_HANDLER_EXITMSG  (sigill, 1, "Illegal instruction\n");
SIGNAL_HANDLER_EXIT     (sigint, 1);
SIGNAL_HANDLER_EXIT     (sigkill, 1);
SIGNAL_HANDLER_EXIT     (sigpipe, 1);
SIGNAL_HANDLER_EXIT     (sigquit, 1);
SIGNAL_HANDLER_EXITMSG  (sigsegv, 1, "Segmentation fault!\n");
SIGNAL_HANDLER_EMPTY    (sigstop); /// \todo Continue & Pause execution
SIGNAL_HANDLER_EXIT     (sigterm, 1);
SIGNAL_HANDLER_EMPTY    (sigtstp); // terminal stop
SIGNAL_HANDLER_EMPTY    (sigttin); // background process attempts read
SIGNAL_HANDLER_EMPTY    (sigttou); // background process attempts write
SIGNAL_HANDLER_EMPTY    (sigusr1);
SIGNAL_HANDLER_EMPTY    (sigusr2);
SIGNAL_HANDLER_EMPTY    (sigurg); // high bandwdith data available at a sockeet

SIGNAL_HANDLER_EMPTY    (sigign);

_sig_func_ptr sigs[] = {
                          sigign, // null signal
                          sighup,
                          sigint,
                          sigquit,
                          sigill,
                          sigign, // no SIGTRAP
                          sigign, // no SIGIOT
                          sigabrt,
                          sigign, // no SIGEMT
                          sigfpe,
                          sigkill,
                          sigbus,
                          sigsegv,
                          sigign, // no SIGSYS
                          sigpipe,
                          sigalrm,
                          sigterm,
                          sigurg,
                          sigstop,
                          sigtstp,
                          sigcont,
                          sigchld,
                          sigign, // no SIGCLD
                          sigttin,
                          sigttou,
                          sigign, // no SIGIO
                          sigign, // no SIGXCPU
                          sigign, // no SIGXFSZ
                          sigign, // no SIGVTALRM
                          sigign, // no SIGPROF
                          sigign, // no SIGWINCH,
                          sigign, // no SIGLOST
                          sigusr1,
                          sigusr2
                         };

static int bTablesInit = 0;

int sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
{
  return (int)syscall4(POSIX_SIGACTION, sig, (int) act, (int) oact, 0);

  // setup the default signal handlers, if needed
  if(bTablesInit == 0)
  {
    bTablesInit = 1;
    struct sigaction in, out;
    in.sa_handler = (_sig_func_ptr) 0;

    int z;
    for(z = 0; z < 32; z++)
      sigaction(z, &in, &out);
  }

  if(sig > 32)
  {
    errno = EINVAL;
    return (int)((_sig_func_ptr) -1);
  }
  else if(sig == 32)
    sig = 0; // null signal

  // SIGKILL and SIGSTOP are not catchable
  else if(sig == 9 || sig == 17)
  {
    errno = EINVAL;
    return (int)((_sig_func_ptr) -1);
  }

  int type = 0;

  struct sigaction* tmpAct = 0;
  if(act)
  {
    tmpAct = (struct sigaction*) malloc(sizeof(struct sigaction));
    memcpy(tmpAct, act, sizeof(struct sigaction));

    uint32_t funcAddr = (uint32_t) (act->sa_handler);
    if(funcAddr == 0)
    {
      // SIG_DFL: set the default handler
      tmpAct->sa_handler = sigs[sig];
      type = 1;
    }
    else if(funcAddr == 1)
    {
      // SIG_IGN: set the ignore handler
      tmpAct->sa_handler = sigign;
      type = 2;
    }
    else if(funcAddr == -1)
    {
      errno = EINVAL;
      return (int)((_sig_func_ptr) -1);
    }
  }

  int ret = syscall4(POSIX_SIGACTION, sig, (int) tmpAct, (int) oact, type);

  if(act)
    free(tmpAct);

  return ret;
}

_sig_func_ptr signal(int s, _sig_func_ptr func)
{
  // obtain the old mask for the sigaction structure, fill it in with default arguments
  // and pass on to sigaction
  struct sigaction act;
  struct sigaction tmp;
  unsigned long mask = 0;
  sigprocmask(0, 0, &mask);
  act.sa_mask = mask;
  act.sa_handler = func;
  act.sa_flags = 0;
  memset(&tmp, 0, sizeof(struct sigaction));
  if(sigaction(s, &act, &tmp) == 0)
  {
    return tmp.sa_handler;
  }

  // errno set by sigaction
  return (_sig_func_ptr) -1;
}

int raise(int sig)
{
  // setup the default signal handlers, if needed
  if(bTablesInit == 0)
  {
    bTablesInit = 1;
    struct sigaction in, out;
    in.sa_handler = (_sig_func_ptr) 0;

    int z;
    for(z = 0; z < 32; z++)
      sigaction(z, &in, &out);
  }

  return (int)syscall1(POSIX_RAISE, sig);
}

#else

int sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
{
  STUBBED("sigaction");
  return -1;
}

#endif

int kill(int pid, int sig)
{
  // setup the default signal handlers, if needed
  if(bTablesInit == 0)
  {
    bTablesInit = 1;
    struct sigaction in, out;
    in.sa_handler = (_sig_func_ptr) 0;

    int z;
    for(z = 0; z < 32; z++)
      sigaction(z, &in, &out);
  }

  return (int)syscall2(POSIX_KILL, pid, sig);
}

int sigpending(long* set)
{
  STUBBED("sigpending");
  return -1;
}

int sigsuspend(const long* sigmask)
{
  STUBBED("sigsuspend");
  return -1;
}

/** Installs default signal handlers to the kernel */
void _init_signals()
{
  return;
    if(bTablesInit == 0)
    {
        bTablesInit = 1;
        struct sigaction in, out;
        in.sa_handler = (_sig_func_ptr) 0;

        int z;
        for(z = 0; z < 32; z++)
            sigaction(z, &in, &out);
    }
}

int fdatasync(int fildes)
{
  STUBBED("fdatasync");
  return -1;
}

struct dlHandle
{
  int mode;
};

void *dlopen(const char *file, int mode)
{
  void* p = (void*) malloc(sizeof(struct dlHandle));
  if(!p)
    return 0;
  void* ret = (void*) syscall3(POSIX_DLOPEN, (int) file, mode, (int) p);
  if(ret)
    return ret;
  free(p);
  return 0;
}

void *dlsym(void* handle, const char* name)
{
  return (void*) syscall2(POSIX_DLSYM, (int) handle, (int) name);
}

int dlclose(void *handle)
{
  STUBBED("dlclose");
  if(handle)
    free(handle);
  return 0;
}

char *dlerror()
{
  STUBBED("dlerror");
  return 0;
}

int poll(struct pollfd fds[], unsigned int nfds, int timeout)
{
  return (int)syscall3(POSIX_POLL, (int)fds, nfds, timeout);
}

void herror(const char *s)
{
  char *buff = (char*)strerror(h_errno);
  printf("%s: %s\n", s, buff);
}

