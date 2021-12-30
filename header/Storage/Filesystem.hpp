#pragma once

#include "Storage/Partition.hpp"
#include "stdint.h"

class Filesystem {
public:
    enum class FileType {
        File,
        Directory,
        Other,
        None
    };

    static void simplifyPath(const char* path, char* buffer, uint64_t bufferSize);

    static int64_t read(const char* filepath, uint64_t offset, uint64_t size, uint8_t* buffer);
    static int64_t write(const char* filepath, uint64_t offset, uint64_t size, uint8_t* buffer);
    static int64_t getSize(const char* filepath);
    static bool exists(const char* filepath);
    static int64_t createFile(const char* filepath);
    static int64_t deleteFile(const char* filepath);
    static int64_t createDirectory(const char* filepath);
    static int64_t deleteDirectory(const char* filepath);
    static int64_t getFileList(const char* filepath, char* buffer, uint64_t bufferSize);
    static FileType getType(const char* filepath);
    static int64_t rename(const char* oldPath, const char* newPath);
    static int64_t getFileTime(const char* filepath);
    static int64_t setFileTime(const char* filepath, uint64_t time);
    static int64_t getFilePermissions(const char* filepath);
    static int64_t setFilePermissions(const char* filepath, uint64_t permissions);
    static int64_t getFileOwner(const char* filepath);
    static int64_t setFileOwner(const char* filepath, uint64_t owner);

    static void addPartition(const shared_ptr<Partition>&, const char* prefix);

private:
    shared_ptr<Partition> partition;
    char* handlingPrefix;
    uint64_t prefixLength;

public:
    bool handles(const char* filepath);
    void setHandlingPrefix(const char* filepath);

    inline Filesystem(shared_ptr<Partition> partition) : partition(partition), handlingPrefix(nullptr), prefixLength(0) {}

    virtual int64_t implRead(const char* filepath, uint64_t offset, uint64_t size, uint8_t* buffer) = 0;
    virtual int64_t implWrite(const char* filepath, uint64_t offset, uint64_t size, uint8_t* buffer) = 0;
    virtual int64_t implGetSize(const char* filepath) = 0;
    virtual bool implExists(const char* filepath) = 0;
    virtual int64_t implCreateFile(const char* filepath) = 0;
    virtual int64_t implDeleteFile(const char* filepath) = 0;
    virtual int64_t implCreateDirectory(const char* filepath) = 0;
    virtual int64_t implDeleteDirectory(const char* filepath) = 0;
    virtual int64_t implGetFileList(const char* filepath, char* buffer, uint64_t bufferSize) = 0;
    virtual FileType implGetType(const char* filepath) = 0;
    virtual int64_t implRename(const char* oldPath, const char* newPath) = 0;
    virtual int64_t implGetFileTime(const char* filepath) = 0;
    virtual int64_t implSetFileTime(const char* filepath, uint64_t time) = 0;
    virtual int64_t implGetFilePermissions(const char* filepath) = 0;
    virtual int64_t implSetFilePermissions(const char* filepath, uint64_t permissions) = 0;
    virtual int64_t implGetFileOwner(const char* filepath) = 0;
    virtual int64_t implSetFileOwner(const char* filepath, uint64_t owner) = 0;
};