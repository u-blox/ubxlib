/*
 * Copyright 2019-2022 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _U_SOCK_ERRNO_H_
#define _U_SOCK_ERRNO_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup sock
 *  @{
 */

/** @file
 * @brief This header file defines the ERRNO values used by the sockets API.
 * The values match those of LWIP.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: ERRNO
 * -------------------------------------------------------------- */

#define  U_SOCK_ENONE            0      //<! No error.
#define  U_SOCK_EPERM            1      //<! Operation not permitted.
#define  U_SOCK_ENOENT           2      //<! No such file or directory.
#define  U_SOCK_ESRCH            3      //<! No such process.
#define  U_SOCK_EINTR            4      //<! Interrupted system call.
#define  U_SOCK_EIO              5      //<! I/O error.
#define  U_SOCK_ENXIO            6      //<! No such device or address.
#define  U_SOCK_E2BIG            7      //<! Arg list too long.
#define  U_SOCK_ENOEXEC          8      //<! Exec format error.
#define  U_SOCK_EBADF            9      //<! Bad file number.
#define  U_SOCK_ECHILD          10      //<! No child processes.
#define  U_SOCK_EAGAIN          11      //<! Try again.
#define  U_SOCK_ENOMEM          12      //<! Out of memory.
#define  U_SOCK_EACCES          13      //<! Permission denied.
#define  U_SOCK_EFAULT          14      //<! Bad address.
#define  U_SOCK_ENOTBLK         15      //<! Block device required.
#define  U_SOCK_EBUSY           16      //<! Device or resource busy.
#define  U_SOCK_EEXIST          17      //<! File exists.
#define  U_SOCK_EXDEV           18      //<! Cross-device link.
#define  U_SOCK_ENODEV          19      //<! No such device.
#define  U_SOCK_ENOTDIR         20      //<! Not a directory.
#define  U_SOCK_EISDIR          21      //<! Is a directory.
#define  U_SOCK_EINVAL          22      //<! Invalid argument.
#define  U_SOCK_ENFILE          23      //<! File table overflow.
#define  U_SOCK_EMFILE          24      //<! Too many open files.
#define  U_SOCK_ENOTTY          25      //<! Not a typewriter.
#define  U_SOCK_ETXTBSY         26      //<! Text file busy.
#define  U_SOCK_EFBIG           27      //<! File too large.
#define  U_SOCK_ENOSPC          28      //<! No space left on device.
#define  U_SOCK_ESPIPE          29      //<! Illegal seek.
#define  U_SOCK_EROFS           30      //<! Read-only file system.
#define  U_SOCK_EMLINK          31      //<! Too many links.
#define  U_SOCK_EPIPE           32      //<! Broken pipe.
#define  U_SOCK_EDOM            33      //<! Math argument out of domain of func.
#define  U_SOCK_ERANGE          34      //<! Math result not representable.
#define  U_SOCK_EDEADLK         35      //<! Resource deadlock would occur.
#define  U_SOCK_ENAMETOOLONG    36      //<! File name too long.
#define  U_SOCK_ENOLCK          37      //<! No record locks available.
#define  U_SOCK_ENOSYS          38      //<! Function not implemented.
#define  U_SOCK_ENOTEMPTY       39      //<! Directory not empty.
#define  U_SOCK_ELOOP           40      //<! Too many symbolic links encountered.
#define  U_SOCK_EWOULDBLOCK     U_SOCK_EAGAIN  //<! Operation would block.
#define  U_SOCK_ENOMSG          42      //<! No message of desired type.
#define  U_SOCK_EIDRM           43      //<! Identifier removed.
#define  U_SOCK_ECHRNG          44      //<! Channel number out of range.
#define  U_SOCK_EL2NSYNC        45      //<! Level 2 not synchronized.
#define  U_SOCK_EL3HLT          46      //<! Level 3 halted.
#define  U_SOCK_EL3RST          47      //<! Level 3 reset.
#define  U_SOCK_ELNRNG          48      //<! Link number out of range.
#define  U_SOCK_EUNATCH         49      //<! Protocol driver not attached.
#define  U_SOCK_ENOCSI          50      //<! No CSI structure available.
#define  U_SOCK_EL2HLT          51      //<! Level 2 halted.
#define  U_SOCK_EBADE           52      //<! Invalid exchange.
#define  U_SOCK_EBADR           53      //<! Invalid request descriptor.
#define  U_SOCK_EXFULL          54      //<! Exchange full.
#define  U_SOCK_ENOANO          55      //<! No anode.
#define  U_SOCK_EBADRQC         56      //<! Invalid request code.
#define  U_SOCK_EBADSLT         57      //<! Invalid slot.

#define  U_SOCK_EDEADLOCK       U_SOCK_EDEADLK

