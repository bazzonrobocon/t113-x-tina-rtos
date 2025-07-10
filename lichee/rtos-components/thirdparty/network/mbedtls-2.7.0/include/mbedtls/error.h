/**
 * \file error.h
 *
 * \brief Error to string translation
 */
/*
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of mbed TLS (https://tls.mbed.org)
 */
#ifndef MBEDTLS_ERROR_H
#define MBEDTLS_ERROR_H

#include <stddef.h>

/**
 * Error code layout.
 *
 * Currently we try to keep all error codes within the negative space of 16
 * bits signed integers to support all platforms (-0x0001 - -0x7FFF). In
 * addition we'd like to give two layers of information on the error if
 * possible.
 *
 * For that purpose the error codes are segmented in the following manner:
 *
 * 16 bit error code bit-segmentation
 *
 * 1 bit  - Unused (sign bit)
 * 3 bits - High level module ID
 * 5 bits - Module-dependent error code
 * 7 bits - Low level module errors
 *
 * For historical reasons, low-level error codes are divided in even and odd,
 * even codes were assigned first, and -1 is reserved for other errors.
 *
 * Low-level module errors (0x0002-0x007E, 0x0003-0x007F)
 *
 * Module   Nr  Codes assigned
 * MPI       7  0x0002-0x0010
 * GCM       3  0x0012-0x0014   0x0013-0x0013
 * BLOWFISH  3  0x0016-0x0018   0x0017-0x0017
 * THREADING 3  0x001A-0x001E
 * AES       4  0x0020-0x0022   0x0023-0x0025
 * CAMELLIA  3  0x0024-0x0026   0x0027-0x0027
 * XTEA      2  0x0028-0x0028   0x0029-0x0029
 * BASE64    2  0x002A-0x002C
 * OID       1  0x002E-0x002E   0x000B-0x000B
 * PADLOCK   1  0x0030-0x0030
 * DES       2  0x0032-0x0032   0x0033-0x0033
 * CTR_DBRG  4  0x0034-0x003A
 * ENTROPY   3  0x003C-0x0040   0x003D-0x003F
 * NET      11  0x0042-0x0052   0x0043-0x0045
 * ASN1      7  0x0060-0x006C
 * CMAC      1  0x007A-0x007A
 * PBKDF2    1  0x007C-0x007C
 * HMAC_DRBG 4                  0x0003-0x0009
 * CCM       3                  0x000D-0x0011
 * ARC4      1                  0x0019-0x0019
 * MD2       1                  0x002B-0x002B
 * MD4       1                  0x002D-0x002D
 * MD5       1                  0x002F-0x002F
 * RIPEMD160 1                  0x0031-0x0031
 * SHA1      1                  0x0035-0x0035
 * SHA256    1                  0x0037-0x0037
 * SHA512    1                  0x0039-0x0039
 *
 * High-level module nr (3 bits - 0x0...-0x7...)
 * Name      ID  Nr of Errors
 * PEM       1   9
 * PKCS#12   1   4 (Started from top)
 * X509      2   20
 * PKCS5     2   4 (Started from top)
 * DHM       3   11
 * PK        3   15 (Started from top)
 * RSA       4   11
 * ECP       4   9 (Started from top)
 * MD        5   5
 * CIPHER    6   8
 * SSL       6   17 (Started from top)
 * SSL       7   31
 *
 * Module dependent error code (5 bits 0x.00.-0x.F8.)
 */

