#include "Storage/Ext4.hpp"

Ext4::Ext4(shared_ptr<Partition> partition) : Filesystem(partition), valid(false) {
    Output::getDefault()->printf("Ext4: Initializing filesystem\n");
    superblock = make_unique<Superblock>();
    if (partition->read(1024, sizeof(Superblock), (uint8_t*) superblock.get()) != sizeof(Superblock)) {
        Output::getDefault()->printf("Ext4: Failed to read superblock\n");
        return;
    }
    if (superblock->magic != 0xEF53) {
        Output::getDefault()->printf("Ext4: Invalid magic: %hx\n", superblock->magic);
        return;
    }

    blockSize = (1 << (10 + superblock->log_block_size));

    INode node;
    getINode(2, &node);
    Output::getDefault()->printf("Ext4: Root inode mode: %hx\n", node.mode);

    valid = true;
}

int64_t Ext4::implRead(const char* filepath, uint64_t offset, uint64_t size, uint8_t* buffer) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implRead(%s, %llu, %llu, %p)\n", filepath, offset, size, buffer);
    return -1;
}
int64_t Ext4::implWrite(const char* filepath, uint64_t offset, uint64_t size, uint8_t* buffer) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implWrite(%s, %llu, %llu, %p)\n", filepath, offset, size, buffer);
    return -1;
}
int64_t Ext4::implGetSize(const char* filepath) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implGetSize(%s)\n", filepath);
    return -1;
}
int64_t Ext4::implCreateFile(const char* filepath) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implCreateFile(%s)\n", filepath);
    return -1;
}
int64_t Ext4::implDeleteFile(const char* filepath) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implDeleteFile(%s)\n", filepath);
    return -1;
}
int64_t Ext4::implCreateDirectory(const char* filepath) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implCreateDirectory(%s)\n", filepath);
    return -1;
}
int64_t Ext4::implDeleteDirectory(const char* filepath) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implDeleteDirectory(%s)\n", filepath);
    return -1;
}
int64_t Ext4::implGetFileList(const char* filepath, char* buffer, uint64_t bufferSize) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implGetFileList(%s, %p, %llu)\n", filepath, buffer, bufferSize);
    return -1;
}
int64_t Ext4::implRename(const char* oldPath, const char* newPath) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implRename(%s, %s)\n", oldPath, newPath);
    return -1;
}
int64_t Ext4::implGetFileTime(const char* filepath) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implGetFileTime(%s)\n", filepath);
    return -1;
}
int64_t Ext4::implSetFileTime(const char* filepath, uint64_t time) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implSetFileTime(%s, %llu)\n", filepath, time);
    return -1;
}
int64_t Ext4::implGetFilePermissions(const char* filepath) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implGetFilePermissions(%s)\n", filepath);
    return -1;
}
int64_t Ext4::implSetFilePermissions(const char* filepath, uint64_t permissions) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implSetFilePermissions(%s, %llu)\n", filepath, permissions);
    return -1;
}
int64_t Ext4::implGetFileOwner(const char* filepath) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implGetFileOwner(%s)\n", filepath);
    return -1;
}
int64_t Ext4::implSetFileOwner(const char* filepath, uint64_t owner) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implSetFileOwner(%s, %llu)\n", filepath, owner);
    return -1;
}
bool Ext4::implExists(const char* filepath) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implExists(%s)\n", filepath);
    return false;
}
Filesystem::FileType Ext4::implGetType(const char* filepath) {
    if (!valid) { return Filesystem::FileType::Other; }
    Output::getDefault()->printf("Ext4::implGetType(%s)\n", filepath);
    return Filesystem::FileType::Other;
}

void Ext4::getINode(uint64_t inodeNumber, INode* out) {
    GroupDesc groupDesc;
    getGroupDescriptor(inodeNumber, &groupDesc);
    uint64_t index = (inodeNumber - 1) % superblock->inodes_per_group;

    uint64_t inodeTableBlock = groupDesc.inode_table_lo;
    inodeTableBlock |= ((uint64_t) groupDesc.inode_table_hi) << 32;

    inodeTableBlock *= blockSize;
    uint64_t offset = index * superblock->inode_size;

    partition->read(inodeTableBlock + offset, sizeof(INode), (uint8_t*) out);
}
void Ext4::getGroupDescriptor(uint64_t inodeNumber, GroupDesc* out) {
    uint64_t groupNumber = (inodeNumber - 1) / superblock->inodes_per_group;
    partition->read(blockSize + groupNumber * superblock->desc_size, sizeof(GroupDesc), (uint8_t*) out);
}