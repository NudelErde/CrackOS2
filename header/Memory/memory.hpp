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
class unique_ptr {
private:
    T* ptr;

public:
    unique_ptr() : ptr(nullptr) {}
    unique_ptr(T* ptr) : ptr(ptr) {}
    unique_ptr(unique_ptr&& other) noexcept : ptr(other.ptr) {
        other.ptr = nullptr;
    }
    unique_ptr& operator=(unique_ptr&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        ptr = other.ptr;
        other.ptr = nullptr;
        return *this;
    }
    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;
    ~unique_ptr() {
        if (ptr) {
            delete ptr;
        }
    }
    bool operator==(const unique_ptr& other) const {
        return ptr == other.ptr;
    }
    bool operator!=(const unique_ptr& other) const {
        return ptr != other.ptr;
    }
    bool exists() const {
        return ptr != nullptr;
    }
    T* get() {
        return ptr;
    }
    T* operator->() {
        return ptr;
    }
    T& operator*() {
        return *ptr;
    }
    T* release() {
        T* ret = ptr;
        ptr = nullptr;
        return ret;
    }
};

template<typename T, typename... Args>
unique_ptr<T> make_unique(Args&&... args) {
    return unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template<typename T>
class weak_ptr;

template<typename T>
class shared_ptr {
public:
    struct Data {
        uint32_t refCount;
        uint32_t weakCount;
        T* ptr;
    };
    Data* data;

    shared_ptr(Data* data) : data(data) {}

    void destroy() {
        if (data) {
            data->refCount--;
            if (data->refCount == 0) {
                delete data->ptr;
                if (data->weakCount == 0) {
                    delete data;
                }
            }
        }
    }

public:
    template<typename... Args>
    static shared_ptr create(Args&&... args) {
        Data* data = new Data();
        data->refCount = 1;

        data->ptr = new T(std::forward<Args>(args)...);
        return shared_ptr(data);
    }

    shared_ptr() : data(nullptr) {}
    shared_ptr(T* ptr) {
        data = new Data();
        data->refCount = 1;
        data->ptr = ptr;
    }

    shared_ptr(unique_ptr<T>&& unique) noexcept {
        if (unique.exists()) {
            shared_ptr(unique.release());
        } else {
            data = nullptr;
        }
    }

    ~shared_ptr() {
        destroy();
    }

    shared_ptr(const shared_ptr& other) {
        data = other.data;
        if (data)
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
            destroy();
        }
        data = other.data;
        if (data)
            data->refCount++;
        return *this;
    }

    shared_ptr& operator=(shared_ptr&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        if (data) {
            destroy();
        }
        data = other.data;
        other.data = nullptr;
        return *this;
    }

    bool exists() const {
        return data != nullptr;
    }

    T* operator->() {
        return (T*) data->ptr;
    }

    T* operator->() const {
        return (T*) data->ptr;
    }

    T& operator*() {
        return *(T*) data->ptr;
    }

    T& operator*() const {
        return *(T*) data->ptr;
    }

    bool operator==(const shared_ptr& other) const {
        return data == other.data;
    }

    bool operator!=(const shared_ptr& other) const {
        return data != other.data;
    }

    friend class weak_ptr<T>;
};

template<typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args) {
    return shared_ptr<T>::create(std::forward<Args>(args)...);
}

template<typename T>
class weak_ptr {
    friend class shared_ptr<T>;

    using Data = typename shared_ptr<T>::Data;
    Data* data;

    void destroy() {
        if (data) {
            data->weakCount--;
            if (data->weakCount == 0 && data->refCount == 0) {
                //object is already destroyed
                delete data;
            }
        }
    }

public:
    weak_ptr() : data(nullptr) {}
    weak_ptr(const shared_ptr<T>& ptr) : data(ptr.data) {
        if (data)
            data->weakCount++;
    }

    weak_ptr(const weak_ptr& other) : data(other.data) {
        if (data)
            data->weakCount++;
    }

    weak_ptr(weak_ptr&& other) noexcept : data(other.data) {
        other.data = nullptr;
    }

    weak_ptr& operator=(const shared_ptr<T>& ptr) {
        if (data) {
            destroy();
        }
        data = ptr.data;
        data->weakCount++;
        return *this;
    }

    weak_ptr& operator=(const weak_ptr& other) {
        if (this == &other) {
            return *this;
        }
        if (data) {
            destroy();
        }
        data = other.data;
        data->weakCount++;
        return *this;
    }

    weak_ptr& operator=(weak_ptr&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        if (data) {
            destroy();
        }
        data = other.data;
        other.data = nullptr;
        return *this;
    }

    bool exists() const {
        return data != nullptr;
    }

    bool expired() const {
        return data == nullptr || data->refCount == 0;
    }

    ~weak_ptr() {
        destroy();
    }

    shared_ptr<T> lock() const {
        if (data == nullptr || data->refCount == 0) {
            return shared_ptr<T>();
        }
        return shared_ptr<T>(data);
    }

    bool operator==(const weak_ptr& rhs) {
        return data == rhs.data;
    }

    bool operator!=(const weak_ptr& rhs) {
        return data != rhs.data;
    }
};

template<typename U, typename V>
shared_ptr<U> static_pointer_cast(const shared_ptr<V>& shared) {
    typename shared_ptr<U>::Data* data = reinterpret_cast<typename shared_ptr<U>::Data*>(shared.data);
    data->ptr = static_cast<U*>(shared.data->ptr);
    return shared_ptr<U>(data);
}