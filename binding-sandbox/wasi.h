/*
** wasi.h
**
** This file is part of mkxp.
**
** Copyright (C) 2024 - 2026 The mkxp-z authors
**
** mkxp is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 2 of the License, or
** (at your option) any later version.
**
** mkxp is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MKXPZ_SANDBOX_WASI_H
#define MKXPZ_SANDBOX_WASI_H

#include <deque>
#include <memory>
#include <string>
#include <vector>
#include <priority_deque.hpp>
#include <unordered_set>
#include "filesystem.h"
#include "wasm-types.h"
#include "binding-base.h"

// WASI Preview 1 error numbers
#define WASIP1_ESUCCESS 0 // No error occurred. System call completed successfully.
#define WASIP1_E2BIG 1 // Argument list too long.
#define WASIP1_EACCES 2 // Permission denied.
#define WASIP1_EADDRINUSE 3 // Address in use.
#define WASIP1_EADDRNOTAVAIL 4 // Address not available.
#define WASIP1_EAFNOSUPPORT 5 // Address family not supported.
#define WASIP1_EAGAIN 6 // Resource unavailable, or operation would block.
#define WASIP1_EALREADY 7 // Connection already in progress.
#define WASIP1_EBADF 8 // Bad file descriptor.
#define WASIP1_EBADMSG 9 // Bad message.
#define WASIP1_EBUSY 10 // Device or resource busy.
#define WASIP1_ECANCELED 11 // Operation canceled.
#define WASIP1_ECHILD 12 // No child processes.
#define WASIP1_ECONNABORTED 13 // Connection aborted.
#define WASIP1_ECONNREFUSED 14 // Connection refused.
#define WASIP1_ECONNRESET 15 // Connection reset.
#define WASIP1_EDEADLK 16 // Resource deadlock would occur.
#define WASIP1_EDESTADDRREQ 17 // Destination address required.
#define WASIP1_EDOM 18 // Mathematics argument out of domain of function.
#define WASIP1_EDQUOT 19 // Reserved.
#define WASIP1_EEXIST 20 // File exists.
#define WASIP1_EFAULT 21 // Bad address.
#define WASIP1_EFBIG 22 // File too large.
#define WASIP1_EHOSTUNREACH 23 // Host is unreachable.
#define WASIP1_EIDRM 24 // Identifier removed.
#define WASIP1_EILSEQ 25 // Illegal byte sequence.
#define WASIP1_EINPROGRESS 26 // Operation in progress.
#define WASIP1_EINTR 27 // Interrupted function.
#define WASIP1_EINVAL 28 // Invalid argument.
#define WASIP1_EIO 29 // I/O error.
#define WASIP1_EISCONN 30 // Socket is connected.
#define WASIP1_EISDIR 31 // Is a directory.
#define WASIP1_ELOOP 32 // Too many levels of symbolic links.
#define WASIP1_EMFILE 33 // File descriptor value too large.
#define WASIP1_EMLINK 34 // Too many links.
#define WASIP1_EMSGSIZE 35 // Message too large.
#define WASIP1_EMULTIHOP 36 // Reserved.
#define WASIP1_ENAMETOOLONG 37 // Filename too long.
#define WASIP1_ENETDOWN 38 // Network is down.
#define WASIP1_ENETRESET 39 // Connection aborted by network.
#define WASIP1_ENETUNREACH 40 // Network unreachable.
#define WASIP1_ENFILE 41 // Too many files open in system.
#define WASIP1_ENOBUFS 42 // No buffer space available.
#define WASIP1_ENODEV 43 // No such device.
#define WASIP1_ENOENT 44 // No such file or directory.
#define WASIP1_ENOEXEC 45 // Executable file format error.
#define WASIP1_ENOLCK 46 // No locks available.
#define WASIP1_ENOLINK 47 // Reserved.
#define WASIP1_ENOMEM 48 // Not enough space.
#define WASIP1_ENOMSG 49 // No message of the desired type.
#define WASIP1_ENOPROTOOPT 50 // Protocol not available.
#define WASIP1_ENOSPC 51 // No space left on device.
#define WASIP1_ENOSYS 52 // Function not supported.
#define WASIP1_ENOTCONN 53 // The socket is not connected.
#define WASIP1_ENOTDIR 54 // Not a directory or a symbolic link to a directory.
#define WASIP1_ENOTEMPTY 55 // Directory not empty.
#define WASIP1_ENOTRECOVERABLE 56 // State not recoverable.
#define WASIP1_ENOTSOCK 57 // Not a socket.
#define WASIP1_ENOTSUP 58 // Not supported, or operation not supported on socket.
#define WASIP1_ENOSTDIO 59 // Inappropriate I/O control operation.
#define WASIP1_ENXIO 60 // No such device or address.
#define WASIP1_EOVERFLOW 61 // Value too large to be stored in data type.
#define WASIP1_EOWNERDEAD 62 // Previous owner died.
#define WASIP1_EPERM 63 // Operation not permitted.
#define WASIP1_EPIPE 64 // Broken pipe.
#define WASIP1_EPROTO 65 // Protocol error.
#define WASIP1_EPROTONOSUPPORT 66 // Protocol not supported.
#define WASIP1_EPROTOTYPE 67 // Protocol wrong type for socket.
#define WASIP1_ERANGE 68 // Result too large.
#define WASIP1_EROFS 69 // Read-only file system.
#define WASIP1_ESPIPE 70 // Invalid seek.
#define WASIP1_ESRCH 71 // No such process.
#define WASIP1_ESTALE 72 // Reserved.
#define WASIP1_ETIMEDOUT 73 // Connection timed out.
#define WASIP1_ETXTBSY 74 // Text file busy.
#define WASIP1_EXDEV 75 // Cross-device link.
#define WASIP1_ENOTCAPABLE 76 // Extension: Capabilities insufficient.

// WASI Preview 1 file types
#define WASIP1_IFUNK 0 // Unknown
#define WASIP1_IFBLK 1 // Block device
#define WASIP1_IFCHR 2 // Character device
#define WASIP1_IFDIR 3 // Directory
#define WASIP1_IFREG 4 // Regular file
#define WASIP1_IFSOCKD 5 // Datagram socket
#define WASIP1_IFSOCKS 6 // Stream socket
#define WASIP1_IFLNK 7 // Symbolic link

// WASI Preview 1 file flags
#define WASIP1_APPEND (1 << 0)
#define WASIP1_DSYNC (1 << 1)
#define WASIP1_NONBLOCK (1 << 2)
#define WASIP1_RSYNC (1 << 3)
#define WASIP1_SYNC (1 << 4)

// WASI Preview 1 rights flags
#define WASIP1_FD_DATASYNC (1 << 0)
#define WASIP1_FD_READ (1 << 1)
#define WASIP1_FD_SEEK (1 << 2)
#define WASIP1_FD_FDSTAT_SET_FLAGS (1 << 3)
#define WASIP1_FD_SYNC (1 << 4)
#define WASIP1_FD_TELL (1 << 5)
#define WASIP1_FD_WRITE (1 << 6)
#define WASIP1_FD_ADVISE (1 << 7)
#define WASIP1_FD_ALLOCATE (1 << 8)
#define WASIP1_PATH_CREATE_DIRECTORY (1 << 9)
#define WASIP1_PATH_CREATE_FILE (1 << 10)
#define WASIP1_PATH_LINK_SOURCE (1 << 11)
#define WASIP1_PATH_LINK_TARGET (1 << 12)
#define WASIP1_PATH_OPEN (1 << 13)
#define WASIP1_FD_READDIR (1 << 14)
#define WASIP1_PATH_READLINK (1 << 15)
#define WASIP1_PATH_RENAME_SOURCE (1 << 16)
#define WASIP1_PATH_RENAME_TARGET (1 << 17)
#define WASIP1_PATH_FILESTAT_GET (1 << 18)
#define WASIP1_PATH_FILESTAT_SET_SIZE (1 << 19)
#define WASIP1_PATH_FILESTAT_SET_TIMES (1 << 20)
#define WASIP1_FD_FILESTAT_GET (1 << 21)
#define WASIP1_FD_FILESTAT_SET_SIZE (1 << 22)
#define WASIP1_FD_FILESTAT_SET_TIMES (1 << 23)
#define WASIP1_PATH_SYMLINK (1 << 24)
#define WASIP1_PATH_REMOVE_DIRECTORY (1 << 25)
#define WASIP1_PATH_UNLINK_FILE (1 << 26)
#define WASIP1_POLL_FD_READWRITE (1 << 27)
#define WASIP1_SOCK_SHUTDOWN (1 << 28)
#define WASIP1_SOCK_ACCEPT (1 << 29)

// WASI Preview 2 filesystem descriptor types
#define WASI_FILESYSTEM_DESCRIPTOR_TYPE_UNKNOWN 0 // The type of the descriptor or file is unknown or is different from any of the other types specified.
#define WASI_FILESYSTEM_DESCRIPTOR_TYPE_BLOCK_DEVICE 1 // The descriptor refers to a block device inode.
#define WASI_FILESYSTEM_DESCRIPTOR_TYPE_CHARACTER_DEVICE 2 // The descriptor refers to a character device inode.
#define WASI_FILESYSTEM_DESCRIPTOR_TYPE_DIRECTORY 3 // The descriptor refers to a directory inode.
#define WASI_FILESYSTEM_DESCRIPTOR_TYPE_FIFO 4 // The descriptor refers to a named pipe.
#define WASI_FILESYSTEM_DESCRIPTOR_TYPE_SYMBOLIC_LINK 5 // The file refers to a symbolic link inode.
#define WASI_FILESYSTEM_DESCRIPTOR_TYPE_REGULAR_FILE 6 // The descriptor refers to a regular file inode.
#define WASI_FILESYSTEM_DESCRIPTOR_TYPE_SOCKET 7 // The descriptor refers to a socket.

// WASI Preview 2 filesystem descriptor flags

// Read mode: Data can be read.
#define WASI_FILESYSTEM_DESCRIPTOR_READ (1 << 0)
// Write mode: Data can be written to.
#define WASI_FILESYSTEM_DESCRIPTOR_WRITE (1 << 1)
// Request that writes be performed according to synchronized I/O file
// integrity completion. The data stored in the file and the file's
// metadata are synchronized. This is similar to `O_SYNC` in POSIX.
//
// The precise semantics of this operation have not yet been defined for
// WASI. At this time, it should be interpreted as a request, and not a
// requirement.
#define WASI_FILESYSTEM_DESCRIPTOR_FILE_INTEGRITY_SYNC (1 << 2)
// Request that writes be performed according to synchronized I/O data
// integrity completion. Only the data stored in the file is
// synchronized. This is similar to `O_DSYNC` in POSIX.
//
// The precise semantics of this operation have not yet been defined for
// WASI. At this time, it should be interpreted as a request, and not a
// requirement.
#define WASI_FILESYSTEM_DESCRIPTOR_DATA_INTEGRITY_SYNC (1 << 3)
// Requests that reads be performed at the same level of integrety
// requested for writes. This is similar to `O_RSYNC` in POSIX.
//
// The precise semantics of this operation have not yet been defined for
// WASI. At this time, it should be interpreted as a request, and not a
// requirement.
#define WASI_FILESYSTEM_DESCRIPTOR_REQUESTED_WRITE_SYNC (1 << 4)
// Mutating directories mode: Directory contents may be mutated.
//
// When this flag is unset on a descriptor, operations using the
// descriptor which would create, rename, delete, modify the data or
// metadata of filesystem objects, or obtain another handle which
// would permit any of those, shall fail with `error-code::read-only` if
// they would otherwise succeed.
//
// This may only be set on directories.
#define WASI_FILESYSTEM_DESCRIPTOR_MUTATE_DIRECTORY (1 << 5)

// WASI Preview 2 filesystem open flags
#define WASI_FILESYSTEM_OPEN_CREATE (1 << 0) // Create file if it does not exist, similar to `O_CREAT` in POSIX.
#define WASI_FILESYSTEM_OPEN_DIRECTORY (1 << 1) // Fail if not a directory, similar to `O_DIRECTORY` in POSIX.
#define WASI_FILESYSTEM_OPEN_EXCLUSIVE (1 << 2) // Fail if file already exists, similar to `O_EXCL` in POSIX.
#define WASI_FILESYSTEM_OPEN_TRUNCATE (1 << 3) // Truncate file to size 0, similar to `O_TRUNC` in POSIX.

// WASI Preview 2 filesystem error codes
#define WASI_FILESYSTEM_ERROR_ACCESS 0 // Permission denied, similar to `EACCES` in POSIX.
#define WASI_FILESYSTEM_ERROR_WOULD_BLOCK 1 // Resource unavailable, or operation would block, similar to `EAGAIN` and `EWOULDBLOCK` in POSIX.
#define WASI_FILESYSTEM_ERROR_ALREADY 2 // Connection already in progress, similar to `EALREADY` in POSIX.
#define WASI_FILESYSTEM_ERROR_BAD_DESCRIPTOR 3 // Bad descriptor, similar to `EBADF` in POSIX.
#define WASI_FILESYSTEM_ERROR_BUSY 4 // Device or resource busy, similar to `EBUSY` in POSIX.
#define WASI_FILESYSTEM_ERROR_DEADLOCK 5 // Resource deadlock would occur, similar to `EDEADLK` in POSIX.
#define WASI_FILESYSTEM_ERROR_QUOTA 6 // Storage quota exceeded, similar to `EDQUOT` in POSIX.
#define WASI_FILESYSTEM_ERROR_EXIST 7 // File exists, similar to `EEXIST` in POSIX.
#define WASI_FILESYSTEM_ERROR_FILE_TOO_LARGE 8 // File too large, similar to `EFBIG` in POSIX.
#define WASI_FILESYSTEM_ERROR_ILLEGAL_BYTE_SEQUENCE 9 // Illegal byte sequence, similar to `EILSEQ` in POSIX.
#define WASI_FILESYSTEM_ERROR_IN_PROGRESS 10 // Operation in progress, similar to `EINPROGRESS` in POSIX.
#define WASI_FILESYSTEM_ERROR_INTERRUPTED 11 // Interrupted function, similar to `EINTR` in POSIX.
#define WASI_FILESYSTEM_ERROR_INVALID 12 // Invalid argument, similar to `EINVAL` in POSIX.
#define WASI_FILESYSTEM_ERROR_IO 13 // I/O error, similar to `EIO` in POSIX.
#define WASI_FILESYSTEM_ERROR_IS_DIRECTORY 14 // Is a directory, similar to `EISDIR` in POSIX.
#define WASI_FILESYSTEM_ERROR_LOOP 15 // Too many levels of symbolic links, similar to `ELOOP` in POSIX.
#define WASI_FILESYSTEM_ERROR_TOO_MANY_LINKS 16 // Too many links, similar to `EMLINK` in POSIX.
#define WASI_FILESYSTEM_ERROR_MESSAGE_SIZE 17 // Message too large, similar to `EMSGSIZE` in POSIX.
#define WASI_FILESYSTEM_ERROR_NAME_TOO_LONG 18 // Filename too long, similar to `ENAMETOOLONG` in POSIX.
#define WASI_FILESYSTEM_ERROR_NO_DEVICE 19 // No such device, similar to `ENODEV` in POSIX.
#define WASI_FILESYSTEM_ERROR_NO_ENTRY 20 // No such file or directory, similar to `ENOENT` in POSIX.
#define WASI_FILESYSTEM_ERROR_NO_LOCK 21 // No locks available, similar to `ENOLCK` in POSIX.
#define WASI_FILESYSTEM_ERROR_INSUFFICIENT_MEMORY 22 // Not enough space, similar to `ENOMEM` in POSIX.
#define WASI_FILESYSTEM_ERROR_INSUFFICIENT_SPACE 23 // No space left on device, similar to `ENOSPC` in POSIX.
#define WASI_FILESYSTEM_ERROR_NOT_DIRECTORY 24 // Not a directory or a symbolic link to a directory, similar to `ENOTDIR` in POSIX.
#define WASI_FILESYSTEM_ERROR_NOT_EMPTY 25 // Directory not empty, similar to `ENOTEMPTY` in POSIX.
#define WASI_FILESYSTEM_ERROR_NOT_RECOVERABLE 26 // State not recoverable, similar to `ENOTRECOVERABLE` in POSIX.
#define WASI_FILESYSTEM_ERROR_UNSUPPORTED 27 // Not supported, similar to `ENOTSUP` and `ENOSYS` in POSIX.
#define WASI_FILESYSTEM_ERROR_NO_TTY 28 // Inappropriate I/O control operation, similar to `ENOTTY` in POSIX.
#define WASI_FILESYSTEM_ERROR_NO_SUCH_DEVICE 29 // No such device or address, similar to `ENXIO` in POSIX.
#define WASI_FILESYSTEM_ERROR_OVERFLOW 30 // Value too large to be stored in data type, similar to `EOVERFLOW` in POSIX.
#define WASI_FILESYSTEM_ERROR_NOT_PERMITTED 31 // Operation not permitted, similar to `EPERM` in POSIX.
#define WASI_FILESYSTEM_ERROR_PIPE 32 // Broken pipe, similar to `EPIPE` in POSIX.
#define WASI_FILESYSTEM_ERROR_READ_ONLY 33 // Read-only file system, similar to `EROFS` in POSIX.
#define WASI_FILESYSTEM_ERROR_INVALID_SEEK 34 // Invalid seek, similar to `ESPIPE` in POSIX.
#define WASI_FILESYSTEM_ERROR_TEXT_FILE_BUSY 35 // Text file busy, similar to `ETXTBSY` in POSIX.
#define WASI_FILESYSTEM_ERROR_CROSS_DEVICE 36 // Cross-device link, similar to `EXDEV` in POSIX.

// WASI Preview 2 stream errors

// The last operation (a write or flush) failed before completion.
//
// More information is available in the `error` payload.
//
// After this, the stream will be closed. All future operations return
// `stream-error::closed`.
#define WASI_STREAMS_ERROR_LAST_OPERATION_FAILED 0
// The stream is closed: no more input will be accepted by the
// stream. A closed output-stream will return this error on all
// future operations.
#define WASI_STREAMS_ERROR_CLOSED 1

// WASI Preview 2 network errors

// Unknown error
#define WASI_NETWORK_ERROR_UNKNOWN 0
// Access denied.
//
// POSIX equivalent: EACCES, EPERM
#define WASI_NETWORK_ERROR_ACCESS_DENIED 1
// The operation is not supported.
//
// POSIX equivalent: EOPNOTSUPP
#define WASI_NETWORK_ERROR_NOT_SUPPORTED 2
// One of the arguments is invalid.
//
// POSIX equivalent: EINVAL
#define WASI_NETWORK_ERROR_INVALID_ARGUMENT 3
// Not enough memory to complete the operation.
//
// POSIX equivalent: ENOMEM, ENOBUFS, EAI_MEMORY
#define WASI_NETWORK_ERROR_OUT_OF_MEMORY 4
// The operation timed out before it could finish completely.
#define WASI_NETWORK_ERROR_TIMEOUT 5
// This operation is incompatible with another asynchronous operation that is already in progress.
//
// POSIX equivalent: EALREADY
#define WASI_NETWORK_ERROR_CONCURRENCY_CONFLICT 6
// Trying to finish an asynchronous operation that:
// - has not been started yet, or:
// - was already finished by a previous `finish-*` call.
//
// Note: this is scheduled to be removed when `future`s are natively supported.
#define WASI_NETWORK_ERROR_NOT_IN_PROGRESS 7
// The operation has been aborted because it could not be completed immediately.
//
// Note: this is scheduled to be removed when `future`s are natively supported.
#define WASI_NETWORK_ERROR_WOULD_BLOCK 8
// The operation is not valid in the socket's current state.
#define WASI_NETWORK_ERROR_INVALID_STATE 9
// A new socket resource could not be created because of a system limit.
#define WASI_NETWORK_ERROR_NEW_SOCKET_LIMIT 10
// A bind operation failed because the provided address is not an address that the `network` can bind to.
#define WASI_NETWORK_ERROR_ADDRESS_NOT_BINDABLE 11
// A bind operation failed because the provided address is already in use or because there are no ephemeral ports available.
#define WASI_NETWORK_ERROR_ADDRESS_IN_USE 12
// The remote address is not reachable
#define WASI_NETWORK_ERROR_REMOTE_UNREACHABLE 13
// The TCP connection was forcefully rejected
#define WASI_NETWORK_ERROR_CONNECTION_REFUSED 14
// The TCP connection was reset.
#define WASI_NETWORK_ERROR_CONNECTION_RESET 15
// A TCP connection was aborted.
#define WASI_NETWORK_ERROR_CONNECTION_ABORTED 16
// The size of a datagram sent to a UDP socket exceeded the maximum
// supported size.
#define WASI_NETWORK_ERROR_DATAGRAM_TOO_LARGE 17
// Name does not exist or has no suitable associated IP addresses.
#define WASI_NETWORK_ERROR_NAME_UNRESOLVABLE 18
// A temporary failure in name resolution occurred.
#define WASI_NETWORK_ERROR_TEMPORARY_RESOLVER_FAILURE 19
// A permanent failure in name resolution occurred.
#define WASI_NETWORK_ERROR_PERMANENT_RESOLVER_FAILURE 20

typedef std::pair<uint32_t, std::string> path_cache_entry_t;

struct fs_dir {
    std::string path; // Path of the directory.
    uint32_t root; // Undefined if this is a preopened directory, otherwise the file descriptor of the preopened directory that contains this directory.
    bool writable; // If true, writes made to this directory handle will be routed into the save directory. Otherwise, writes are disallowed.
};

struct fs_file {
    struct FileSystem::File file;
    std::unordered_set<uint32_t> streams; // Set of all FSFILESTREAM file descriptors that refer to this file.
    uint64_t offset; // The seek position of this file.
    uint32_t root; // The file descriptor of the preopened directory that contains this file.
};

struct fs_dir_stream {
    std::deque<std::pair<std::string, enum PHYSFS_FileType>> entries; // List of remaining directory entries.
};

struct fs_file_stream {
    uint64_t offset; // The seek position of this stream.
    uint32_t root; // If this is nonzero, the stream is open and backed by the FSFILE with this file descriptor. If this is zero, the stream is closed. The stream will auto-close when the backing file is closed.
};

struct ai_stream_entry {
    bool is_ipv6;
    union {
        uint8_t ipv4[4];
        uint16_t ipv6[8];
    } inner;
};

struct ai_stream {
    std::deque<struct ai_stream_entry> entries; // List of remaining addresses.
};

enum wasi_fd_type {
    STDIN, // This file descriptor is standard input. The `handle` field is null.
    STDOUT, // This file descriptor is standard output. The `handle` field is null.
    STDERR, // This file descriptor is standard error. The `handle` field is null.
    FS, // This file descriptor is a preopened directory handled by PhysFS. The `handle` field is a `struct fs_dir *`.
    FSDIR, // This file descriptor is a directory handled by PhysFS. The `handle` field is a `struct fs_dir *`.
    FSDIRSTREAM, // This file descriptor is a directory stream for listing the entries in a directory. The `handle` field is a `struct fs_dir_stream *`.
    FSFILE, // This file descriptor is a file handled by PhysFS. The `handle` field is a `struct fs_file *`.
    FSFILESTREAM, // This file descriptor is a file stream backed by a FSFILE. The `handle` field is a `struct fs_file_stream *`.
    AISTREAM, // This file descriptor is an address info stream. The `handle` field is a `struct ai_stream *`.
    VACANT, // Indicates this is a vacant file descriptor that doesn't correspond to a file. The `handle` field is null.
};

struct wasi_file_entry {
    // The file/directory handle that the file descriptor corresponds to. The exact type of this handle depends on the type of file descriptor.
    void *handle;

    wasi_fd_type type;

    struct fs_dir *dir_handle() const noexcept;
    struct fs_dir_stream *dir_stream() const noexcept;
    struct fs_file *file_handle() const noexcept;
    struct fs_file_stream *file_stream() const noexcept;
    struct ai_stream *ai_stream() const noexcept;
};

struct wasi_instance {
    std::shared_ptr<struct w2c_ruby> ruby;

    // WASI file descriptor table. Maps WASI file descriptors (unsigned 32-bit integers) to file handles.
    std::vector<struct wasi_file_entry> fdtable;

    // List of vacant WASI file descriptors so that we can reallocate vacant WASI file descriptors quickly.
    boost::container::priority_deque<uint32_t> vacant_fds;

    // Line buffer for standard output and standard error, respectively.
    std::string stdio_line_buffers[2];

    uint64_t prng_state;
    uint8_t prng_buffer[4];
    mkxp_sandbox::wasm_size_t prng_buffer_size;

    wasi_instance(std::shared_ptr<struct w2c_ruby> ruby);
    ~wasi_instance();
    uint32_t allocate_file_descriptor(enum wasi_fd_type type, void *handle = nullptr);
    void deallocate_file_descriptor(uint32_t fd);

    // Gets a pointer to the given address in sandbox memory.
    // Unlike `sandbox_ref`, the address does not need to be aligned.
    template <typename T> void *ptr_unaligned(mkxp_sandbox::wasm_ptr_t address) const noexcept {
        return mkxp_sandbox::sandbox_ptr_unaligned<T>(*ruby, address);
    }

    // Gets a pointer to the given index in the array at a given address in sandbox memory.
    // Unlike `sandbox_ref`, the address does not need to be aligned.
    template <typename T> void *ptr_unaligned(mkxp_sandbox::wasm_ptr_t array_address, mkxp_sandbox::wasm_size_t array_index) const noexcept {
        return mkxp_sandbox::sandbox_ptr_unaligned<T>(*ruby, array_address, array_index);
    }

    // Gets a reference to the value stored at a given address in sandbox memory.
    // Make sure the address is aligned, or this function will abort.
    template <typename T> T &ref(mkxp_sandbox::wasm_ptr_t address) const noexcept {
        return mkxp_sandbox::sandbox_ref<T>(*ruby, address);
    }

    // Gets a reference to the value stored at the given index in the array at a given address in sandbox memory.
    // Make sure the address is aligned, or this function will abort.
    template <typename T> T &ref(mkxp_sandbox::wasm_ptr_t array_address, mkxp_sandbox::wasm_size_t array_index) const noexcept {
        return ref<T>(array_address + array_index * sizeof(T));
    }

    // Checks if the array with the given address and size in bytes is within the bounds of sandbox memory.
    // If it isn't, aborts. Otherwise, does nothing.
    void check_bounds(mkxp_sandbox::wasm_ptr_t address, mkxp_sandbox::wasm_size_t size) const noexcept;

    // Gets a string stored at a given address in sandbox memory.
    struct mkxp_sandbox::sandbox_str_guard str(mkxp_sandbox::wasm_ptr_t address, mkxp_sandbox::wasm_size_t max_size) const noexcept;

    // Gets the length of a string stored at a given address in sandbox memory.
    mkxp_sandbox::wasm_size_t strlen(mkxp_sandbox::wasm_ptr_t address) const noexcept;

    // Copies a string into a sandbox memory address.
    void strcpy(mkxp_sandbox::wasm_ptr_t dst_address, const char *src) const noexcept;

    // Copies a string into a sandbox memory address.
    void strncpy_s(mkxp_sandbox::wasm_ptr_t dst_address, const char *src, mkxp_sandbox::wasm_size_t max_size) const noexcept;

    // Copies an array of length `num_elements` into a sandbox memory address.
    template <typename T> void arycpy(mkxp_sandbox::wasm_ptr_t dst_address, const T *src, mkxp_sandbox::wasm_size_t num_elements) const noexcept {
        mkxp_sandbox::sandbox_arycpy(*ruby, dst_address, src, num_elements);
    }

    // Uses `cabi_realloc()` from the Canonical ABI (see https://github.com/WebAssembly/component-model/blob/main/design/mvp/CanonicalABI.md)
    // to allocate a given amount of memory suitable for storing objects of the given type,
    // which is required to implement WASI functions in WASI Preview 2 and later that return a variable-length list or a string.
    // `cabi_realloc()` traps if allocation fails, so this will always return a valid pointer.
    template <typename T> mkxp_sandbox::wasm_ptr_t cabi_alloc(mkxp_sandbox::wasm_size_t size_in_bytes) const noexcept {
        return cabi_alloc_impl(sizeof(T), size_in_bytes);
    }

    bool sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const;

    bool sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size);

private:
    mkxp_sandbox::wasm_ptr_t cabi_alloc_impl(mkxp_sandbox::wasm_size_t alignment, mkxp_sandbox::wasm_size_t size) const noexcept;
};

struct w2c_wasi__snapshot__preview1 : wasi_instance {};

struct w2c_wasi0x3Acli0x2Fenvironment0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Acli0x2Fexit0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Acli0x2Fstderr0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Acli0x2Fstdin0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Acli0x2Fstdout0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Acli0x2Fterminal0x2Dinput0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Acli0x2Fterminal0x2Doutput0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Acli0x2Fterminal0x2Dstderr0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Acli0x2Fterminal0x2Dstdin0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Acli0x2Fterminal0x2Dstdout0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Aclocks0x2Fmonotonic0x2Dclock0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Aclocks0x2Ftimezone0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Aclocks0x2Fwall0x2Dclock0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Afilesystem0x2Fpreopens0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Afilesystem0x2Ftypes0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Ahttp0x2Foutgoing0x2Dhandler0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Ahttp0x2Ftypes0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Aio0x2Ferror0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Aio0x2Fpoll0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Aio0x2Fstreams0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Arandom0x2Finsecure0x2Dseed0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Arandom0x2Finsecure0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Arandom0x2Frandom0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Asockets0x2Finstance0x2Dnetwork0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Asockets0x2Fip0x2Dname0x2Dlookup0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Asockets0x2Fnetwork0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Asockets0x2Ftcp0x2Dcreate0x2Dsocket0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Asockets0x2Ftcp0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Asockets0x2Fudp0x2Dcreate0x2Dsocket0x4000x2E20x2E0 : wasi_instance {};
struct w2c_wasi0x3Asockets0x2Fudp0x4000x2E20x2E0 : wasi_instance {};

#endif /* MKXPZ_SANDBOX_WASI_H */
