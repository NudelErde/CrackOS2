#include "Storage/Ext4.hpp"
#include "Common/Math.hpp"
#include "LanguageFeatures/memory.hpp"

Ext4::Ext4(shared_ptr<Partition> partition) : Filesystem(partition), valid(false) {
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

    valid = true;
}

struct IOOperationData {
    uint64_t offset;
    uint64_t size;
    uint8_t* buffer;
    int64_t result;
};
int64_t Ext4::implRead(const char* filepath, uint64_t offset, uint64_t size, uint8_t* buffer) {
    if (!valid) { return -1; }
    uint64_t inodeNumber = getINodeNumber(filepath);
    if (inodeNumber < 1) { return -1; }
    IOOperationData data;
    data.result = 0;
    data.offset = offset;
    data.size = size;
    data.buffer = buffer;
    travelFile(
            inodeNumber, [](void* context, uint64_t offsetInFile, uint64_t offsetInPartition, uint64_t lengthInByte, Ext4* instance) -> bool {
                IOOperationData* data = (IOOperationData*) context;
                //read overlap of [offsetInFile, offsetInFile + lengthInByte) and [data->offset, data->offset + data->size)
                //return false if there is no overlap
                uint64_t overlapStart = max(offsetInFile, data->offset);                          // relative to filestart
                uint64_t overlapEnd = min(offsetInFile + lengthInByte, data->offset + data->size);// relative to filestart
                if (overlapStart >= overlapEnd) {
                    return false;
                }
                uint64_t overlapLength = overlapEnd - overlapStart;

                uint64_t realOffsetInPartition = offsetInPartition + overlapStart;

                uint64_t offsetInBuffer = data->offset - overlapStart;
                // example:
                // data->offset: 0x1000, offsetInFile = 0xF00, offsetInBuffer = 0x1000 - 0xF00 = 0x100

                uint64_t lengthInBuffer = min(overlapLength, data->size - offsetInBuffer);

                data->result += instance->partition->read(realOffsetInPartition, lengthInBuffer, data->buffer + offsetInBuffer);
                return false;
            },
            &data);
    return data.result;
}
int64_t Ext4::implWrite(const char* filepath, uint64_t offset, uint64_t size, uint8_t* buffer) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implWrite(%s, %llu, %llu, %p)\n", filepath, offset, size, buffer);
    Output::getDefault()->printf("Not implemented\n");
    stop();
    return -1;
}
int64_t Ext4::implGetSize(const char* filepath) {
    if (!valid) { return -1; }
    int64_t inodeNumber = getINodeNumber(filepath);
    if (inodeNumber < 0) { return -1; }
    INode node;
    getINode(inodeNumber, &node);
    return node.getFileSize();
}
int64_t Ext4::implCreateFile(const char* filepath) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implCreateFile(%s)\n", filepath);
    Output::getDefault()->printf("Not implemented\n");
    stop();
    return -1;
}
int64_t Ext4::implDeleteFile(const char* filepath) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implDeleteFile(%s)\n", filepath);
    Output::getDefault()->printf("Not implemented\n");
    stop();
    return -1;
}
int64_t Ext4::implCreateDirectory(const char* filepath) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implCreateDirectory(%s)\n", filepath);
    Output::getDefault()->printf("Not implemented\n");
    stop();
    return -1;
}
int64_t Ext4::implDeleteDirectory(const char* filepath) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implDeleteDirectory(%s)\n", filepath);
    Output::getDefault()->printf("Not implemented\n");
    stop();
    return -1;
}
int64_t Ext4::implGetFileList(const char* filepath, char* buffer, uint64_t bufferSize) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implGetFileList(%s, %p, %llu)\n", filepath, buffer, bufferSize);
    Output::getDefault()->printf("Not implemented\n");
    stop();
    return -1;
}
int64_t Ext4::implRename(const char* oldPath, const char* newPath) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implRename(%s, %s)\n", oldPath, newPath);
    Output::getDefault()->printf("Not implemented\n");
    stop();
    return -1;
}
int64_t Ext4::implGetFileTime(const char* filepath) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implGetFileTime(%s)\n", filepath);
    Output::getDefault()->printf("Not implemented\n");
    stop();
    return -1;
}
int64_t Ext4::implSetFileTime(const char* filepath, uint64_t time) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implSetFileTime(%s, %llu)\n", filepath, time);
    Output::getDefault()->printf("Not implemented\n");
    stop();
    return -1;
}
int64_t Ext4::implGetFilePermissions(const char* filepath) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implGetFilePermissions(%s)\n", filepath);
    Output::getDefault()->printf("Not implemented\n");
    stop();
    return -1;
}
int64_t Ext4::implSetFilePermissions(const char* filepath, uint64_t permissions) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implSetFilePermissions(%s, %llu)\n", filepath, permissions);
    Output::getDefault()->printf("Not implemented\n");
    stop();
    return -1;
}
int64_t Ext4::implGetFileOwner(const char* filepath) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implGetFileOwner(%s)\n", filepath);
    Output::getDefault()->printf("Not implemented\n");
    stop();
    return -1;
}
int64_t Ext4::implSetFileOwner(const char* filepath, uint64_t owner) {
    if (!valid) { return -1; }
    Output::getDefault()->printf("Ext4::implSetFileOwner(%s, %llu)\n", filepath, owner);
    Output::getDefault()->printf("Not implemented\n");
    stop();
    return -1;
}
bool Ext4::implExists(const char* filepath) {
    if (!valid) { return false; }
    uint64_t inodeNumber = getINodeNumber(filepath);
    if (inodeNumber < 1) { return false; }
    return true;
}
Filesystem::FileType Ext4::implGetType(const char* filepath) {
    if (!valid) { return Filesystem::FileType::Other; }
    uint64_t inodeNumber = getINodeNumber(filepath);
    if (inodeNumber < 1) { return Filesystem::FileType::None; }
    INode node;
    getINode(inodeNumber, &node);
    uint64_t type = node.mode & 0xF000;
    switch (type) {
        case 0x8000:
            return Filesystem::FileType::Directory;
        case 0x4000:
            return Filesystem::FileType::File;
        default:
            return Filesystem::FileType::Other;
    }
}