#ifdef __cplusplus
extern "C" {
#endif

#define  EPERM            1  /* Operation not permitted */
#define  ENOENT           2  /* No such file or directory */
#define  ESRCH            3  /* No such process */
#define  EINTR            4  /* Interrupted system call */
#define  EIO              5  /* I/O error */
#define  ENXIO            6  /* No such device or address */
#define  E2BIG            7  /* Arg list too long */
#define  ENOEXEC          8  /* Exec format error */
#define  EBADF            9  /* Bad file number */
#define  ECHILD          10  /* No child processes */
#define  EAGAIN          11  /* Try again */
#define  ENOMEM          12  /* Out of memory */
#define  EACCES          13  /* Permission denied */
#define  EFAULT          14  /* Bad address */
#define  ENOTBLK         15  /* Block device required */
#define  EBUSY           16  /* Device or resource busy */
#define  EEXIST          17  /* File exists */
#define  EXDEV           18  /* Cross-device link */
#define  ENODEV          19  /* No such device */
#define  ENOTDIR         20  /* Not a directory */
#define  EISDIR          21  /* Is a directory */
#define  EINVAL          22  /* Invalid argument */
#define  ENFILE          23  /* File table overflow */
#define  EMFILE          24  /* Too many open files */
#define  ENOTTY          25  /* Not a typewriter */
#define  ETXTBSY         26  /* Text file busy */
#define  EFBIG           27  /* File too large */
#define  ENOSPC          28  /* No space left on device */
#define  ESPIPE          29  /* Illegal seek */
#define  EROFS           30  /* Read-only file system */
#define  EMLINK          31  /* Too many links */
#define  EPIPE           32  /* Broken pipe */
#define  EDOM            33  /* Math argument out of domain of func */
#define  ERANGE          34  /* Math result not representable */
#define  EDEADLK         35  /* Resource deadlock would occur */
#define  ENAMETOOLONG    36  /* File name too long */
#define  ENOLCK          37  /* No record locks available */
#define  ENOSYS          38  /* Function not implemented */
#define  ENOTEMPTY       39  /* Directory not empty */
#define  ELOOP           40  /* Too many symbolic links encountered */
#define  EWOULDBLOCK     EAGAIN  /* Operation would block */
#define  ENOMSG          42  /* No message of desired type */
#define  EIDRM           43  /* Identifier removed */
#define  ECHRNG          44  /* Channel number out of range */
#define  EL2NSYNC        45  /* Level 2 not synchronized */
#define  EL3HLT          46  /* Level 3 halted */
#define  EL3RST          47  /* Level 3 reset */
#define  ELNRNG          48  /* Link number out of range */
#define  EUNATCH         49  /* Protocol driver not attached */
#define  ENOCSI          50  /* No CSI structure available */
#define  EL2HLT          51  /* Level 2 halted */
#define  EBADE           52  /* Invalid exchange */
#define  EBADR           53  /* Invalid request descriptor */
#define  EXFULL          54  /* Exchange full */
#define  ENOANO          55  /* No anode */
#define  EBADRQC         56  /* Invalid request code */
#define  EBADSLT         57  /* Invalid slot */

#define  EDEADLOCK       EDEADLK

#define  EBFONT          59  /* Bad font file format */
#define  ENOSTR          60  /* Device not a stream */
#define  ENODATA         61  /* No data available */
#define  ETIME           62  /* Timer expired */
#define  ENOSR           63  /* Out of streams resources */
#define  ENONET          64  /* Machine is not on the network */
#define  ENOPKG          65  /* Package not installed */
#define  EREMOTE         66  /* Object is remote */
#define  ENOLINK         67  /* Link has been severed */
#define  EADV            68  /* Advertise error */
#define  ESRMNT          69  /* Srmount error */
#define  ECOMM           70  /* Communication error on send */
#define  EPROTO          71  /* Protocol error */
#define  EMULTIHOP       72  /* Multihop attempted */
#define  EDOTDOT         73  /* RFS specific error */
#define  EBADMSG         74  /* Not a data message */
#define  EOVERFLOW       75  /* Value too large for defined data type */
#define  ENOTUNIQ        76  /* Name not unique on network */
#define  EBADFD          77  /* File descriptor in bad state */
#define  EREMCHG         78  /* Remote address changed */
#define  ELIBACC         79  /* Can not access a needed shared library */
#define  ELIBBAD         80  /* Accessing a corrupted shared library */
#define  ELIBSCN         81  /* .lib section in a.out corrupted */
#define  ELIBMAX         82  /* Attempting to link in too many shared libraries */
#define  ELIBEXEC        83  /* Cannot exec a shared library directly */
#define  EILSEQ          84  /* Illegal byte sequence */
#define  ERESTART        85  /* Interrupted system call should be restarted */
#define  ESTRPIPE        86  /* Streams pipe error */
#define  EUSERS          87  /* Too many users */
#define  ENOTSOCK        88  /* Socket operation on non-socket */
#define  EDESTADDRREQ    89  /* Destination address required */
#define  EMSGSIZE        90  /* Message too long */
#define  EPROTOTYPE      91  /* Protocol wrong type for socket */
#define  ENOPROTOOPT     92  /* Protocol not available */
#define  EPROTONOSUPPORT 93  /* Protocol not supported */
#define  ESOCKTNOSUPPORT 94  /* Socket type not supported */
#define  EOPNOTSUPP      95  /* Operation not supported on transport endpoint */
#define  EPFNOSUPPORT    96  /* Protocol family not supported */
#define  EAFNOSUPPORT    97  /* Address family not supported by protocol */
#define  EADDRINUSE      98  /* Address already in use */
#define  EADDRNOTAVAIL   99  /* Cannot assign requested address */
#define  ENETDOWN       100  /* Network is down */
#define  ENETUNREACH    101  /* Network is unreachable */
#define  ENETRESET      102  /* Network dropped connection because of reset */
#define  ECONNABORTED   103  /* Software caused connection abort */
#define  ECONNRESET     104  /* Connection reset by peer */
#define  ENOBUFS        105  /* No buffer space available */
#define  EISCONN        106  /* Transport endpoint is already connected */
#define  ENOTCONN       107  /* Transport endpoint is not connected */
#define  ESHUTDOWN      108  /* Cannot send after transport endpoint shutdown */
#define  ETOOMANYREFS   109  /* Too many references: cannot splice */
#define  ETIMEDOUT      110  /* Connection timed out */
#define  ECONNREFUSED   111  /* Connection refused */
#define  EHOSTDOWN      112  /* Host is down */
#define  EHOSTUNREACH   113  /* No route to host */
#define  EALREADY       114  /* Operation already in progress */
#define  EINPROGRESS    115  /* Operation now in progress */
#define  ESTALE         116  /* Stale NFS file handle */
#define  EUCLEAN        117  /* Structure needs cleaning */
#define  ENOTNAM        118  /* Not a XENIX named type file */
#define  ENAVAIL        119  /* No XENIX semaphores available */
#define  EISNAM         120  /* Is a named type file */
#define  EREMOTEIO      121  /* Remote I/O error */
#define  EDQUOT         122  /* Quota exceeded */

#define  ENOMEDIUM      123  /* No medium found */
#define  EMEDIUMTYPE    124  /* Wrong medium type */

#ifndef errno
extern int errno;
#endif

/**
 * \brief Translate a mbed TLS error code into a string representation,
 *        Result is truncated if necessary and always includes a terminating
 *        null byte.
 *
 * \param errnum    error code
 * \param buffer    buffer to place representation in
 * \param buflen    length of the buffer
 */
void mbedtls_strerror( int errnum, char *buffer, size_t buflen );

#ifdef __cplusplus
}
#endif

#endif /* error.h */
