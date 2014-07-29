/*
 * Voxels is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Voxels is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Voxels; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */
#ifndef STREAM_H
#define STREAM_H

#include <cstdint>
#include <stdexcept>
#include <cwchar>
#include <string>
#include <cstdio>
#include <cstring>
#include <memory>
#include <list>
#include <cmath>
#include <functional>
#include <type_traits>
#include <cassert>
#include "util/string_cast.h"
#include "util/enum_traits.h"
#ifdef DEBUG_STREAM
#include <iostream>
#define DUMP_V(fn, v) do {cerr << #fn << ": read " << v << endl;} while(false)
#else
#define DUMP_V(fn, v)
#endif

using namespace std;

class IOException : public runtime_error
{
public:
    explicit IOException(const string & msg)
        : runtime_error(msg)
    {
    }
    explicit IOException(exception * e, bool deleteIt = true)
        : runtime_error((dynamic_cast<IOException *>(e) == nullptr) ? string("IO Error : ") + e->what() : string(e->what()))
    {
        if(deleteIt)
            delete e;
    }
    explicit IOException(exception & e, bool deleteIt = false)
        : IOException(&e, deleteIt)
    {
    }
};

class EOFException final : public IOException
{
public:
    explicit EOFException()
        : IOException("IO Error : reached end of file")
    {
    }
};

class NoStreamsLeftException final : public IOException
{
public:
    explicit NoStreamsLeftException()
        : IOException("IO Error : no streams left")
    {
    }
};

class UTFDataFormatException final : public IOException
{
public:
    explicit UTFDataFormatException()
        : IOException("IO Error : invalid UTF data")
    {
    }
};

class InvalidDataValueException final : public IOException
{
public:
    explicit InvalidDataValueException(string msg)
        : IOException(msg)
    {
    }
};

typedef float float32_t;
typedef double float64_t;