void Ext4::getINode(int64_t inodeNumber, INode* out) {
    if (inodeNumber < 1) {
        return;
    }
    GroupDesc groupDesc;
    getGroupDescriptor(inodeNumber, &groupDesc);
    uint64_t index = (inodeNumber - 1) % superblock->inodes_per_group;

    uint64_t inodeTableBlock = groupDesc.inode_table_lo;
    inodeTableBlock |= ((uint64_t) groupDesc.inode_table_hi) << 32;

    inodeTableBlock *= blockSize;
    uint64_t offset = index * superblock->inode_size;

    partition->read(inodeTableBlock + offset, sizeof(INode), (uint8_t*) out);
}
void Ext4::getGroupDescriptor(int64_t inodeNumber, GroupDesc* out) {
    if (inodeNumber < 1) {
        return;
    }
    uint64_t groupNumber = (inodeNumber - 1) / superblock->inodes_per_group;
    partition->read(blockSize + groupNumber * superblock->desc_size, sizeof(GroupDesc), (uint8_t*) out);
}

struct ExtentHeader {
    uint16_t magic;
    uint16_t entries;
    uint16_t maxEntries;
    uint16_t depth;
    uint32_t generation;
} __attribute__((packed));
static_assert(sizeof(ExtentHeader) == 12, "ExtentHeader has wrong size");
struct ExtentIndex {
    uint32_t blockInFile;
    uint32_t leafLow;
    uint16_t leafHigh;
    uint16_t unused;
} __attribute__((packed));
static_assert(sizeof(ExtentIndex) == 12, "ExtentIndex has wrong size");
struct ExtentLeaf {
    uint32_t blockInFile;
    uint16_t blockCount;// max value = 32768
    uint16_t startHigh;
    uint32_t startLow;
} __attribute__((packed));
static_assert(sizeof(ExtentLeaf) == 12, "ExtentLeaf has wrong size");

bool Ext4::travelExtentTree(uint8_t* buffer, TravelCallback callback, void* context) {
    ExtentHeader* header = (ExtentHeader*) buffer;
    if (header->magic != 0xF30A) {
        return false;
    }
    uint8_t* entries = buffer + sizeof(ExtentHeader);
    if (header->depth == 0) {
        for (uint16_t i = 0; i < header->entries; i++) {
            ExtentLeaf* leaf = (ExtentLeaf*) entries;
            uint64_t blockInPartition = leaf->startLow | ((uint64_t) leaf->startHigh) << 32;
            if (callback(context, leaf->blockInFile * blockSize, blockInPartition * blockSize, leaf->blockCount * blockSize, this)) {
                return true;
            }
            entries += sizeof(ExtentLeaf);
        }
    } else {
        for (uint16_t i = 0; i < header->entries; ++i) {
            ExtentIndex* index = (ExtentIndex*) entries;
            uint8_t* buffer = new uint8_t[blockSize];
            uint64_t position = index->leafLow | ((uint64_t) index->leafHigh) << 32;
            partition->read(position * blockSize, blockSize, buffer);
            if (travelExtentTree(buffer, callback, context)) {
                return true;
            }
            delete[] buffer;
        }
    }
    return false;
}

