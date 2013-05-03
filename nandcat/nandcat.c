/*
 * deprogram: this isn't really netcat at all anymore. it's really just a
 * very specialized wrapper for nandroid's new dump functionality. grabbed
 * some shit from hither and yon and slapped it together, gutted everything
 * else. don't say i didn't warn ya.
 *
 * TODO: gut more.
 */


/* Copyright (c) 2013 henry j. mason <henry@cyngn.com>
 *
 * original openbeastie license:
 *
 * Copyright (c) 2001 Eric Jackson <ericj@monkey.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Re-written nc(1) for OpenBSD. Original implementation by
 * *Hobbit* <hobbit@avian.org>.
 */



#include <sys/un.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include "atomicio.h"

#ifndef SUN_LEN
#define SUN_LEN(su) \
    (sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif

#define PORT_MAX	65535
#define PORT_MAX_LEN	6
#define UNIX_DG_TMP_SOCKET_SIZE	19

/* Command Line Options */
char    *NDflag;    /* Execute nandroid dump  */
char    *NUflag;    /* Execute nandroid undump */

int timeout = -1;
int family = AF_UNSPEC;
char *portlist[PORT_MAX+1];
char *unix_dg_tmp_socket;


int local_listen(char *, char *, struct addrinfo);
void readwrite(int);
int unix_bind(char *);
int unix_listen(char *);
void ndump(int);
void nundump(int);

void help(void);
void usage(int);

int
main(int argc, char *argv[])
{
    int ch, s, ret, socksv;
    char *host, *uport;
    struct addrinfo hints;
    struct servent *sv;
    socklen_t len;
    struct sockaddr_storage cliaddr;
    char *proxy;
    const char *errstr, *proxyhost = "", *proxyport = NULL;
    struct addrinfo proxyhints;
    char unix_dg_tmp_socket_buf[UNIX_DG_TMP_SOCKET_SIZE];

    ret = 1;
    s = 0;
    socksv = 5;
    host = NULL;
    uport = NULL;
    sv = NULL;

    while ((ch = getopt(argc, argv, "d:u:")) != -1) {
        switch (ch) {
        case 'd':
            NDflag = strdup(optarg);
            fprintf(stderr, "setting NDflag=%s\n", NDflag);
            break;
        case 'u':
            NUflag = strdup(optarg);
            fprintf(stderr, "setting NUflag=%s\n", NUflag);
            break;
        default:
            usage(1);
        }
    }

    argc -= optind;
    argv += optind;

    /* get our port argument, if set */
    if (argc == 0) {
        uport = "6666"; // default
    } else if (argv[0] && argc == 1) {
        uport = argv[0];
    } else
    usage(1);

    if (NDflag && NUflag)
        errx(1, "cannot use -d and -u together");
    if (!NDflag && !NUflag)
        errx(1, "must specify an option and partition");

    fprintf(stderr, "listening on port %s \n", uport);

    /* Initialize addrinfo structure. */
    if (family != AF_UNIX) {
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = family;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
    }

    int connfd;
    ret = 0;

    if (family == AF_UNIX)
        s = unix_listen(host);
    if (family != AF_UNIX)
        s = local_listen(host, uport, hints);

    if (s < 0)
        err(1, NULL);

    len = sizeof(cliaddr);
    connfd = accept(s, (struct sockaddr *)&cliaddr,
        &len);

    if (NDflag)
        ndump (connfd);
    if (NUflag)
        nundump (connfd);

    readwrite(connfd);
    close(connfd);

    if (family != AF_UNIX)
        close(s);

    if (s)
        close(s);

    exit(ret);
}

/*
 * unix_bind()
 * Returns a unix socket bound to the given path
 */
int
unix_bind(char *path)
{
    struct sockaddr_un sun;
    int s;

    /* Create unix domain socket. */
    if ((s = socket(AF_UNIX, SOCK_STREAM,
         0)) < 0)
        return (-1);

    memset(&sun, 0, sizeof(struct sockaddr_un));
    sun.sun_family = AF_UNIX;

    if (strlcpy(sun.sun_path, path, sizeof(sun.sun_path)) >=
        sizeof(sun.sun_path)) {
        close(s);
        errno = ENAMETOOLONG;
        return (-1);
    }

    if (bind(s, (struct sockaddr *)&sun, SUN_LEN(&sun)) < 0) {
        close(s);
        return (-1);
    }
    return (s);
}


/*
 * unix_listen()
 * Create a unix domain socket, and listen on it.
 */
int
unix_listen(char *path)
{
    int s;
    if ((s = unix_bind(path)) < 0)
        return (-1);

    if (listen(s, 5) < 0) {
        close(s);
        return (-1);
    }
    return (s);
}

/*
 * local_listen()
 * Returns a socket listening on a local port, binds to specified source
 * address. Returns -1 on failure.
 */
int
local_listen(char *host, char *port, struct addrinfo hints)
{
    struct addrinfo *res, *res0;
    int s, ret, x = 1;
    int error;

    /* Allow nodename to be null. */
    hints.ai_flags |= AI_PASSIVE;

    /*
     * In the case of binding to a wildcard address
     * default to binding to an ipv4 address.
     */
    if (host == NULL && hints.ai_family == AF_UNSPEC)
        hints.ai_family = AF_INET;

    if ((error = getaddrinfo(host, port, &hints, &res)))
        errx(1, "getaddrinfo: %s", gai_strerror(error));

    res0 = res;
    do {
        if ((s = socket(res0->ai_family, res0->ai_socktype,
            res0->ai_protocol)) < 0)
            continue;

#ifndef ANDROID
        if (rtableid) {
            if (setsockopt(s, IPPROTO_IP, SO_RTABLE, &rtableid,
                sizeof(rtableid)) == -1)
                err(1, "setsockopt SO_RTABLE");
        }
#endif /* !ANDROID */

#ifdef ANDROID
        ret = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &x, sizeof(x));
#else
        ret = setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &x, sizeof(x));
#endif
        if (ret == -1)
            err(1, NULL);

        if (bind(s, (struct sockaddr *)res0->ai_addr,
            res0->ai_addrlen) == 0)
            break;

        close(s);
        s = -1;
    } while ((res0 = res0->ai_next) != NULL);

    if (s != -1) {
        if (listen(s, 1) < 0)
            err(1, "listen");
    }

    freeaddrinfo(res);

    return (s);
}



