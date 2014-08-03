#ifndef STRING_CAST_H_INCLUDED
#define STRING_CAST_H_INCLUDED

#include <cwchar>
#include <string>
#include <cstring>

using namespace std;

template <typename T>
T string_cast(string);

template <typename T>
T string_cast(wstring);

template <>
inline string string_cast<string>(string str)
{
    return str;
}

template <>
inline wstring string_cast<wstring>(wstring str)
{
    return str;
}

template <>
inline string string_cast<string>(wstring wstr)
{
    size_t destLen = wstr.length() * 4 + 1 + 32/*for extra buffer space*/;
    char *str = new char[destLen];
    for(size_t i = 0; i < destLen; i++)
        str[i] = 0;
    const wchar_t *ptr = wstr.c_str();
    mbstate_t mbstate;
    memset((void *)&mbstate, 0, sizeof(mbstate));
    size_t v = wcsrtombs(str, &ptr, destLen - 1, &mbstate);

    if(v == (size_t) - 1)
    {
        delete []str;
        throw runtime_error("can't convert wide character string to multi-byte string");
    }

    str[v] = '\0';
    string retval = str;
    delete []str;
    return retval;
}

template <>
inline wstring string_cast<wstring>(string str)
{
    size_t destLen = str.length() + 1 + 32/* for extra buffer space*/;
    wchar_t *wstr = new wchar_t[destLen];
    for(size_t i = 0; i < destLen; i++)
        wstr[i] = 0;
    const char *ptr = str.c_str();
    mbstate_t mbstate;
    memset((void *)&mbstate, 0, sizeof(mbstate));
    size_t v = mbsrtowcs(wstr, &ptr, destLen - 1, &mbstate);

    if(v == (size_t) - 1)
    {
        delete []wstr;
        throw runtime_error("can't convert multi-byte string to wide character string");
    }

    wstr[v] = '\0';
    wstring retval = wstr;
    delete []wstr;
    return retval;
}

#endif // STRING_CAST_H_INCLUDED
