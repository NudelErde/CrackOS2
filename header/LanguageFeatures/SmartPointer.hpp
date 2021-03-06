#pragma once

#include "BasicOutput/Output.hpp"
#include "CPUControl/cpu.hpp"
#include "LanguageFeatures/Types.hpp"
#include <stdint.h>

template<typename T>
class enable_shared_from_this;

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
        if (ptr) {
            delete ptr;
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
    operator bool() const {
        return exists();
    }
    T* get() {
        return ptr;
    }
    const T* get() const {
        return ptr;
    }
    T* operator->() {
        return ptr;
    }
    T* operator->() const {
        return ptr;
    }
    T& operator*() {
        return *ptr;
    }
    const T& operator*() const {
        return *ptr;
    }
    T* release() {
        T* ret = ptr;
        ptr = nullptr;
        return ret;
    }
    T& operator[](int64_t index) {
        return ptr[index];
    }
    const T& operator[](int64_t index) const {
        return ptr[index];
    }
};

template<typename T, typename... Args>
unique_ptr<T> make_unique(Args&&... args) {
    return unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template<typename T>
class weak_ptr;

#ifdef SHARED_POINTER_DEBUG_OUTPUT
#define SHARED_POINTER_DEBUG_DESTROY() Output::getDefault()->printf("(this = %p) Destroying shared_ptr(%p) (%u references -> %u references) (%u weak references)\n", this, data, data->refCount, data->refCount - 1, data->weakCount)
#define SHARED_POINTER_DEBUG_NORMAL_CONSTRUCTOR() Output::getDefault()->printf("(this = %p) shared_ptr constructor %p (%u references -> %u references) (%u weak references)\n", this, data, data->refCount - 1, data->refCount, data->weakCount)
#define SHARED_POINTER_DEBUG_DESTRUCTOR() \
    if (data) Output::getDefault()->printf("(this = %p) shared_ptr destructor %p (%u references -> %u references) (%u weak references)\n", this, data, data->refCount, data->refCount - 1, data->weakCount)
#define SHARED_POINTER_DEBUG_COPY_CONSTRUCTOR() ([&]() {                                                                                                                                                                             \
    if (other.data) {                                                                                                                                                                                                                \
        Output::getDefault()->printf("(this = %p) shared_ptr copy constructor %p (%u references -> %u references) (%u weak references)\n", this, other.data, other.data->refCount, other.data->refCount + 1, other.data->weakCount); \
    } else {                                                                                                                                                                                                                         \
        Output::getDefault()->printf("(this = %p) shared_ptr copy constructor %p\n", this, other.data);                                                                                                                              \
    } })()
#define SHARED_POINTER_DEBUG_MOVE_CONSTRUCTOR() ([&]() {                                                                                                                                                                             \
    if (other.data) {                                                                                                                                                                                                                \
        Output::getDefault()->printf("(this = %p) shared_ptr move constructor %p (%u references -> %u references) (%u weak references)\n", this, other.data, other.data->refCount, other.data->refCount + 1, other.data->weakCount); \
    } else {                                                                                                                                                                                                                         \
        Output::getDefault()->printf("(this = %p) shared_ptr move constructor %p\n", this, other.data);                                                                                                                              \
    } })()
#define SHARED_POINTER_DEBUG_COPY_ASSIGN() ([&]() {                                                                                                                                                                                  \
        Output::getDefault()->printf("(this = %p) shared_ptr(%p)", this, data);                                                                                                                                                      \
        if (this->data != nullptr) {                                                                                                                                                                                                 \
            Output::getDefault()->printf(" (%u references -> %u references) (% weak references)", data->refCount, data->refCount - 1, data->weakCount);                                                                              \
        }                                                                                                                                                                                                                            \
        Output::getDefault()->printf(" copy assignment %p", other.data);                                                                                                                                                             \
        if (other.data != nullptr) {                                                                                                                                                                                                 \
            Output::getDefault()->printf(" (%u references -> %u references) (%u weak references)\n", other.data->refCount, other.data->refCount + 1, other.data->weakCount);\
        } })()
#define SHARED_POINTER_DEBUG_MOVE_ASSIGN() ([&]() {                                                                                                                                                                                  \
        Output::getDefault()->printf("(this = %p) shared_ptr(%p)", this, data);                                                                                                                                                      \
        if (this->data != nullptr) {                                                                                                                                                                                                 \
            Output::getDefault()->printf(" (%u references -> %u references) (% weak references)", data->refCount, data->refCount - 1, data->weakCount);                                                                              \
        }                                                                                                                                                                                                                            \
        Output::getDefault()->printf(" copy assignment %p", other.data);                                                                                                                                                             \
        if (other.data != nullptr) {                                                                                                                                                                                                 \
            Output::getDefault()->printf(" (%u references -> %u references) (%u weak references)\n", other.data->refCount, other.data->refCount + 1, other.data->weakCount);\
        } })()


