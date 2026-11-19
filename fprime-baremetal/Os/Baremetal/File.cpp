// ======================================================================
// \title Os/Baremetal/File.cpp
// \brief baremetal implementation for Os::File
// ======================================================================

#include <Fw/Types/Assert.hpp>
#include <Os/File.hpp>
#include <cstring>
#include <fprime-baremetal/Os/Baremetal/File.hpp>
#include <fprime-baremetal/Os/Baremetal/MicroFs/MicroFs.hpp>
#include <fprime-baremetal/Os/Baremetal/error.hpp>

namespace Os {
namespace Baremetal {
namespace File {

BaremetalFile::BaremetalFile(const BaremetalFile& other) : FileInterface(other) {
    this->helpAssign(other);
}

BaremetalFile::~BaremetalFile() {
    // An open file is closed on destruction. Qualified call: overrides are not reachable from a destructor
    if (this->BaremetalFile::isOpenImpl()) {
        this->BaremetalFile::close();
    }
}

BaremetalFile& BaremetalFile::operator=(const BaremetalFile& other) {
    if (this != &other) {
        // Any file currently held must be closed first or its descriptor is never released
        this->BaremetalFile::close();
        FileInterface::operator=(other);
        this->helpAssign(other);
    }
    return *this;
}

void BaremetalFile::helpAssign(const BaremetalFile& other) {
    this->m_handle.m_state_entry = other.m_handle.m_state_entry;
    this->m_handle.m_file_descriptor = other.m_handle.m_file_descriptor;
    this->m_handle.m_mode = other.m_handle.m_mode;

    if (other.m_handle.m_state_entry != BaremetalFileHandle::INVALID_STATE_ENTRY &&
        other.m_handle.m_file_descriptor != BaremetalFileHandle::INVALID_FILE_DESCRIPTOR) {
        MicroFs::MicroFsFileState* state =
            MicroFs::getFileStateFromIndex(other.m_handle.m_state_entry - MicroFs::MICROFS_FD_OFFSET);
        FW_ASSERT(state != nullptr);

        FwIndexType fdEntry = 0;
        auto status = MicroFs::getFileStateNextFreeFd(state, fdEntry);
        FW_ASSERT(status != MicroFs::Status::INVALID);

        state->fd[fdEntry].loc = state->fd[other.m_handle.m_file_descriptor].loc;
        state->fd[fdEntry].status = state->fd[other.m_handle.m_file_descriptor].status;

        // store file descriptor for this file
        this->m_handle.m_file_descriptor = fdEntry;
    }
}

BaremetalFile::Status BaremetalFile::open(const char* path,
                                          BaremetalFile::Mode mode,
                                          BaremetalFile::OverwriteType overwrite) {
    FileInterface::assertValidPath(path);
    // Dispatched virtually so that a subclass overriding openImpl() sees every open
    return this->openImpl(path, mode, overwrite);
}

bool BaremetalFile::isOpenImpl() const {
    return this->_isOpen();
}

BaremetalFile::Status BaremetalFile::openImpl(const char* path,
                                              BaremetalFile::Mode mode,
                                              BaremetalFile::OverwriteType overwrite) {
    FW_ASSERT(path != nullptr);
    FW_ASSERT((Mode::OPEN_NO_MODE < mode) && (mode < Mode::MAX_OPEN_MODE));
    FW_ASSERT((0 <= this->m_mode) && (this->m_mode < Mode::MAX_OPEN_MODE));
    FW_ASSERT((0 <= overwrite) && (overwrite < OverwriteType::MAX_OVERWRITE_TYPE));
    // Check for already opened file
    if (this->isOpen()) {
        return Status::INVALID_MODE;
    }
    Status stat = OP_OK;

    // common checks
    // retrieve index to file entry
    FwIndexType entry = 0;
    auto status = MicroFs::getFileStateIndex(path, entry);
    // not found
    if (status == MicroFs::INVALID) {
        return Status::DOESNT_EXIST;
    }

    MicroFs::MicroFsFileState* state = MicroFs::getFileStateFromIndex(entry);
    FW_ASSERT(state != nullptr);

    FwIndexType fdEntry = 0;
    status = MicroFs::getFileStateNextFreeFd(state, fdEntry);
    if (status == MicroFs::Status::INVALID) {
        return Status::NO_MORE_RESOURCES;
    }

    switch (mode) {
        case OPEN_READ:
            // if not written to yet, doesn't exist for read
            if (!state->created) {
                return Status::DOESNT_EXIST;
            }
            state->fd[fdEntry].loc = 0;
            break;
        case OPEN_WRITE:
        case OPEN_SYNC_WRITE:  // fall through; same for microfs
            // If the file has never previously been opened, then initialize the
            // size to 0.
            if (!state->created) {
                state->currSize = 0;
            }
            state->fd[fdEntry].loc = 0;
            break;
        case OPEN_CREATE:
            if (state->created && (overwrite == BaremetalFile::OverwriteType::NO_OVERWRITE)) {
                return Status::FILE_EXISTS;
            }
            // truncate file length to zero
            state->currSize = 0;
            state->fd[fdEntry].loc = 0;
            break;
        case OPEN_APPEND:
            // If the file has never previously been opened, then initialize the
            // size to 0.
            if (!state->created) {
                state->currSize = 0;
            }

            // Set to 0. On write, this loc should update to file size
            state->fd[fdEntry].loc = 0;
            break;
        default:
            FW_ASSERT(0, mode);
            break;
    }

    state->created = true;
    state->fd[fdEntry].status = MicroFs::Status::VALID;

    // store mode
    this->m_handle.m_mode = mode;
    this->markOpen(mode);

    // store file descriptor for this file
    this->m_handle.m_file_descriptor = fdEntry;

    // store state entry into state structure
    this->m_handle.m_state_entry = entry + MicroFs::MICROFS_FD_OFFSET;

    return stat;
}

bool BaremetalFile::_isOpen() const {
    FW_ASSERT((0 <= this->m_handle.m_mode) && (this->m_handle.m_mode < Mode::MAX_OPEN_MODE), this->m_handle.m_mode);
    return (this->m_handle.m_mode != OPEN_NO_MODE);
}

BaremetalFile::Status BaremetalFile::preallocate(FwSizeType offset, FwSizeType length) {
    FW_ASSERT((0 <= this->m_mode) && (this->m_mode < Mode::MAX_OPEN_MODE));
    // Check that the file is open for writing before attempting operation
    if (not this->isOpen()) {
        return Status::NOT_OPENED;
    } else if (OPEN_READ == this->m_mode) {
        return Status::INVALID_MODE;
    }
    MicroFs::MicroFsFileState* state =
        MicroFs::getFileStateFromIndex(this->m_handle.m_state_entry - MicroFs::MICROFS_FD_OFFSET);
    FW_ASSERT(state != nullptr);
    FwSizeType sum = offset + length;
    auto status = (sum > state->dataSize) ? Status::BAD_SIZE : Status::OP_OK;
    if (status == Status::OP_OK) {
        if (state->currSize < sum) {
            (void)memset(&state->data[state->currSize], 0, sum - state->currSize);
            state->currSize = sum;
        }
    }
    return status;
}

void BaremetalFile::close() {
    // A file that is already closed holds no descriptor to release
    if (this->m_handle.m_state_entry != BaremetalFileHandle::INVALID_STATE_ENTRY &&
        this->m_handle.m_file_descriptor != BaremetalFileHandle::INVALID_FILE_DESCRIPTOR) {
        // only do cleanup of file state
        // if file system memory is still around
        // catches case where file objects are still
        // lingering after cleanup
        if ((MicroFs::getSingleton()).s_microFsMem) {
            // get state to clear it
            MicroFs::MicroFsFileState* state =
                MicroFs::getFileStateFromIndex(this->m_handle.m_state_entry - MicroFs::MICROFS_FD_OFFSET);
            FW_ASSERT(state != nullptr);
            state->fd[this->m_handle.m_file_descriptor].loc = 0;
            state->fd[this->m_handle.m_file_descriptor].status = MicroFs::Status::INVALID;
        }
    }
    // reset fd
    this->m_handle.m_file_descriptor = BaremetalFileHandle::INVALID_FILE_DESCRIPTOR;
    this->m_handle.m_mode = OPEN_NO_MODE;
    this->markClosed();
}

BaremetalFile::Status BaremetalFile::size(FwSizeType& size_result) {
    FW_ASSERT((0 <= this->m_mode) && (this->m_mode < Mode::MAX_OPEN_MODE));
    // Check that the file is open before attempting operation
    if (not this->isOpen()) {
        return Status::NOT_OPENED;
    }
    MicroFs::MicroFsFileState* state =
        MicroFs::getFileStateFromIndex(this->m_handle.m_state_entry - MicroFs::MICROFS_FD_OFFSET);
    FW_ASSERT(state != nullptr);
    size_result = state->currSize;

    return OP_OK;
}

BaremetalFile::Status BaremetalFile::position(FwSizeType& position_result) {
    FW_ASSERT((0 <= this->m_mode) && (this->m_mode < Mode::MAX_OPEN_MODE));
    // Check that the file is open before attempting operation
    if (not this->isOpen()) {
        return Status::NOT_OPENED;
    }
    MicroFs::MicroFsFileState* state =
        MicroFs::getFileStateFromIndex(this->m_handle.m_state_entry - MicroFs::MICROFS_FD_OFFSET);
    FW_ASSERT(state != nullptr);
    position_result = state->fd[this->m_handle.m_file_descriptor].loc;

    return OP_OK;
}

BaremetalFile::Status BaremetalFile::seek(FwSignedSizeType offset, BaremetalFile::SeekType seekType) {
    FW_ASSERT((0 <= seekType) && (seekType < SeekType::MAX_SEEK_TYPE));
    // Cannot do a seek with a negative offset in absolute mode
    FW_ASSERT((seekType == SeekType::RELATIVE) || (offset >= 0));
    FW_ASSERT((0 <= this->m_mode) && (this->m_mode < Mode::MAX_OPEN_MODE));
    // Check that the file is open before attempting operation
    if (not this->isOpen()) {
        return Status::NOT_OPENED;
    }
    // make sure it has been opened
    if (!this->_isOpen()) {
        return NOT_OPENED;
    }

    // get file state entry
    FW_ASSERT(this->m_handle.m_state_entry != BaremetalFileHandle::INVALID_STATE_ENTRY);

    MicroFs::MicroFsFileState* state =
        MicroFs::getFileStateFromIndex(this->m_handle.m_state_entry - MicroFs::MICROFS_FD_OFFSET);
    FW_ASSERT(state != nullptr);

    auto& loc = state->fd[this->m_handle.m_file_descriptor].loc;

    // compute new operation location
    switch (seekType) {
        case SeekType::ABSOLUTE:
            // make sure not too far
            if ((offset > static_cast<FwSignedSizeType>(state->dataSize)) or (offset < 0)) {
                return INVALID_ARGUMENT;
            }
            loc = offset;
            break;
        case SeekType::RELATIVE:
            // make sure not too far
            if (static_cast<FwSizeType>(loc + offset) > state->dataSize) {
                return INVALID_ARGUMENT;
            }
            loc += offset;
            break;
        default:
            FW_ASSERT(0, seekType);
            break;
    }

    // Per POSIX semantics, seeking past EOF does not change file size.
    // The size will be updated when a write occurs, and any gap will be zero-filled then.

    return OP_OK;
}

BaremetalFile::Status BaremetalFile::flush() {
    FW_ASSERT((0 <= this->m_mode) && (this->m_mode < Mode::MAX_OPEN_MODE));
    // Check that the file is open for writing before attempting operation
    if (not this->isOpen()) {
        return Status::NOT_OPENED;
    } else if (OPEN_READ == this->m_mode) {
        return Status::INVALID_MODE;
    }
    // make sure it has been opened
    if (!this->_isOpen()) {
        return NOT_OPENED;
    }

    // RAM, so no need to flush
    return OP_OK;
}

BaremetalFile::Status BaremetalFile::read(U8* buffer, FwSizeType& size, BaremetalFile::WaitType wait) {
    FW_ASSERT(buffer != nullptr);
    FW_ASSERT((0 <= this->m_mode) && (this->m_mode < Mode::MAX_OPEN_MODE));
    // Check that the file is open for reading before attempting operation
    if (not this->isOpen()) {
        size = 0;
        return Status::NOT_OPENED;
    } else if (OPEN_READ != this->m_mode) {
        size = 0;
        return Status::INVALID_MODE;
    }

    // make sure it has been opened
    if (!this->_isOpen()) {
        size = 0;
        return NOT_OPENED;
    }
    // get file state entry
    FW_ASSERT(this->m_handle.m_state_entry != BaremetalFileHandle::INVALID_STATE_ENTRY);

    MicroFs::MicroFsFileState* state =
        MicroFs::getFileStateFromIndex(this->m_handle.m_state_entry - MicroFs::MICROFS_FD_OFFSET);
    FW_ASSERT(state != nullptr);

    // Make code more readable
    auto& loc = state->fd[this->m_handle.m_file_descriptor].loc;

    // find size to copy

    // check to see if already at the end of the file. If so, return 0 for size
    if (loc >= (state->currSize)) {
        size = 0;
        return OP_OK;
    }

    // copy requested bytes, unless it would be more than the file size.
    // If it would be more than the file size, copy the remainder and set
    // the size to the actual copied
    if ((loc + size) > (state->currSize)) {
        size = state->currSize - loc;
    }

    // copy data from location to buffer
    (void)memcpy(buffer, state->data + loc, size);

    // move location pointer
    loc += size;

    return OP_OK;
}

BaremetalFile::Status BaremetalFile::write(const U8* buffer, FwSizeType& size, BaremetalFile::WaitType wait) {
    FW_ASSERT(buffer != nullptr);
    FW_ASSERT((0 <= this->m_mode) && (this->m_mode < Mode::MAX_OPEN_MODE));
    // Check that the file is open for writing before attempting operation
    if (not this->isOpen()) {
        size = 0;
        return Status::NOT_OPENED;
    } else if (OPEN_READ == this->m_mode) {
        size = 0;
        return Status::INVALID_MODE;
    }

    // make sure it has been opened in write mode
    switch (this->m_handle.m_mode) {
        case OPEN_NO_MODE:
            return NOT_OPENED;
        case OPEN_WRITE:  // all fall through for write cases
        case OPEN_SYNC_WRITE:
        case OPEN_CREATE:
        case OPEN_APPEND:
            break;
        default:
            size = 0;
            return OTHER_ERROR;
    }

    // get file state entry
    FW_ASSERT(this->m_handle.m_state_entry != BaremetalFileHandle::INVALID_STATE_ENTRY);

    MicroFs::MicroFsFileState* state =
        MicroFs::getFileStateFromIndex(this->m_handle.m_state_entry - MicroFs::MICROFS_FD_OFFSET);
    FW_ASSERT(state != nullptr);

    // write up to the end of the allocated buffer
    // if write size is greater, truncate the write
    // and set size to what was actually written
    FwSizeType& loc = state->fd[this->m_handle.m_file_descriptor].loc;

    // Make sure we write to the end of file when appending
    // Only reposition for APPEND mode if we're actually writing data (size > 0)
    // Per POSIX semantics, zero-byte writes should not reposition in APPEND mode
    if (this->m_handle.m_mode == OPEN_APPEND && size > 0) {
        loc = state->currSize;
    }

    // If writing past current file size AND actually writing data, zero-fill the gap
    // Note: A zero-byte write should NOT expand the file per POSIX semantics
    if (size > 0 && loc > state->currSize) {
        (void)memset(&state->data[state->currSize], 0, loc - state->currSize);
    }

    if (loc + size > state->dataSize) {
        size = state->dataSize - loc;
    }

    // copy data to file buffer (only if size > 0)
    if (size > 0) {
        (void)memcpy(&state->data[loc], buffer, size);
    }

    // increment location
    loc += size;

    // Check if the currSize is to be increased.
    // Only increase size if we actually wrote data (size > 0).
    // A zero-byte write should not expand the file per POSIX semantics.
    if (size > 0 && loc > state->currSize) {
        state->currSize = loc;
    }

    return OP_OK;
}

FileHandle* BaremetalFile::getHandle() {
    return &this->m_handle;
}

}  // namespace File
}  // namespace Baremetal
}  // namespace Os
