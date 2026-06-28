#pragma once
#include <cstddef>
#include <vector>
template <typename T>
struct SkSpan {
    const T* fPtr = nullptr;
    size_t fCount = 0;
    SkSpan() = default;
    SkSpan(const T* ptr, size_t count) : fPtr(ptr), fCount(count) {}
    template <typename U, typename = std::enable_if_t<std::is_same_v<U, T>>>
    SkSpan(const std::vector<U>& vec) : fPtr(vec.data()), fCount(vec.size()) {}
    const T* data() const { return fPtr; }
    size_t size() const { return fCount; }
    const T& operator[](size_t i) const { return fPtr[i]; }
    const T* begin() const { return fPtr; }
    const T* end() const { return fPtr + fCount; }
};
template <typename U>
SkSpan(const std::vector<U>&) -> SkSpan<U>;
