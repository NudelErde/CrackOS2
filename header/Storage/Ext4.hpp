#pragma once

#include "Storage/Filesystem.hpp"

class Ext4 : public Filesystem {
public:
    Ext4(shared_ptr<Partition> partition);

    int64_t implRead(const char* filepath, uint64_t offset, uint64_t size, uint8_t* buffer) override;
    int64_t implWrite(const char* filepath, uint64_t offset, uint64_t size, uint8_t* buffer) override;
    int64_t implGetSize(const char* filepath) override;
    bool implExists(const char* filepath) override;
    int64_t implCreateFile(const char* filepath) override;
    int64_t implDeleteFile(const char* filepath) override;
    int64_t implCreateDirectory(const char* filepath) override;
    int64_t implDeleteDirectory(const char* filepath) override;
    int64_t implGetFileList(const char* filepath, char* buffer, uint64_t bufferSize) override;
    FileType implGetType(const char* filepath) override;
    int64_t implRename(const char* oldPath, const char* newPath) override;
    int64_t implGetFileTime(const char* filepath) override;
    int64_t implSetFileTime(const char* filepath, uint64_t time) override;
    int64_t implGetFilePermissions(const char* filepath) override;
    int64_t implSetFilePermissions(const char* filepath, uint64_t permissions) override;
    int64_t implGetFileOwner(const char* filepath) override;
    int64_t implSetFileOwner(const char* filepath, uint64_t owner) override;
};