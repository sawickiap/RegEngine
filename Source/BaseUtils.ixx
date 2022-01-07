module;

#include "BaseUtils.hpp"
#include <cstdarg>

export module BaseUtils;

export string VFormat(const str_view& format, va_list argList)
{
    const size_t dstLen = (size_t)_vscprintf(format.c_str(), argList);
    if(dstLen)
    {
        std::vector<char> buf(dstLen + 1);
        vsprintf_s(&buf[0], dstLen + 1, format.c_str(), argList);
        return string{buf.data(), buf.data() + dstLen};
    }
    else
        return {};
}
export wstring VFormat(const wstr_view& format, va_list argList)
{
    const size_t dstLen = (size_t)_vscwprintf(format.c_str(), argList);
    if(dstLen)
    {
        std::vector<wchar_t> buf(dstLen + 1);
        vswprintf_s(&buf[0], dstLen + 1, format.c_str(), argList);
        return wstring{buf.data(), buf.data() + dstLen};
    }
    else
        return {};
}

export string Format(const str_view& format, ...)
{
	auto formatStr = format.c_str();
    va_list argList;
    va_start(argList, formatStr);
    auto result = VFormat(format, argList);
    va_end(argList);
    return result;
}
export wstring Format(const wstr_view& format, ...)
{
	auto formatStr = format.c_str();
    va_list argList;
    va_start(argList, formatStr);
    auto result = VFormat(format, argList);
    va_end(argList);
    return result;
}
