#include "Storage/Filesystem.hpp"
#include "Common/Math.hpp"
#include "LanguageFeatures/memory.hpp"

bool Filesystem::handles(const char* filepath) {
    uint64_t inLength = strlen(filepath);
    if (inLength == 0) {
        return false;
    }
    if (inLength < prefixLength) {
        return false;
    }
    for (uint64_t i = 0; i < inLength; i++) {
        if (handlingPrefix[i] == '\0') {
            return true;
        }
        if (filepath[i] != handlingPrefix[i]) {
            return false;
        }
    }
    return false;
}
void Filesystem::setHandlingPrefix(const char* filepath) {
    uint64_t length = strlen(filepath);
    if (handlingPrefix) {
        delete handlingPrefix;
    }
    handlingPrefix = new char[length + 1];
    prefixLength = length;
    memcpy(handlingPrefix, filepath, length + 1);
}

struct FilesystemNode {
    shared_ptr<Filesystem> filesystem;
    shared_ptr<FilesystemNode> next;
};

static FilesystemNode* firstFilesystemNode;

static shared_ptr<Filesystem> createFilesystem(const shared_ptr<Partition>& partition) {
    if (partition) {
        return partition->createFilesystem(partition);
    }
    return shared_ptr<Filesystem>();
}

void Filesystem::addPartition(const shared_ptr<Partition>& partition, const char* prefix) {
    shared_ptr<Filesystem> filesystem = createFilesystem(partition);
    if (!filesystem) {
        return;
    }
    filesystem->setHandlingPrefix(prefix);
    if (firstFilesystemNode != nullptr) {
        shared_ptr<FilesystemNode> node = make_shared<FilesystemNode>();
        node->filesystem = filesystem;
        node->next = firstFilesystemNode->next;
        firstFilesystemNode->next = std::move(node);
    } else {
        firstFilesystemNode = new FilesystemNode();
        firstFilesystemNode->filesystem = filesystem;
    }
}

static shared_ptr<Filesystem> getFilesystem(const char* filepath) {
    if (strlen(filepath) == 0) {
        return shared_ptr<Filesystem>();
    }
    for (FilesystemNode* node = firstFilesystemNode; node; node = node->next.get()) {
        if (node->filesystem->handles(filepath)) {
            return node->filesystem;
        }
    }
    return shared_ptr<Filesystem>();
}