class Reader
{
private:
    template <typename T>
    static T limitAfterRead(T v, T min, T max)
    {
        if(v < min || v > max)
        {
            throw InvalidDataValueException("read value out of range : " + to_string(v));
        }
        return v;
    }
public:
    Reader()
    {
    }
    Reader(const Reader &) = delete;
    const Reader & operator =(const Reader &) = delete;
    virtual ~Reader()
    {
    }
    virtual uint8_t readByte() = 0;
    void readBytes(uint8_t * array, size_t count)
    {
        for(size_t i = 0; i < count; i++)
        {
            array[i] = readByte();
        }
    }
    uint8_t readU8()
    {
        uint8_t retval = readByte();
        DUMP_V(readU8, (unsigned)retval);
        return retval;
    }
    int8_t readS8()
    {
        int8_t retval = readByte();
        DUMP_V(readS8, (int)retval);
        return retval;
    }
    uint16_t readU16()
    {
        uint16_t v = readU8();
        uint16_t retval = (v << 8) | readU8();
        DUMP_V(readU16, retval);
        return retval;
    }
    int16_t readS16()
    {
        int16_t retval = readU16();
        DUMP_V(readS16, retval);
        return retval;
    }
    uint32_t readU32()
    {
        uint32_t v = readU16();
        uint32_t retval = (v << 16) | readU16();
        DUMP_V(readU32, retval);
        return retval;
    }
    int32_t readS32()
    {
        int32_t retval = readU32();
        DUMP_V(readS32, retval);
        return retval;
    }
    uint64_t readU64()
    {
        uint64_t v = readU32();
        uint64_t retval = (v << 32) | readU32();
        DUMP_V(readU64, retval);
        return retval;
    }
    int64_t readS64()
    {
        int64_t retval = readU64();
        DUMP_V(readS64, retval);
        return retval;
    }
    float32_t readF32()
    {
        static_assert(sizeof(float32_t) == sizeof(uint32_t), "float32_t is not 32 bits");
        union
        {
            uint32_t ival;
            float32_t fval;
        };
        ival = readU32();
        float32_t retval = fval;
        DUMP_V(readF32, retval);
        return retval;
    }
    float64_t readF64()
    {
        static_assert(sizeof(float64_t) == sizeof(uint64_t), "float64_t is not 64 bits");
        union
        {
            uint64_t ival;
            float64_t fval;
        };
        ival = readU64();
        float64_t retval = fval;
        DUMP_V(readF64, retval);
        return retval;
    }
    bool readBool()
    {
        return readU8() != 0;
    }
    wstring readString()
    {
        wstring retval = L"";
        for(;;)
        {
            uint32_t b1 = readU8();
            if(b1 == 0)
            {
                DUMP_V(readString, "\"" + wcsrtombs(retval) + "\"");
                return retval;
            }
            else if((b1 & 0x80) == 0)
            {
                retval += (wchar_t)b1;
            }
            else if((b1 & 0xE0) == 0xC0)
            {
                uint32_t b2 = readU8();
                if((b2 & 0xC0) != 0x80)
                    throw UTFDataFormatException();
                uint32_t v = b2 & 0x3F;
                v |= (b1 & 0x1F) << 6;
                retval += (wchar_t)v;
            }
            else if((b1 & 0xF0) == 0xE0)
            {
                uint32_t b2 = readU8();
                if((b2 & 0xC0) != 0x80)
                    throw UTFDataFormatException();
                uint32_t b3 = readU8();
                if((b3 & 0xC0) != 0x80)
                    throw UTFDataFormatException();
                uint32_t v = b3 & 0x3F;
                v |= (b2 & 0x3F) << 6;
                v |= (b1 & 0xF) << 12;
                retval += (wchar_t)v;
            }
            else if((b1 & 0xF8) == 0xF0)
            {
                uint32_t b2 = readU8();
                if((b2 & 0xC0) != 0x80)
                    throw UTFDataFormatException();
                uint32_t b3 = readU8();
                if((b3 & 0xC0) != 0x80)
                    throw UTFDataFormatException();
                uint32_t b4 = readU8();
                if((b4 & 0xC0) != 0x80)
                    throw UTFDataFormatException();
                uint32_t v = b4 & 0x3F;
                v |= (b3 & 0x3F) << 6;
                v |= (b2 & 0x3F) << 12;
                v |= (b1 & 0x7) << 18;
                if(v >= 0x10FFFF)
                    throw UTFDataFormatException();
                retval += (wchar_t)v;
            }
            else
                throw UTFDataFormatException();
        }
    }
    uint8_t readLimitedU8(uint8_t min, uint8_t max)
    {
        return limitAfterRead(readU8(), min, max);
    }
    int8_t readLimitedS8(int8_t min, int8_t max)
    {
        return limitAfterRead(readS8(), min, max);
    }
    uint16_t readLimitedU16(uint16_t min, uint16_t max)
    {
        return limitAfterRead(readU16(), min, max);
    }
    int16_t readLimitedS16(int16_t min, int16_t max)
    {
        return limitAfterRead(readS16(), min, max);
    }
    uint32_t readLimitedU32(uint32_t min, uint32_t max)
    {
        return limitAfterRead(readU32(), min, max);
    }
    int32_t readLimitedS32(int32_t min, int32_t max)
    {
        return limitAfterRead(readS32(), min, max);
    }
    uint64_t readLimitedU64(uint64_t min, uint64_t max)
    {
        return limitAfterRead(readU64(), min, max);
    }
    int64_t readLimitedS64(int64_t min, int64_t max)
    {
        return limitAfterRead(readS64(), min, max);
    }
    float32_t readFiniteF32()
    {
        float32_t retval = readF32();
        if(!isfinite(retval))
        {
            throw InvalidDataValueException("read value is not finite");
        }
        return retval;
    }
    float64_t readFiniteF64()
    {
        float64_t retval = readF64();
        if(!isfinite(retval))
        {
            throw InvalidDataValueException("read value is not finite");
        }
        return retval;
    }
    float32_t readLimitedF32(float32_t min, float32_t max)
    {
        return limitAfterRead(readFiniteF32(), min, max);
    }
    float64_t readLimitedF64(float64_t min, float64_t max)
    {
        return limitAfterRead(readFiniteF64(), min, max);
    }
};

