#pragma once
// Minimal SkShaper stub for native tests (no Skia dependency)
#include "include/core/SkFont.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkTextBlob.h"

#include <cstddef>
#include <memory>

class SkShaper {
public:
    virtual ~SkShaper() = default;

    class FontRunIterator {
    public:
        virtual ~FontRunIterator() = default;
        virtual void consume() = 0;
        virtual size_t endOfCurrentRun() const = 0;
        virtual bool atEnd() const = 0;
        virtual const SkFont& currentFont() const = 0;
    };

    class LanguageRunIterator {
    public:
        virtual ~LanguageRunIterator() = default;
        virtual void consume() = 0;
        virtual size_t endOfCurrentRun() const = 0;
        virtual bool atEnd() const = 0;
    };

    class FeatureRunIterator {
    public:
        virtual ~FeatureRunIterator() = default;
        virtual void consume() = 0;
        virtual size_t endOfCurrentRun() const = 0;
        virtual bool atEnd() const = 0;
    };
};