int64_t Filesystem::read(const char* filepath, uint64_t offset, uint64_t size, uint8_t* buffer) {
    shared_ptr<Filesystem> filesystem = getFilesystem(filepath);
    if (!filesystem) {
        return -1;
    }
    const char* path = filesystem->prefixLength + filepath;
    return filesystem->implRead(path, offset, size, buffer);
}
int64_t Filesystem::write(const char* filepath, uint64_t offset, uint64_t size, uint8_t* buffer) {
    shared_ptr<Filesystem> filesystem = getFilesystem(filepath);
    if (!filesystem) {
        return -1;
    }
    const char* path = filesystem->prefixLength + filepath;
    return filesystem->implWrite(path, offset, size, buffer);
}
int64_t Filesystem::getSize(const char* filepath) {
    shared_ptr<Filesystem> filesystem = getFilesystem(filepath);
    if (!filesystem) {
        return -1;
    }
    const char* path = filesystem->prefixLength + filepath;
    return filesystem->implGetSize(path);
}
bool Filesystem::exists(const char* filepath) {
    shared_ptr<Filesystem> filesystem = getFilesystem(filepath);
    if (!filesystem) {
        return false;
    }
    const char* path = filesystem->prefixLength + filepath;
    return filesystem->implExists(path);
}
int64_t Filesystem::createFile(const char* filepath) {
    shared_ptr<Filesystem> filesystem = getFilesystem(filepath);
    if (!filesystem) {
        return -1;
    }
    const char* path = filesystem->prefixLength + filepath;
    return filesystem->implCreateFile(path);
}
int64_t Filesystem::deleteFile(const char* filepath) {
    shared_ptr<Filesystem> filesystem = getFilesystem(filepath);
    if (!filesystem) {
        return -1;
    }
    const char* path = filesystem->prefixLength + filepath;
    return filesystem->implDeleteFile(path);
}
int64_t Filesystem::createDirectory(const char* filepath) {
    shared_ptr<Filesystem> filesystem = getFilesystem(filepath);
    if (!filesystem) {
        return -1;
    }
    const char* path = filesystem->prefixLength + filepath;
    return filesystem->implCreateDirectory(path);
}
int64_t Filesystem::deleteDirectory(const char* filepath) {
    shared_ptr<Filesystem> filesystem = getFilesystem(filepath);
    if (!filesystem) {
        return -1;
    }
    const char* path = filesystem->prefixLength + filepath;
    return filesystem->implDeleteDirectory(path);
}
int64_t Filesystem::getFileList(const char* filepath, char* buffer, uint64_t bufferSize) {
    shared_ptr<Filesystem> filesystem = getFilesystem(filepath);
    if (!filesystem) {
        return -1;
    }
    const char* path = filesystem->prefixLength + filepath;
    return filesystem->implGetFileList(path, buffer, bufferSize);
}
Filesystem::FileType Filesystem::getType(const char* filepath) {
    shared_ptr<Filesystem> filesystem = getFilesystem(filepath);
    if (!filesystem) {
        return FileType::None;
    }
    const char* path = filesystem->prefixLength + filepath;
    return filesystem->implGetType(path);
}
int64_t Filesystem::rename(const char* oldPath, const char* newPath) {
    shared_ptr<Filesystem> filesystem = getFilesystem(oldPath);
    if (!filesystem) {
        return -1;
    }
    const char* oldP = filesystem->prefixLength + oldPath;
    const char* newP = filesystem->prefixLength + newPath;
    return filesystem->implRename(oldP, newP);
}
int64_t Filesystem::getFileTime(const char* filepath) {
    shared_ptr<Filesystem> filesystem = getFilesystem(filepath);
    if (!filesystem) {
        return -1;
    }
    const char* path = filesystem->prefixLength + filepath;
    return filesystem->implGetFileTime(path);
}
int64_t Filesystem::setFileTime(const char* filepath, uint64_t time) {
    shared_ptr<Filesystem> filesystem = getFilesystem(filepath);
    if (!filesystem) {
        return -1;
    }
    const char* path = filesystem->prefixLength + filepath;
    return filesystem->implSetFileTime(path, time);
}
int64_t Filesystem::getFilePermissions(const char* filepath) {
    shared_ptr<Filesystem> filesystem = getFilesystem(filepath);
    if (!filesystem) {
        return -1;
    }
    const char* path = filesystem->prefixLength + filepath;
    return filesystem->implGetFilePermissions(path);
}
int64_t Filesystem::setFilePermissions(const char* filepath, uint64_t permissions) {
    shared_ptr<Filesystem> filesystem = getFilesystem(filepath);
    if (!filesystem) {
        return -1;
    }
    const char* path = filesystem->prefixLength + filepath;
    return filesystem->implSetFilePermissions(path, permissions);
}
int64_t Filesystem::getFileOwner(const char* filepath) {
    shared_ptr<Filesystem> filesystem = getFilesystem(filepath);
    if (!filesystem) {
        return -1;
    }
    const char* path = filesystem->prefixLength + filepath;
    return filesystem->implGetFileOwner(path);
}
int64_t Filesystem::setFileOwner(const char* filepath, uint64_t owner) {
    shared_ptr<Filesystem> filesystem = getFilesystem(filepath);
    if (!filesystem) {
        return -1;
    }
    const char* path = filesystem->prefixLength + filepath;
    return filesystem->implSetFileOwner(path, owner);
}