class Writer
{
public:
    Writer()
    {
    }
    Writer(const Writer &) = delete;
    const Writer & operator =(const Writer &) = delete;
    virtual ~Writer()
    {
    }
    virtual void writeByte(uint8_t) = 0;
    virtual void flush()
    {
    }
    void writeBytes(const uint8_t * array, size_t count)
    {
        for(size_t i = 0; i < count; i++)
            writeByte(array[i]);
    }
    void writeU8(uint8_t v)
    {
        writeByte(v);
    }
    void writeS8(int8_t v)
    {
        writeByte(v);
    }
    void writeU16(uint16_t v)
    {
        writeU8((uint8_t)(v >> 8));
        writeU8((uint8_t)(v & 0xFF));
    }
    void writeS16(int16_t v)
    {
        writeU16(v);
    }
    void writeU32(uint32_t v)
    {
        writeU16((uint16_t)(v >> 16));
        writeU16((uint16_t)(v & 0xFFFF));
    }
    void writeS32(int32_t v)
    {
        writeU32(v);
    }
    void writeU64(uint64_t v)
    {
        writeU32((uint64_t)(v >> 32));
        writeU32((uint64_t)(v & 0xFFFFFFFFU));
    }
    void writeS64(int64_t v)
    {
        writeU64(v);
    }
    void writeF32(float32_t v)
    {
        static_assert(sizeof(float32_t) == sizeof(uint32_t), "float is not 32 bits");
        union
        {
            uint32_t ival;
            float32_t fval;
        };
        fval = v;
        writeU32(ival);
    }
    void writeF64(float64_t v)
    {
        static_assert(sizeof(float64_t) == sizeof(uint64_t), "double is not 64 bits");
        union
        {
            uint64_t ival;
            float64_t fval;
        };
        fval = v;
        writeU64(ival);
    }
    void writeBool(bool v)
    {
        writeU8(v ? 1 : 0);
    }
    void writeString(wstring v)
    {
        for(size_t i = 0; i < v.length(); i++)
        {
            uint32_t ch = v[i];
            if(ch != 0 && ch < 0x80)
            {
                writeU8(ch);
            }
            else if(ch < 0x800)
            {
                writeU8(0xC0 | ((ch >> 6) & 0x1F));
                writeU8(0x80 | ((ch) & 0x3F));
            }
            else if(ch < 0x1000)
            {
                writeU8(0xE0 | ((ch >> 12) & 0xF));
                writeU8(0x80 | ((ch >> 6) & 0x3F));
                writeU8(0x80 | ((ch) & 0x3F));
            }
            else
            {
                writeU8(0xF0 | ((ch >> 18) & 0x7));
                writeU8(0x80 | ((ch >> 12) & 0x3F));
                writeU8(0x80 | ((ch >> 6) & 0x3F));
                writeU8(0x80 | ((ch) & 0x3F));
            }
        }
        writeU8(0);
    }
};

template <typename ReturnType>
struct read_base
{
    read_base(const read_base &) = delete;
    const read_base & operator =(const read_base &) = delete;
protected:
    ReturnType value;
    read_base(ReturnType && value)
        : value(value)
    {
    }
public:
    operator ReturnType &&() &&
    {
        return std::move(value);
    }
};

struct VariableSet;

template <typename T>
struct read
{
    read(Reader &reader) = delete;
    read(Reader &reader, VariableSet &variableSet) = delete;
};

