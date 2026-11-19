// ======================================================================
// \title Os/Baremetal/error.cpp
// \brief implementation for baremetal errno conversion
// ======================================================================
#include "fprime-baremetal/Os/Baremetal/error.hpp"
#include <cerrno>

namespace Os {
namespace Baremetal {

Os::FileInterface::Status errno_to_file_status(int errno_input) {
    Os::FileInterface::Status status = Os::FileInterface::Status::OP_OK;
    switch (errno_input) {
        case 0:
            status = Os::FileInterface::Status::OP_OK;
            break;
        // Fallthrough intended
        case ENOSPC:
        case EFBIG:
            status = Os::FileInterface::Status::NO_SPACE;
            break;
        case ENOENT:
            status = Os::FileInterface::Status::DOESNT_EXIST;
            break;
        // Fallthrough intended
        case EPERM:
        case EACCES:
            status = Os::FileInterface::Status::NO_PERMISSION;
            break;
        case EEXIST:
            status = Os::FileInterface::Status::FILE_EXISTS;
            break;
        case EBADF:
            status = Os::FileInterface::Status::NOT_OPENED;
            break;
        // Fallthrough intended
        case ENOSYS:
        case EOPNOTSUPP:
            status = Os::FileInterface::Status::NOT_SUPPORTED;
            break;
        case EINVAL:
            status = Os::FileInterface::Status::INVALID_ARGUMENT;
            break;
        default:
            status = Os::FileInterface::Status::OTHER_ERROR;
            break;
    }
    return status;
}

Os::FileSystemInterface::Status errno_to_filesystem_status(int errno_input) {
    Os::FileSystemInterface::Status status = Os::FileSystemInterface::Status::OP_OK;
    switch (errno_input) {
        case 0:
            status = Os::FileSystemInterface::Status::OP_OK;
            break;
        // All fall through are intended to fallback on OTHER_ERROR
        case EACCES:
            status = Os::FileSystemInterface::Status::NO_PERMISSION;
            break;
        case EPERM:
        case EROFS:
        case EFAULT:
            status = Os::FileSystemInterface::Status::NO_PERMISSION;
            break;
        case EEXIST:
            status = Os::FileSystemInterface::Status::ALREADY_EXISTS;
            break;
        case ELOOP:
        case ENOENT:
            status = Os::FileSystemInterface::Status::DOESNT_EXIST;
            break;
        case ENAMETOOLONG:
            status = Os::FileSystemInterface::Status::INVALID_PATH;
            break;
        case ENOTDIR:
            status = Os::FileSystemInterface::Status::NOT_DIR;
            break;
        case EDQUOT:
            status = Os::FileSystemInterface::Status::NO_SPACE;
            break;
        case EMLINK:
            status = Os::FileSystemInterface::Status::FILE_LIMIT;
            break;
        case ENOSPC:
        case EFBIG:
            status = Os::FileSystemInterface::Status::NO_SPACE;
            break;
        case ENOSYS:
        case EOPNOTSUPP:
            status = Os::FileSystemInterface::Status::NOT_SUPPORTED;
            break;
        case ERANGE:
            status = Os::FileSystemInterface::Status::BUFFER_TOO_SMALL;
            break;
        case EXDEV:
            status = Os::FileSystemInterface::Status::EXDEV_ERROR;
            break;
        default:
            status = Os::FileSystemInterface::Status::OTHER_ERROR;
            break;
    }
    return status;
}

Os::DirectoryInterface::Status errno_to_directory_status(int errno_input) {
    Os::DirectoryInterface::Status status = Os::DirectoryInterface::Status::OP_OK;
    switch (errno_input) {
        case 0:
            status = Os::DirectoryInterface::Status::OP_OK;
            break;
        case ENOENT:
            status = Os::DirectoryInterface::Status::DOESNT_EXIST;
            break;
        case EACCES:
            status = Os::DirectoryInterface::Status::NO_PERMISSION;
            break;
        case ENOTDIR:
            status = Os::DirectoryInterface::Status::NOT_DIR;
            break;
        case EEXIST:
            status = Os::DirectoryInterface::Status::ALREADY_EXISTS;
            break;
        default:
            status = Os::DirectoryInterface::Status::OTHER_ERROR;
            break;
    }
    return status;
}

Os::RawTimeInterface::Status errno_to_rawtime_status(int errno_input) {
    Os::RawTimeInterface::Status status = Os::RawTimeInterface::Status::OP_OK;
    switch (errno_input) {
        case 0:
            status = Os::RawTimeInterface::Status::OP_OK;
            break;
        case EINVAL:
            status = Os::RawTimeInterface::Status::INVALID_PARAMS;
            break;
        default:
            status = Os::RawTimeInterface::Status::OTHER_ERROR;
            break;
    }
    return status;
}

Os::TaskInterface::Status baremetal_status_to_task_status(int baremetal_status) {
    Os::TaskInterface::Status status = Os::TaskInterface::Status::OP_OK;
    switch (baremetal_status) {
        case 0:
            status = Os::TaskInterface::Status::OP_OK;
            break;
        case EINVAL:
            status = Os::TaskInterface::Status::INVALID_PARAMS;
            break;
        case EPERM:
            status = Os::TaskInterface::Status::ERROR_PERMISSION;
            break;
        case EAGAIN:
            status = Os::TaskInterface::Status::ERROR_RESOURCES;
            break;
        default:
            status = Os::TaskInterface::Status::UNKNOWN_ERROR;
            break;
    }
    return status;
}
}  // namespace Baremetal
}  // namespace Os
