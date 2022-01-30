namespace std
{

// Formatters for str_view, wstr_view.
template<>
struct formatter<str_view, char> : formatter<string_view, char>
{
	auto format(const str_view& str, format_context& ctx)
	{
		return formatter<string_view, char>::format(
			string_view(str.data(), str.size()), ctx);
	}
};
template<>
struct formatter<wstr_view, wchar_t> : formatter<wstring_view, wchar_t>
{
	auto format(const wstr_view& str, wformat_context& ctx)
	{
		return formatter<wstring_view, wchar_t>::format(
			wstring_view(str.data(), str.size()), ctx);
	}
};

// Conversion from ANSI str_view to Unicode.
// You must explicitly construct str_view, cannot pass const char* or std::string!
template<>
struct formatter<str_view, wchar_t> : formatter<wstring_view, wchar_t>
{
	auto format(const str_view& str, wformat_context& ctx)
	{
		return formatter<wstring_view, wchar_t>::format(
			ConvertCharsToUnicode(str, CP_ACP), ctx);
	}
};

inline void FormatConstChar(wformat_context& ctx, wchar_t ch)
{
    auto it = ctx.out();
    *it = ch;
    ctx.advance_to(++it);
}

// Formatters for glm::vec2, vec3 etc. of any component count, type, and qualifier (e.g. aligned, packed).
// Output looks like: 1.4343,-10.333,0
template<uint8_t L, typename T, glm::qualifier Q>
struct formatter<glm::vec<L, T, Q>, wchar_t> : formatter<T, wchar_t>
{
    auto format(const glm::vec<L, T, Q>& v, wformat_context& ctx)
    {
        ctx.advance_to(formatter<T, wchar_t>::format(v.x, ctx));
        for(uint8_t i = 1; i < L; ++i)
        {
            FormatConstChar(ctx, L',');
            ctx.advance_to(formatter<T, wchar_t>::format(v[i], ctx));
        }
        return ctx.out();
    }
};

// Formatters for glm::mat4 etc. of any size, type, and qualifier (e.g. aligned, packed).
// Output is row-major and looks like: 1,0,0,1.111;0,1,0,2.335432;0,0,1,3.34234;0,0,0,1
template<uint8_t C, uint8_t R, typename T, glm::qualifier Q>
struct formatter<glm::mat<C, R, T, Q>, wchar_t> : formatter<T, wchar_t>
{
    auto format(const glm::mat<C, R, T, Q>& m, wformat_context& ctx)
    {
        for(uint8_t r = 0; r < R; ++r)
        {
            for(uint8_t c = 0; c < C; ++c)
            {
                if(c > 0)
                    FormatConstChar(ctx, L',');
                else if(r > 0)
                    FormatConstChar(ctx, L';');
                ctx.advance_to(formatter<T, wchar_t>::format(m[c][r], ctx));
            }
        }
        return ctx.out();
    }
};

} // namespace std