template <typename T>
struct is_value_modified
{
    constexpr bool operator ()(const T &) const
    {
        return false;
    }
};

template <typename T>
struct write
{
    template <typename VariableType>
    write(Writer &writer, VariableType value) = delete;
    template <typename VariableType>
    write(Writer &writer, VariableSet &variableSet, VariableType value) = delete;
};

template <typename T>
struct rw_class_traits_helper_has_read_with_VariableSet
{
    static constexpr bool value = false;
};

template <typename T>

template <typename T>
struct rw_class_traits

template <typename T, typename = std::enable_if<std::is_class<T>::value>::type>
struct read<T> : public read_base<T>
{
    read(Reader &reader)
        : read_base<T>(T::read(reader))
    {
    }
}

template <typename T, typename std::enable_if<std::is_class<T>::value, int>::type = 0>
inline void write(Writer &writer, const T &value)
{
    value.write(writer);
}

template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
inline T read_finite(Reader & reader)
{
    return read<T>(reader);
}

template <typename T>
inline T read_checked(Reader & reader, function<bool(T)> checkFn)
{
    T retval = read<T>(reader);
    if(!checkFn(retval))
    {
        throw InvalidDataValueException("check failed in read_checked");
    }
    return retval;
}

#define DEFINE_RW_FUNCTIONS_FOR_BASIC_INTEGER_TYPE(typeName, functionSuffix) \
template <typename T, typename std::enable_if<std::is_same<T, typeName>::value, int>::type = 0> \
inline typeName read(Reader & reader) \
{ \
    return reader.read ## functionSuffix(); \
} \
template <typename T, typename std::enable_if<std::is_same<T, typeName>::value, int>::type = 0> \
inline typeName read_limited(Reader & reader, typeName minV, typeName maxV) \
{ \
    return reader.readLimited ## functionSuffix(minV, maxV); \
} \
template <typename T, typename std::enable_if<std::is_same<T, typeName>::value, int>::type = 0> \
inline void write(Writer & writer, typeName value) \
{ \
    writer.write ## functionSuffix(value); \
}

#define DEFINE_RW_FUNCTIONS_FOR_BASIC_FLOAT_TYPE(typeName, functionSuffix) \
template <typename T, typename std::enable_if<std::is_same<T, typeName>::value, int>::type = 0> \
inline typeName read(Reader & reader) \
{ \
    return reader.read ## functionSuffix(); \
} \
template <typename T, typename std::enable_if<std::is_same<T, typeName>::value, int>::type = 0> \
inline typeName read_finite(Reader & reader) \
{ \
    return reader.readFinite ## functionSuffix(); \
} \
template <typename T, typename std::enable_if<std::is_same<T, typeName>::value, int>::type = 0> \
inline typeName read_limited(Reader & reader, typeName minV, typeName maxV) \
{ \
    return reader.readLimited ## functionSuffix(minV, maxV); \
} \
template <typename T, typename std::enable_if<std::is_same<T, typeName>::value, int>::type = 0> \
inline void write(Writer & writer, typeName value) \
{ \
    writer.write ## functionSuffix(value); \
}

#define DEFINE_RW_FUNCTIONS_FOR_BASIC_TYPE(typeName, functionSuffix) \
template <typename T, typename std::enable_if<std::is_same<T, typeName>::value, int>::type = 0> \
inline typeName read(Reader & reader) \
{ \
    return reader.read ## functionSuffix(); \
} \
template <typename T, typename std::enable_if<std::is_same<T, typeName>::value, int>::type = 0> \
inline void write(Writer & writer, typeName value) \
{ \
    writer.write ## functionSuffix(value); \
}

