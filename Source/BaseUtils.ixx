module;

#include "BaseUtils.hpp"
#include <cstdarg>

export module BaseUtils;

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

export wstring Format(const wstr_view& format, ...)
{
	auto formatStr = format.c_str();
    va_list argList;
    va_start(argList, formatStr);
    auto result = VFormat(format, argList);
    va_end(argList);
    return result;
}