#define  U_SOCK_EBFONT          59      //<! Bad font file format.
#define  U_SOCK_ENOSTR          60      //<! Device not a stream.
#define  U_SOCK_ENODATA         61      //<! No data available.
#define  U_SOCK_ETIME           62      //<! Timer expired.
#define  U_SOCK_ENOSR           63      //<! Out of streams resources.
#define  U_SOCK_ENONET          64      //<! Machine is not on the network.
#define  U_SOCK_ENOPKG          65      //<! Package not installed.
#define  U_SOCK_EREMOTE         66      //<! Object is remote.
#define  U_SOCK_ENOLINK         67      //<! Link has been severed.
#define  U_SOCK_EADV            68      //<! Advertise error.
#define  U_SOCK_ESRMNT          69      //<! Srmount error.
#define  U_SOCK_ECOMM           70      //<! Communication error on send.
#define  U_SOCK_EPROTO          71      //<! Protocol error.
#define  U_SOCK_EMULTIHOP       72      //<! Multihop attempted.
#define  U_SOCK_EDOTDOT         73      //<! RFS specific error.
#define  U_SOCK_EBADMSG         74      //<! Not a data message.
#define  U_SOCK_EOVERFLOW       75      //<! Value too large for defined data type.
#define  U_SOCK_ENOTUNIQ        76      //<! Name not unique on network.
#define  U_SOCK_EBADFD          77      //<! File descriptor in bad state.
#define  U_SOCK_EREMCHG         78      //<! Remote address changed.
#define  U_SOCK_ELIBACC         79      //<! Can not access a needed shared library.
#define  U_SOCK_ELIBBAD         80      //<! Accessing a corrupted shared library.
#define  U_SOCK_ELIBSCN         81      //<! .lib section in a.out corrupted.
#define  U_SOCK_ELIBMAX         82      //<! Attempting to link in too many shared libraries.
#define  U_SOCK_ELIBEXEC        83      //<! Cannot exec a shared library directly.
#define  U_SOCK_EILSEQ          84      //<! Illegal byte sequence.
#define  U_SOCK_ERESTART        85      //<! Interrupted system call should be restarted.
#define  U_SOCK_ESTRPIPE        86      //<! Streams pipe error.
#define  U_SOCK_EUSERS          87      //<! Too many users.
#define  U_SOCK_ENOTSOCK        88      //<! Socket operation on non-socket.
#define  U_SOCK_EDESTADDRREQ    89      //<! Destination address required.
#define  U_SOCK_EMSGSIZE        90      //<! Message too long.
#define  U_SOCK_EPROTOTYPE      91      //<! Protocol wrong type for socket.
#define  U_SOCK_ENOPROTOOPT     92      //<! Protocol not available.
#define  U_SOCK_EPROTONOSUPPORT 93      //<! Protocol not supported.
#define  U_SOCK_ESOCKTNOSUPPORT 94      //<! Socket type not supported.
#define  U_SOCK_EOPNOTSUPP      95      //<! Operation not supported on transport endpoint.
#define  U_SOCK_EPFNOSUPPORT    96      //<! Protocol family not supported.
#define  U_SOCK_EAFNOSUPPORT    97      //<! Address family not supported by protocol.
#define  U_SOCK_EADDRINUSE      98      //<! Address already in use.
#define  U_SOCK_EADDRNOTAVAIL   99      //<! Cannot assign requested address.
#define  U_SOCK_ENETDOWN       100      //<! Network is down.
#define  U_SOCK_ENETUNREACH    101      //<! Network is unreachable.
#define  U_SOCK_ENETRESET      102      //<! Network dropped connection because of reset.
#define  U_SOCK_ECONNABORTED   103      //<! Software caused connection abort.
#define  U_SOCK_ECONNRESET     104      //<! Connection reset by peer.
#define  U_SOCK_ENOBUFS        105      //<! No buffer space available.
#define  U_SOCK_EISCONN        106      //<! Transport endpoint is already connected.
#define  U_SOCK_ENOTCONN       107      //<! Transport endpoint is not connected.
#define  U_SOCK_ESHUTDOWN      108      //<! Cannot send after transport endpoint shutdown.
#define  U_SOCK_ETOOMANYREFS   109      //<! Too many references: cannot splice.
#define  U_SOCK_ETIMEDOUT      110      //<! Connection timed out.
#define  U_SOCK_ECONNREFUSED   111      //<! Connection refused.
#define  U_SOCK_EHOSTDOWN      112      //<! Host is down.
#define  U_SOCK_EHOSTUNREACH   113      //<! No route to host.
#define  U_SOCK_EALREADY       114      //<! Operation already in progress.
#define  U_SOCK_EINPROGRESS    115      //<! Operation now in progress.
#define  U_SOCK_ESTALE         116      //<! Stale NFS file handle.
#define  U_SOCK_EUCLEAN        117      //<! Structure needs cleaning.
#define  U_SOCK_ENOTNAM        118      //<! Not a XENIX named type file.
#define  U_SOCK_ENAVAIL        119      //<! No XENIX semaphores available.
#define  U_SOCK_EISNAM         120      //<! Is a named type file.
#define  U_SOCK_EREMOTEIO      121      //<! Remote I/O error.
#define  U_SOCK_EDQUOT         122      //<! Quota exceeded.

#define  U_SOCK_ENOMEDIUM      123      //<! No medium found.
#define  U_SOCK_EMEDIUMTYPE    124      //<! Wrong medium type.

/** @}*/

#endif // _U_SOCK_ERRNO_H_

// End of file