DEFINE_RW_FUNCTIONS_FOR_BASIC_INTEGER_TYPE(uint8_t, U8)
DEFINE_RW_FUNCTIONS_FOR_BASIC_INTEGER_TYPE(int8_t, S8)
DEFINE_RW_FUNCTIONS_FOR_BASIC_INTEGER_TYPE(uint16_t, U16)
DEFINE_RW_FUNCTIONS_FOR_BASIC_INTEGER_TYPE(int16_t, S16)
DEFINE_RW_FUNCTIONS_FOR_BASIC_INTEGER_TYPE(uint32_t, U32)
DEFINE_RW_FUNCTIONS_FOR_BASIC_INTEGER_TYPE(int32_t, S32)
DEFINE_RW_FUNCTIONS_FOR_BASIC_INTEGER_TYPE(uint64_t, U64)
DEFINE_RW_FUNCTIONS_FOR_BASIC_INTEGER_TYPE(int64_t, S64)
DEFINE_RW_FUNCTIONS_FOR_BASIC_FLOAT_TYPE(float32_t, F32)
DEFINE_RW_FUNCTIONS_FOR_BASIC_FLOAT_TYPE(float64_t, F64)
DEFINE_RW_FUNCTIONS_FOR_BASIC_TYPE(wstring, String)
DEFINE_RW_FUNCTIONS_FOR_BASIC_TYPE(bool, Bool)

#undef DEFINE_RW_FUNCTIONS_FOR_BASIC_INTEGER_TYPE
#undef DEFINE_RW_FUNCTIONS_FOR_BASIC_FLOAT_TYPE
#undef DEFINE_RW_FUNCTIONS_FOR_BASIC_TYPE

template <typename T, typename std::enable_if<std::is_enum<T>::value, int>::type = 0>
inline T read(Reader &reader)
{
    return (T)read_limited<typename enum_traits<T>::rwtype>(reader, (typename enum_traits<T>::rwtype)enum_traits<T>::minimum, (typename enum_traits<T>::rwtype)enum_traits<T>::maximum);
}

template <typename T, typename std::enable_if<std::is_enum<T>::value, int>::type = 0>
inline void write(Writer &writer, T value)
{
    write<typename enum_traits<T>::rwtype>(writer, (typename enum_traits<T>::rwtype)value);
}

class FileReader final : public Reader
{
private:
    FILE * f;
public:
    FileReader(wstring fileName)
    {
        string str = string_cast<string>(fileName);
        f = fopen(str.c_str(), "rb");
        if(f == nullptr)
            throw IOException(string("IO Error : ") + strerror(errno));
    }
    explicit FileReader(FILE * f)
        : f(f)
    {
        assert(f != nullptr);
    }
    virtual ~FileReader()
    {
        fclose(f);
    }
    virtual uint8_t readByte() override
    {
        int ch = fgetc(f);
        if(ch == EOF)
        {
            if(ferror(f))
                throw IOException("IO Error : can't read from file");
            throw EOFException();
        }
        return ch;
    }
};

class FileWriter final : public Writer
{
private:
    FILE * f;
public:
    FileWriter(wstring fileName)
    {
        string str = string_cast<string>(fileName);
        f = fopen(str.c_str(), "wb");
        if(f == nullptr)
            throw IOException(string("IO Error : ") + strerror(errno));
    }
    explicit FileWriter(FILE * f)
        : f(f)
    {
        assert(f != nullptr);
    }
    virtual ~FileWriter()
    {
        fclose(f);
    }
    virtual void writeByte(uint8_t v) override
    {
        if(fputc(v, f) == EOF)
            throw IOException("IO Error : can't write to file");
    }
    virtual void flush() override
    {
        if(EOF == fflush(f))
            throw IOException("IO Error : can't write to file");
    }
};

class MemoryReader final : public Reader
{
private:
    const shared_ptr<const uint8_t> mem;
    size_t offset;
    const size_t length;
public:
    explicit MemoryReader(shared_ptr<const uint8_t> mem, size_t length)
        : mem(mem), offset(0), length(length)
    {
    }
    template <size_t length>
    explicit MemoryReader(const uint8_t a[length])
        : MemoryReader(shared_ptr<const uint8_t>(&a[0], [](const uint8_t *){}))
    {
    }
    virtual uint8_t readByte() override
    {
        if(offset >= length)
            throw EOFException();
        return mem.get()[offset++];
    }
};

