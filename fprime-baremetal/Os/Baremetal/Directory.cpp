// ======================================================================
// \title Os/Baremetal/Directory.cpp
// \brief Baremetal implementation for Os::Directory
// ======================================================================

#include <Fw/Types/Assert.hpp>
#include <Fw/Types/StringUtils.hpp>
#include <cstdio>
#include <cstring>
#include <fprime-baremetal/Os/Baremetal/Directory.hpp>
#include <fprime-baremetal/Os/Baremetal/MicroFs/MicroFs.hpp>

namespace Os {
namespace Baremetal {
namespace Directory {

BaremetalDirectory::BaremetalDirectory() : Os::DirectoryInterface(), m_handle() {}

DirectoryHandle* BaremetalDirectory::getHandle() {
    return &this->m_handle;
}

bool BaremetalDirectory::isOpenImpl() const {
    return this->m_handle.m_dir_index != BaremetalDirectoryHandle::INVALID_DIR_DESCRIPTOR;
}

BaremetalDirectory::Status BaremetalDirectory::open(const char* path, OpenMode mode) {
    // The directory can only be one of the
    // bin file names, and it is already "created" if
    // the bin exists. See if the directory matches
    // one of the bins, and if it does, return OK, otherwise
    // return NO_PERMISSION.

    FW_ASSERT(path != nullptr);
    FW_ASSERT(mode >= 0 and mode < OpenMode::MAX_OPEN_MODE);
    // Re-opening releases the previous directory; a failed open leaves the directory closed
    if (this->isOpen()) {
        this->close();
    }

    const char* dirPathSpec = "/" MICROFS_BIN_STRING "%" MICROFS_INDEX_SCN_FORMAT;

    // if the directory number can be scanned out following the directory path spec,
    // the directory name has the correct format
    FwIndexType binIndex = 0;

    int stat = sscanf(path, dirPathSpec, &binIndex);
    if (stat != 1) {
        return NO_PERMISSION;
    }

    // If the path format is correct, check to see if it is in the
    // range of bins
    if (binIndex < MicroFs::getSingleton().s_microFsConfig.numBins) {
        this->m_handle.m_dir_index = binIndex;
        this->m_handle.m_file_index = 0;
        return OP_OK;
    } else {
        return NO_PERMISSION;
    }
}

BaremetalDirectory::Status BaremetalDirectory::rewind() {
    if (not this->isOpen()) {
        return Status::NOT_OPENED;
    }
    this->m_handle.m_file_index = 0;
    return Status::OP_OK;
}

BaremetalDirectory::Status BaremetalDirectory::read(char* fileNameBuffer, FwSizeType bufSize) {
    FW_ASSERT(fileNameBuffer != nullptr);
    FW_ASSERT(bufSize > 0);
    if (not this->isOpen()) {
        return Status::NOT_OPENED;
    }

    MicroFs& microfs = MicroFs::getSingleton();

    if (this->m_handle.m_file_index < 0 ||
        FwSizeType(this->m_handle.m_file_index) >= microfs.s_microFsConfig.bins[this->m_handle.m_dir_index].numFiles) {
        return NO_MORE_FILES;
    }

    Status status = NO_MORE_FILES;

    for (; FwSizeType(this->m_handle.m_file_index) < microfs.s_microFsConfig.bins[this->m_handle.m_dir_index].numFiles;
         this->m_handle.m_file_index++) {
        Fw::String fileStr;
        const char* filePathSpec = "/" MICROFS_BIN_STRING "%d/" MICROFS_FILE_STRING "%d";
        fileStr.format(filePathSpec, this->m_handle.m_dir_index, this->m_handle.m_file_index);

        // get file state
        FwIndexType fileIndex = 0;
        auto stateStatus = MicroFs::getFileStateIndex(fileStr.toChar(), fileIndex);
        // should always find it, since it is from a known valid bin
        FW_ASSERT(stateStatus != MicroFs::Status::INVALID);
        MicroFs::MicroFsFileState* fState = MicroFs::getFileStateFromIndex(fileIndex);
        FW_ASSERT(fState != nullptr);

        // check to see if it has been written
        if (fState->created) {
            // Copy at most bufSize characters; the result is always null-terminated
            (void)Fw::StringUtils::string_copy(fileNameBuffer, fileStr.toChar(), bufSize);
            status = OP_OK;
            // increment here too otherwise the break will cause the index to not increment
            this->m_handle.m_file_index++;
            break;
        }
    }

    return status;
}

void BaremetalDirectory::close() {
    this->m_handle.m_dir_index = BaremetalDirectoryHandle::INVALID_DIR_DESCRIPTOR;
}

}  // namespace Directory
}  // namespace Baremetal
}  // namespace Os