#else
#define SHARED_POINTER_DEBUG_DESTROY()
#define SHARED_POINTER_DEBUG_NORMAL_CONSTRUCTOR()
#define SHARED_POINTER_DEBUG_DESTRUCTOR()
#define SHARED_POINTER_DEBUG_COPY_CONSTRUCTOR()
#define SHARED_POINTER_DEBUG_MOVE_CONSTRUCTOR()
#define SHARED_POINTER_DEBUG_COPY_ASSIGN()
#define SHARED_POINTER_DEBUG_MOVE_ASSIGN()

#endif

template<typename T>
class weak_ptr;

template<typename T>
class shared_ptr;

template<typename T>
weak_ptr<T> weak_ptr_from_shared(const shared_ptr<T>& ptr);

template<typename T, typename = int>
struct IsLikeEnableShared : false_type {};

template<typename T>
struct IsLikeEnableShared<T, decltype((void) T::__self, 0)> : true_type {};

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
            SHARED_POINTER_DEBUG_DESTROY();
            data->refCount--;
            if (data->refCount == 0) {
                delete data->ptr;
                data->ptr = nullptr;
                if (data->weakCount == 0) {
                    delete data;
                    data = nullptr;
                }
            }
            data = nullptr;// do not delete data twice
        }
    }

    void check() const {
        if (data == nullptr) {
            Output::getDefault()->printf("(this = %p) Shared pointer accessed after destruction\n", this);
            stop();
        }
        if (data->ptr == nullptr) {
            Output::getDefault()->printf("(this = %p) Shared pointer (%p) data accessed after destruction (%i references, %i weak references)\n", this, data, data->refCount, data->weakCount);
            stop();
        }
    }

public:
    shared_ptr() : data(nullptr) {}
    shared_ptr(T* ptr) {
        data = new Data();
        data->refCount = 1;
        data->ptr = ptr;
        if constexpr (IsLikeEnableShared<T>::value) {
            data->ptr->__self = weak_ptr_from_shared<T>(*this);
        }
        SHARED_POINTER_DEBUG_NORMAL_CONSTRUCTOR();
    }

    shared_ptr(unique_ptr<T>&& unique) noexcept {
        if (unique.exists()) {
            T* p = unique.release();
            data = new Data();
            data->refCount = 1;
            data->ptr = p;
            if constexpr (IsLikeEnableShared<T>::value) {
                data->ptr->__self = weak_ptr_from_shared<T>(*this);
            }
            SHARED_POINTER_DEBUG_NORMAL_CONSTRUCTOR();
        } else {
            data = nullptr;
        }
    }

    ~shared_ptr() {
        SHARED_POINTER_DEBUG_DESTRUCTOR();
        destroy();
    }

    shared_ptr(const shared_ptr& other) {
        SHARED_POINTER_DEBUG_COPY_CONSTRUCTOR();
        data = other.data;
        if (data)
            data->refCount++;
    }

    shared_ptr(shared_ptr&& other) noexcept {
        SHARED_POINTER_DEBUG_MOVE_CONSTRUCTOR();
        data = other.data;
        other.data = nullptr;
    }

    shared_ptr& operator=(const shared_ptr& other) {
        SHARED_POINTER_DEBUG_COPY_ASSIGN();
        if (this == &other) {
            return *this;
        }
        if (other.data)
            other.data->refCount++;
        if (data) {
            destroy();
        }
        data = other.data;
        return *this;
    }

    shared_ptr& operator=(shared_ptr&& other) noexcept {
        SHARED_POINTER_DEBUG_MOVE_ASSIGN();
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
        return data != nullptr && data->ptr != nullptr;
    }

    T* get() const {
        if (!exists()) {
            return nullptr;
        }
        return data->ptr;
    }

    T* operator->() {
        check();
        return (T*) data->ptr;
    }

    T* operator->() const {
        check();
        return (T*) data->ptr;
    }

    T& operator*() {
        check();
        return *(T*) data->ptr;
    }

    T& operator*() const {
        check();
        return *(T*) data->ptr;
    }

    bool operator==(const shared_ptr& other) const {
        return data == other.data;
    }

    bool operator!=(const shared_ptr& other) const {
        return data != other.data;
    }

    operator bool() const {
        return exists();
    }

    friend class weak_ptr<T>;
};

template<typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args) {
    return shared_ptr<T>(new T(std::forward<Args>(args)...));
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
            data = nullptr;
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
        other.data->weakCount++;
        if (data) {
            destroy();
        }
        data = other.data;
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

    operator bool() const {
        return exists();
    }

    shared_ptr<T> lock() const {
        if (expired()) {
            return shared_ptr<T>();
        }
        data->refCount++;
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
    data->refCount++;
    return shared_ptr<U>(data);
}

template<typename T>
class enable_shared_from_this {
protected:
    shared_ptr<T> shared_from_this() const {
        return __self.lock();
    }

    shared_ptr<T> shared_from_this() {
        return __self.lock();
    }

    weak_ptr<T> weak_from_this() const {
        return __self;
    }

    weak_ptr<T> weak_from_this() {
        return __self;
    }

private:
    weak_ptr<T> __self;

    friend class shared_ptr<T>;
    friend class weak_ptr<T>;
};

template<typename T>
weak_ptr<T> weak_ptr_from_shared(const shared_ptr<T>& ptr) {
    return weak_ptr<T>(ptr);
}