class StreamPipe final
{
    StreamPipe(const StreamPipe &) = delete;
    const StreamPipe & operator =(const StreamPipe &) = delete;
private:
    shared_ptr<Reader> readerInternal;
    shared_ptr<Writer> writerInternal;
public:
    StreamPipe();
    Reader & reader()
    {
        return *readerInternal;
    }
    Writer & writer()
    {
        return *writerInternal;
    }
    shared_ptr<Reader> preader()
    {
        return readerInternal;
    }
    shared_ptr<Writer> pwriter()
    {
        return writerInternal;
    }
};

class DumpingReader final : public Reader
{
private:
    Reader &reader;
public:
    DumpingReader(Reader& reader)
        : reader(reader)
    {
    }
    virtual uint8_t readByte() override;
};

struct StreamRW
{
    StreamRW()
    {
    }
    StreamRW(const StreamRW &) = delete;
    const StreamRW & operator =(const StreamRW &) = delete;
    virtual ~StreamRW()
    {
    }
    Reader & reader()
    {
        return *preader();
    }
    Writer & writer()
    {
        return *pwriter();
    }
    virtual shared_ptr<Reader> preader() = 0;
    virtual shared_ptr<Writer> pwriter() = 0;
};

class StreamRWWrapper final : public StreamRW
{
private:
    shared_ptr<Reader> preaderInternal;
    shared_ptr<Writer> pwriterInternal;
public:
    StreamRWWrapper(shared_ptr<Reader> preaderInternal, shared_ptr<Writer> pwriterInternal)
        : preaderInternal(preaderInternal), pwriterInternal(pwriterInternal)
    {
    }

    virtual shared_ptr<Reader> preader() override
    {
        return preaderInternal;
    }

    virtual shared_ptr<Writer> pwriter() override
    {
        return pwriterInternal;
    }
};

class StreamBidirectionalPipe final
{
private:
    StreamPipe pipe1, pipe2;
    shared_ptr<StreamRW> port1Internal, port2Internal;
public:
    StreamBidirectionalPipe()
    {
        port1Internal = shared_ptr<StreamRW>(new StreamRWWrapper(pipe1.preader(), pipe2.pwriter()));
        port2Internal = shared_ptr<StreamRW>(new StreamRWWrapper(pipe2.preader(), pipe1.pwriter()));
    }
    shared_ptr<StreamRW> pport1()
    {
        return port1Internal;
    }
    shared_ptr<StreamRW> pport2()
    {
        return port2Internal;
    }
    StreamRW & port1()
    {
        return *port1Internal;
    }
    StreamRW & port2()
    {
        return *port2Internal;
    }
};

struct StreamServer
{
    StreamServer()
    {
    }
    StreamServer(const StreamServer &) = delete;
    const StreamServer & operator =(const StreamServer &) = delete;
    virtual ~StreamServer()
    {
    }
    virtual shared_ptr<StreamRW> accept() = 0;
};

class StreamServerWrapper final : public StreamServer
{
private:
    list<shared_ptr<StreamRW>> streams;
    shared_ptr<StreamServer> nextServer;
public:
    StreamServerWrapper(list<shared_ptr<StreamRW>> streams, shared_ptr<StreamServer> nextServer = nullptr)
        : streams(streams), nextServer(nextServer)
    {
    }
    virtual shared_ptr<StreamRW> accept() override
    {
        if(streams.empty())
        {
            if(nextServer == nullptr)
                throw NoStreamsLeftException();
            return nextServer->accept();
        }
        shared_ptr<StreamRW> retval = streams.front();
        streams.pop_front();
        return retval;
    }
};

#endif // STREAM_H