void ndump(int fd)
{
    printf("ndump() NDflag=%s \n", NDflag);

    /* duplicate the socket for the child program */
    dup2 (fd, 0);
    close (fd);
    dup2 (0, 1);

     // don't redirect stderr! we DGAF
//     dup2 (0, 2);

    execl("/sbin/nandroid", "nandroid", "dump", NDflag, NULL);
    errx(1, "exec /sbin/nandroid dump %s failed", NDflag);
}

void nundump(int fd)
{
    printf("nundump() NUflag=%s \n", NUflag);

    dup2 (fd, 0);
    close (fd);
    dup2 (0, 1);

    execl("/sbin/nandroid", "nandroid", "undump", NUflag, NULL);
    errx(1, "exec /sbin/nandroid undump %s failed", NUflag);
}

/*
 * readwrite()
 * Loop that polls on the network file descriptor and stdin.
 */
void
readwrite(int nfd)
{
    struct pollfd pfd[2];
    unsigned char buf[16384];
    int n, wfd = fileno(stdin);
    int lfd = fileno(stdout);
    int plen;

    // can we use jumbo packets? or larger chunk size?
//	plen = jflag ? 16384 : 2048;
    plen = 2048;

    /* Setup Network FD */
    pfd[0].fd = nfd;
    pfd[0].events = POLLIN;

    /* Set up STDIN FD. */
    pfd[1].fd = wfd;
    pfd[1].events = POLLIN;

    while (pfd[0].fd != -1) {
        if ((n = poll(pfd, 2, timeout)) < 0) {
            close(nfd);
            err(1, "Polling Error");
        }

        if (n == 0)
            return;

        if (pfd[0].revents & POLLIN) {
            if ((n = read(nfd, buf, plen)) < 0)
                return;
            else if (n == 0) {
                shutdown(nfd, SHUT_RD);
                pfd[0].fd = -1;
                pfd[0].events = 0;
            } else {
                if (atomicio(vwrite, lfd, buf, n) != n)
                    return;
            }
        }

        if (pfd[1].revents & POLLIN) {
            if ((n = read(wfd, buf, plen)) < 0)
                return;
            else if (n == 0) {
                shutdown(nfd, SHUT_WR);
                pfd[1].fd = -1;
                pfd[1].events = 0;
            } else {
                if (atomicio(vwrite, nfd, buf, n) != n)
                    return;
            }
        }
    }
}

void
help(void)
{
    usage(0);
    fprintf(stderr, "\tCommand Summary:\n\
    \t-d		nandroid dump specified partition\n\
    \t-u		nandroid undump specified partition\n");
    exit(1);
}

void
usage(int ret)
{
    fprintf(stderr,
        "usage: nandcat [-d partition]\n"
        "\t  [-u partition] [port]\n");
    if (ret)
        exit(1);
}
