#pragma once
// Minimal SkSpan stub for native tests
#include <cstddef>
#include <type_traits>
#include <vector>

template <typename T>
class SkSpan {
public:
    SkSpan() : fData(nullptr), fSize(0) {}
    SkSpan(T* data, size_t size) : fData(data), fSize(size) {}
    SkSpan(std::vector<T>& v) : fData(v.data()), fSize(v.size()) {}
    SkSpan(const std::vector<T>& v) : fData(v.data()), fSize(v.size()) {}
    template <size_t N>
    SkSpan(T (&arr)[N]) : fData(arr), fSize(N) {}

    T* data() const { return fData; }
    size_t size() const { return fSize; }
    bool empty() const { return fSize == 0; }

    T& operator[](size_t i) const { return fData[i]; }
    T* begin() const { return fData; }
    T* end() const { return fData + fSize; }

private:
    T* fData;
    size_t fSize;
};
