#ifndef CACHED_VARIABLE_H_INCLUDED
#define CACHED_VARIABLE_H_INCLUDED

#include <atomic>
#include <cstdint>

template <typename T>
class CachedVariable
{
    std::atomic_uint_fast8_t viewIndex;
    T views[2];
public:
    CachedVariable()
        : viewIndex(0)
    {
    }
    const T & read() const
    {
        return views[viewIndex ? 1 : 0];
    }
    T & writeRef()
    {
        return views[viewIndex ? 0 : 1];
    }
    void finishWrite()
    {
        viewIndex ^= 1;
    }
    void write(const T & value)
    {
        writeRef() = value;
        finishWrite();
    }
};

#endif // CACHED_VARIABLE_H_INCLUDED