void Ext4::travelFile(int64_t inodeNumber, TravelCallback callback, void* context) {
    if (inodeNumber < 1) {
        return;
    }
    INode inode;
    getINode(inodeNumber, &inode);
    if (inode.flags & 0x80000) {// extend tree
        travelExtentTree((uint8_t*) &inode.block, callback, context);
        return;
    }
    if (inode.flags & 0x10000000) {// inline data
        GroupDesc groupDesc;
        getGroupDescriptor(inodeNumber, &groupDesc);

        uint64_t index = (inodeNumber - 1) % superblock->inodes_per_group;

        uint64_t inodeTableBlock = groupDesc.inode_table_lo;
        inodeTableBlock |= ((uint64_t) groupDesc.inode_table_hi) << 32;

        inodeTableBlock *= blockSize;
        uint64_t offset = index * superblock->inode_size;
        callback(context, 0, inodeTableBlock + offset + 0x28, inode.getFileSize(), this);// address of inline data
        return;
    }
}

bool lookForFile(void* context, uint64_t inFile, uint64_t inPartition, uint64_t size, Ext4* instance) {
    for (uint64_t offset = 0; offset < size; offset += instance->blockSize) {//iterate every block
        uint8_t* buffer = new uint8_t[instance->blockSize];
        instance->partition->read(inPartition + offset, instance->blockSize, buffer);
        for (uint64_t i = 0; i < instance->blockSize;) {// iterate every entry
            struct DirEntry {
                uint32_t inode;
                uint16_t recLength;
                uint8_t nameLength;
                uint8_t fileType;
                char name[];
            } __attribute__((packed));
            static_assert(sizeof(DirEntry) == 8, "DirEntry has wrong size");
            DirEntry* entry = (DirEntry*) (buffer + i);
            i += entry->recLength;
            if (entry->inode == 0) {
                continue;
            }
            const char* filename = *(const char**) context;
            bool invalid = false;
            for (uint16_t j = 0; j < entry->nameLength; ++j) {
                if (entry->name[j] != filename[j]) {
                    invalid = true;
                    break;
                }
            }
            if (invalid) {
                continue;
            }
            if (filename[entry->nameLength] == '\0') {
                *(int64_t*) context = entry->inode;
                *(uint64_t*) context |= 1ull << 63ull;// signal that we found the file
                delete[] buffer;
                return true;
            } else if (filename[entry->nameLength] == '/' && entry->fileType == 0x2) {
                //found directory
                if (filename[entry->nameLength + 1] == '\0') {//found directory
                    *(int64_t*) context = entry->inode;
                } else {
                    *(int64_t*) context = instance->getINodeNumber(filename + entry->nameLength, entry->inode);
                }
                *(uint64_t*) context |= 1ull << 63ull;// signal that we found the file
                delete[] buffer;
                return true;
            }
        }
        delete[] buffer;
    }
    return false;
}

int64_t Ext4::getINodeNumber(const char* filepath, int64_t inodeNumber) {
    if (inodeNumber < 1) {
        return -1;
    }
    if (inodeNumber == 2 && strcmp(filepath, "/") == 0) {
        return 2;
    }
    INode inode;
    getINode(inodeNumber, &inode);
    //if directory search for entry
    //if file return inode number
    if ((inode.mode & 0xF000ull) == 0x4000ull) {
        //directory
        union Context {
            int64_t result;
            const char* filepath;
        };
        static_assert(sizeof(Context) == sizeof(uint64_t), "Context has wrong size");
        Context context;
        context.filepath = filepath;
        while (context.filepath[0] == '/') {
            context.filepath++;
        }
        travelFile(inodeNumber, lookForFile, &context);
        if (((uint64_t) context.result) & 1ull << 63ull) {
            uint64_t res = ((uint64_t) context.result) & ~(1ull << 63ull);
            return res;
        }
        return -1;
    } else if ((inode.mode & 0xF000ull) == 0x8000ull) {
        //regular file
        return inodeNumber;
    }
    return -1;
}