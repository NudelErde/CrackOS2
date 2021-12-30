#include "Storage/Ext4.hpp"

Ext4::Ext4(shared_ptr<Partition> partition) : Filesystem(partition) {
    Output::getDefault()->printf("Ext4: Initializing filesystem\n");
}

int64_t Ext4::implRead(const char* filepath, uint64_t offset, uint64_t size, uint8_t* buffer) {
    Output::getDefault()->printf("Ext4::implRead(%s, %llu, %llu, %p)\n", filepath, offset, size, buffer);
    return -1;
}
int64_t Ext4::implWrite(const char* filepath, uint64_t offset, uint64_t size, uint8_t* buffer) {
    Output::getDefault()->printf("Ext4::implWrite(%s, %llu, %llu, %p)\n", filepath, offset, size, buffer);
    return -1;
}
int64_t Ext4::implGetSize(const char* filepath) {
    Output::getDefault()->printf("Ext4::implGetSize(%s)\n", filepath);
    return -1;
}
int64_t Ext4::implCreateFile(const char* filepath) {
    Output::getDefault()->printf("Ext4::implCreateFile(%s)\n", filepath);
    return -1;
}
int64_t Ext4::implDeleteFile(const char* filepath) {
    Output::getDefault()->printf("Ext4::implDeleteFile(%s)\n", filepath);
    return -1;
}
int64_t Ext4::implCreateDirectory(const char* filepath) {
    Output::getDefault()->printf("Ext4::implCreateDirectory(%s)\n", filepath);
    return -1;
}
int64_t Ext4::implDeleteDirectory(const char* filepath) {
    Output::getDefault()->printf("Ext4::implDeleteDirectory(%s)\n", filepath);
    return -1;
}
int64_t Ext4::implGetFileList(const char* filepath, char* buffer, uint64_t bufferSize) {
    Output::getDefault()->printf("Ext4::implGetFileList(%s, %p, %llu)\n", filepath, buffer, bufferSize);
    return -1;
}
int64_t Ext4::implRename(const char* oldPath, const char* newPath) {
    Output::getDefault()->printf("Ext4::implRename(%s, %s)\n", oldPath, newPath);
    return -1;
}
int64_t Ext4::implGetFileTime(const char* filepath) {
    Output::getDefault()->printf("Ext4::implGetFileTime(%s)\n", filepath);
    return -1;
}
int64_t Ext4::implSetFileTime(const char* filepath, uint64_t time) {
    Output::getDefault()->printf("Ext4::implSetFileTime(%s, %llu)\n", filepath, time);
    return -1;
}
int64_t Ext4::implGetFilePermissions(const char* filepath) {
    Output::getDefault()->printf("Ext4::implGetFilePermissions(%s)\n", filepath);
    return -1;
}
int64_t Ext4::implSetFilePermissions(const char* filepath, uint64_t permissions) {
    Output::getDefault()->printf("Ext4::implSetFilePermissions(%s, %llu)\n", filepath, permissions);
    return -1;
}
int64_t Ext4::implGetFileOwner(const char* filepath) {
    Output::getDefault()->printf("Ext4::implGetFileOwner(%s)\n", filepath);
    return -1;
}
int64_t Ext4::implSetFileOwner(const char* filepath, uint64_t owner) {
    Output::getDefault()->printf("Ext4::implSetFileOwner(%s, %llu)\n", filepath, owner);
    return -1;
}
bool Ext4::implExists(const char* filepath) {
    Output::getDefault()->printf("Ext4::implExists(%s)\n", filepath);
    return false;
}
Filesystem::FileType Ext4::implGetType(const char* filepath) {
    Output::getDefault()->printf("Ext4::implGetType(%s)\n", filepath);
    return Filesystem::FileType::Other;
}