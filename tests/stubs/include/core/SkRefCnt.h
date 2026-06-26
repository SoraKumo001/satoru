#pragma once
// Minimal SkRefCnt / sk_sp stubs for native tests (no Skia dependency)
#include <atomic>
#include <cstddef>
#include <utility>

class SkRefCnt {
public:
    SkRefCnt() : fRefCnt(1) {}
    virtual ~SkRefCnt() = default;
    void ref() const { ++fRefCnt; }
    void unref() const {
        if (--fRefCnt == 0) delete this;
    }
    bool unique() const { return fRefCnt == 1; }
protected:
    mutable std::atomic<int> fRefCnt;
};

template <typename T> class sk_sp {
public:
    sk_sp() : fPtr(nullptr) {}
    sk_sp(std::nullptr_t) : fPtr(nullptr) {}
    sk_sp(T* p) : fPtr(p) { if (fPtr) fPtr->ref(); }
    sk_sp(const sk_sp& o) : fPtr(o.fPtr) { if (fPtr) fPtr->ref(); }
    sk_sp(sk_sp&& o) noexcept : fPtr(o.fPtr) { o.fPtr = nullptr; }
    ~sk_sp() { if (fPtr) fPtr->unref(); }

    sk_sp& operator=(std::nullptr_t) { reset(); return *this; }
    sk_sp& operator=(const sk_sp& o) {
        if (o.fPtr) o.fPtr->ref();
        if (fPtr) fPtr->unref();
        fPtr = o.fPtr;
        return *this;
    }
    sk_sp& operator=(sk_sp&& o) noexcept {
        if (fPtr) fPtr->unref();
        fPtr = o.fPtr;
        o.fPtr = nullptr;
        return *this;
    }

    T* get() const { return fPtr; }
    T* operator->() const { return fPtr; }
    T& operator*() const { return *fPtr; }
    explicit operator bool() const { return fPtr != nullptr; }
    bool operator==(const sk_sp& o) const { return fPtr == o.fPtr; }
    bool operator!=(const sk_sp& o) const { return fPtr != o.fPtr; }
    bool operator==(std::nullptr_t) const { return fPtr == nullptr; }
    bool operator!=(std::nullptr_t) const { return fPtr != nullptr; }

    void reset(T* p = nullptr) {
        if (fPtr) fPtr->unref();
        fPtr = p;
    }
    T* release() { T* p = fPtr; fPtr = nullptr; return p; }
    void swap(sk_sp& o) noexcept { std::swap(fPtr, o.fPtr); }

    template <typename U>
    sk_sp(const sk_sp<U>& o) : fPtr(static_cast<T*>(o.get())) { if (fPtr) fPtr->ref(); }

private:
    T* fPtr;
};

template <typename T, typename... Args>
sk_sp<T> sk_make_sp(Args&&... args) {
    return sk_sp<T>(new T(std::forward<Args>(args)...));
}
