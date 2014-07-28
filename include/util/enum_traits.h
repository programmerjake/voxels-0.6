#ifndef ENUM_TRAITS_H_INCLUDED
#define ENUM_TRAITS_H_INCLUDED

#include <cstddef>
#include <iterator>
#include <type_traits>

using namespace std;

template <typename T>
struct enum_traits;

template <typename T>
struct enum_iterator : public std::iterator<std::random_access_iterator_tag, T>
{
    T value;
    constexpr enum_iterator()
    {
    }
    constexpr enum_iterator(T value)
        : value(value)
    {
    }
    constexpr const T operator *() const
    {
        return value;
    }
    friend constexpr enum_iterator<T> operator +(ptrdiff_t a, T b)
    {
        return enum_iterator<T>((T)(a + (ptrdiff_t)b));
    }
    constexpr enum_iterator<T> operator +(ptrdiff_t b) const
    {
        return enum_iterator<T>((T)((ptrdiff_t)value + b));
    }
    constexpr enum_iterator<T> operator -(ptrdiff_t b) const
    {
        return enum_iterator<T>((T)((ptrdiff_t)value - b));
    }
    constexpr ptrdiff_t operator -(enum_iterator b) const
    {
        return (ptrdiff_t)value - (ptrdiff_t)b.value;
    }
    constexpr const T operator [](ptrdiff_t b) const
    {
        return (T)((ptrdiff_t)value + b);
    }
    const enum_iterator & operator +=(ptrdiff_t b)
    {
        value = (T)((ptrdiff_t)value + b);
        return *this;
    }
    const enum_iterator & operator -=(ptrdiff_t b)
    {
        value = (T)((ptrdiff_t)value - b);
        return *this;
    }
    const enum_iterator & operator ++()
    {
        value++;
        return *this;
    }
    const enum_iterator & operator --()
    {
        value--;
        return *this;
    }
    enum_iterator operator ++(int)
    {
        return enum_iterator(value++);
    }
    enum_iterator operator --(int)
    {
        return enum_iterator(value--);
    }
    constexpr bool operator ==(enum_iterator b) const
    {
        return value == b.value;
    }
    constexpr bool operator !=(enum_iterator b) const
    {
        return value != b.value;
    }
    constexpr bool operator >=(enum_iterator b) const
    {
        return value >= b.value;
    }
    constexpr bool operator <=(enum_iterator b) const
    {
        return value <= b.value;
    }
    constexpr bool operator >(enum_iterator b) const
    {
        return value > b.value;
    }
    constexpr bool operator <(enum_iterator b) const
    {
        return value < b.value;
    }
};

template <typename T, T minV, T maxV>
struct enum_traits_default
{
    typedef typename std::enable_if<std::is_enum<T>::value, T>::type type;
    typedef typename std::underlying_type<T>::type rwtype;
    static constexpr T minimum = minV;
    static constexpr T maximum = maxV;
    static constexpr enum_iterator<T> begin()
    {
        return enum_iterator<T>(minimum);
    }
    static constexpr enum_iterator<T> end()
    {
        return enum_iterator<T>(maximum) + 1;
    }
    static constexpr size_t size()
    {
        return end() - begin();
    }
};

template <typename T>
struct enum_traits : public enum_traits_default<typename std::enable_if<std::is_enum<T>::value, T>::type, T::enum_first, T::enum_last>
{
};

#define DEFINE_ENUM_LIMITS(first, last) \
enum_first = first, \
enum_last = last,

#endif // ENUM_TRAITS_H_INCLUDED
