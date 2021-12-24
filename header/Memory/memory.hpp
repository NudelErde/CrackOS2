#pragma once

#include "LanguageFeatures/Types.hpp"
#include <stdint.h>

constexpr uint64_t pageSize = 0x1000;

struct SegmentSelector {
    uint8_t requestedPrivilegeLevel : 2;
    bool inLocalTable : 1;
    uint16_t index : 13;
};
static_assert(sizeof(SegmentSelector) == sizeof(uint16_t));

enum class Segment {
    KERNEL_CODE = 1,
    KERNEL_DATA = 2,
    USER_CODE = 3,
    USER_DATA = 4,
};

inline SegmentSelector toSegmentSelector(Segment segment) {
    SegmentSelector selector;
    if (segment < Segment::USER_CODE) {
        selector.requestedPrivilegeLevel = 0;
    } else {
        selector.requestedPrivilegeLevel = 3;
    }
    selector.inLocalTable = false;
    selector.index = (uint16_t) segment;
    return selector;
}

inline void out8(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1"
                 :
                 : "a"(val), "Nd"(port));
}

inline uint8_t in8(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0"
                 : "=a"(ret)
                 : "Nd"(port));
    return ret;
}

inline void out16(uint16_t port, uint16_t val) {
    asm volatile("outw %0, %1"
                 :
                 : "a"(val), "Nd"(port));
}

inline uint16_t in16(uint16_t port) {
    uint16_t ret;
    asm volatile("inw %1, %0"
                 : "=a"(ret)
                 : "Nd"(port));
    return ret;
}

inline void out32(uint16_t port, uint32_t val) {
    asm volatile("outl %0, %1"
                 :
                 : "a"(val), "Nd"(port));
}

inline uint32_t in32(uint16_t port) {
    uint32_t ret;
    asm volatile("inl %1, %0"
                 : "=a"(ret)
                 : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    asm volatile("outb %%al, $0x80"
                 :
                 : "a"(0));
}

[[nodiscard]] inline void* operator new(uint64_t size, void* ptr) noexcept {
    return ptr;
}

template<typename T>
class shared_ptr {
    struct Data {
        uint32_t refCount;
        uint8_t objectBuffer[sizeof(T)];
    };
    Data* data;

    shared_ptr(Data* data) : data(data) {}

public:
    template<typename... Args>
    static shared_ptr create(Args&&... args) {
        Data* data = new Data();
        data->refCount = 1;
        new (data->objectBuffer) T(std::forward<Args>(args)...);
        return shared_ptr(data);
    }

    shared_ptr() : data(nullptr) {}
    shared_ptr(T* ptr) {
        data = new Data();
        data->refCount = 1;
        T* object = (T*) data->objectBuffer;
        *object = std::move(*ptr);
    }

    ~shared_ptr() {
        if (data) {
            data->refCount--;
            if (data->refCount == 0) {
                ((T*) data->objectBuffer)->~T();
                delete data;
            }
        }
    }

    shared_ptr(const shared_ptr& other) {
        data = other.data;
        data->refCount++;
    }

    shared_ptr(shared_ptr&& other) noexcept {
        data = other.data;
        other.data = nullptr;
    }

    shared_ptr& operator=(const shared_ptr& other) {
        if (this == &other) {
            return *this;
        }
        if (data) {
            data->refCount--;
            if (data->refCount == 0) {
                ((T*) data->objectBuffer)->~T();
                delete data;
            }
        }
        data = other.data;
        data->refCount++;
        return *this;
    }

    shared_ptr& operator=(shared_ptr&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        if (data) {
            data->refCount--;
            if (data->refCount == 0) {
                ((T*) data->objectBuffer)->~T();
                delete data;
            }
        }
        data = other.data;
        other.data = nullptr;
        return *this;
    }

    bool exists() const {
        return data != nullptr;
    }

    T* operator->() {
        return (T*) data->objectBuffer;
    }

    const T* operator->() const {
        return (const T*) data->objectBuffer;
    }

    T* operator&() {
        return (T*) data->objectBuffer;
    }

    const T* operator&() const {
        return (const T*) data->objectBuffer;
    }

    T& operator*() {
        return *(T*) data->objectBuffer;
    }

    const T& operator*() const {
        return *(const T*) data->objectBuffer;
    }

    bool operator==(const shared_ptr& other) const {
        return data == other.data;
    }

    bool operator!=(const shared_ptr& other) const {
        return data != other.data;
    }
};

template<typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args) {
    return shared_ptr<T>::create(std::forward<Args>(args)...);
}