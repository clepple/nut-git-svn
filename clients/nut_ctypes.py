#!/usr/bin/env python

"""nut_ctypes.py - a Python interface to the C libupsclient library.

Copyright (C) 2008  Charles Lepple <clepple+nut@ghz.cc>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

"""

# ctypes documentation: http://docs.python.org/lib/ctypes-structures-unions.html

# Ideally, this module would be wrapped by a higher-level object-oriented
# module. That higher-level module could also be reimplemented in pure Python,
# since the NUT protocol uses simple, line-based TCP transactions.

from ctypes import *

# Based on the following definition in upsclient.h:

"""
typedef struct {
        char    *host;
        int     port;
        int     fd;
        int     flags;
        int     upserror;
        int     syserrno;
        int     upsclient_magic;

        PCONF_CTX_t     pc_ctx;

        char    errbuf[UPSCLI_ERRBUF_LEN];

#ifdef HAVE_SSL
        SSL_CTX *ssl_ctx;
        SSL     *ssl;
#else
        void    *ssl_ctx;
        void    *ssl;
#endif

        char    readbuf[64];
        size_t  readlen;
        size_t  readidx;

}       UPSCONN_t;

#define UPSCLI_ERRBUF_LEN       256
#define UPSCLI_NETBUF_LEN       512     /* network i/o buffer */

[...]

/* flags for use with upscli_connect */

#define UPSCLI_CONN_TRYSSL      0x0001  /* try SSL, OK if not supported       */
#define UPSCLI_CONN_REQSSL      0x0002  /* try SSL, fail if not supported     */
#ifdef  HAVE_IPV6
#define UPSCLI_CONN_INET        0x0004  /* IPv4 only */
#define UPSCLI_CONN_INET6       0x0008  /* IPv6 only */
#endif

"""

# Constants:
UPSCLI_ERRBUF_LEN   = 256
UPSCLI_NETBUF_LEN   = 512

UPSCLI_CONN_TRYSSL  = 0x0001
UPSCLI_CONN_REQSSL  = 0x0002
UPSCLI_CONN_INET    = 0x0004
UPSCLI_CONN_INET6   = 0x0008

class UPSCONN_t(Structure):
    _fields_ = [
        ("host",    c_char_p),
        ("port",    c_int),
        ("fd",      c_int),
        ("flags",   c_int),
        ("upserror",c_int),
        ("syserrno",c_int),
        ("upsclient_magic", c_int),
        ("pc_ctx",  c_void_p), # TODO: define this structure?
        ("errbuf",  c_char * UPSCLI_ERRBUF_LEN),
        ("ssl_ctx", c_void_p),
        ("ssl",     c_void_p),
        ("readbuf", c_char * 64),
        ("readlen", c_size_t),
        ("readidx", c_size_t)
    ]
    
    def __repr__(self):
        s = Structure.__repr__(self)
        if self.host:
            s = s[:-1] + ', host: %s>' % self.host
        if self.port:
            s = s[:-1] + ', port: %d>' % self.port
        
        return s
    
UPSCONN_t_p = POINTER(UPSCONN_t)

import sys
if sys.platform == "darwin":
    libupsclient = cdll.LoadLibrary(".libs/libupsclient.dylib")
elif sys.platform.startswith("linux"):
    libupsclient = cdll.LoadLibrary(".libs/libupsclient.so")

libupsclient.upscli_strerror.argtypes = [UPSCONN_t_p]
libupsclient.upscli_strerror.restype = c_char_p
libupsclient.upscli_strerror.__doc__ = """upscli_strerror(ctypes.pointer(UPSCONN_t)) -> str"""

libupsclient.upscli_connect.argtypes = [UPSCONN_t_p, c_char_p, c_int, c_int]
libupsclient.upscli_connect.__doc__ = """upscli_connect(ctypes.pointer(UPSCONN_t), 'host', port, flags) -> int"""

libupsclient.upscli_disconnect.argtypes = [UPSCONN_t_p]
libupsclient.upscli_disconnect.__doc__ = """upscli_disconnect(ctypes.pointer(UPSCONN_t)) -> int"""

PORT = 3493

def main(argv=()):
    """A quick unit test to connect and disconnect from upsd."""
    upsconn = UPSCONN_t()
    try:
        host = argv[1]
    except IndexError:
        host = 'localhost'
        
    try:
        port = int(argv[2])
    except IndexError:
        port = PORT
    
    res = libupsclient.upscli_connect(pointer(upsconn), host, port, 0)
    if res:
        print 'Error connecting, %s' % libupsclient.upscli_strerror(upsconn)
        return
    else:
        print 'Connected to %s:%d' % (host, port)
    
    res = libupsclient.upscli_disconnect(pointer(upsconn))
    if res:
        print 'Error disconnecting, %s' % libupsclient.upscli_strerror(upsconn)
    else:
        print 'Disconnected.'
    
    
if __name__ == '__main__':
    main(sys.argv)
