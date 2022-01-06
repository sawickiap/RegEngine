#include "../ThirdParty/str_view/str_view.hpp"
#include <cstdarg>

std::wstring Format(const wstr_view& format, ...)
{
    va_list argList;
    va_start(argList, format.c_str());
	std::wstring result={};
    va_end(argList);
    return result;
}
