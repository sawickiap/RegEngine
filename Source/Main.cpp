#include <string>
#include <cstdarg>

inline size_t tstrlen(const wchar_t* sz) { return wcslen(sz); }
inline void tstrcpy(wchar_t* dst, size_t dstCapacity, const wchar_t* src) { wcscpy_s(dst, dstCapacity, src); }

template<typename CharT>
class str_view_template
{
public:
    typedef CharT CharT;
    typedef std::basic_string<CharT, std::char_traits<CharT>, std::allocator<CharT>> StringT;

    inline str_view_template();    
    inline ~str_view_template();

    inline str_view_template<CharT>& operator=(const str_view_template<CharT>& src);

    inline str_view_template<CharT>& operator=(str_view_template<CharT>&& src);

    inline size_t length() const;

    inline bool empty() const;

    inline const CharT* data() const { return m_Begin; }

    inline const CharT* begin() const { return m_Begin; }

    inline CharT operator[](size_t index) const { return m_Begin[index]; }

    inline const CharT* c_str() const;

private:
    mutable size_t m_Length;
    const CharT* m_Begin;
    mutable const CharT* m_NullTerminatedPtr;
};

typedef str_view_template<char> str_view;
typedef str_view_template<wchar_t> wstr_view;

template<typename CharT>
inline str_view_template<CharT>::str_view_template() :
	m_Length(0),
	m_Begin(nullptr),
	m_NullTerminatedPtr(nullptr)
{
}

template<typename CharT>
inline str_view_template<CharT>::~str_view_template()
{
	if(m_NullTerminatedPtr && m_NullTerminatedPtr != m_Begin)
		delete[] m_NullTerminatedPtr;
}

template<typename CharT>
inline str_view_template<CharT>& str_view_template<CharT>::operator=(const str_view_template<CharT>& src)
{
	if(&src != this)
    {
		if(m_NullTerminatedPtr && m_NullTerminatedPtr != m_Begin)
			delete[] m_NullTerminatedPtr;
		m_Begin = src.m_Begin;
		m_Length = src.m_Length;
		m_NullTerminatedPtr = src.m_NullTerminatedPtr == src.m_Begin ? m_Begin : nullptr;
    }
	return *this;
}

template<typename CharT>
inline str_view_template<CharT>& str_view_template<CharT>::operator=(str_view_template<CharT>&& src)
{
	if(&src != this)
    {
		if(m_NullTerminatedPtr && m_NullTerminatedPtr != m_Begin)
			delete[] m_NullTerminatedPtr;
		m_Begin = src.m_Begin;
		m_Length = src.m_Length;
		m_NullTerminatedPtr = src.m_NullTerminatedPtr;
        src.m_Length = 0;
		src.m_Begin = nullptr;
        src.m_NullTerminatedPtr = nullptr;
    }
	return *this;
}

template<typename CharT>
inline size_t str_view_template<CharT>::length() const
{
    if(m_Length == SIZE_MAX)
    {
        m_Length = tstrlen(m_Begin);
    }
    return m_Length;
}

template<typename CharT>
inline bool str_view_template<CharT>::empty() const
{
    if(m_Length == SIZE_MAX)
    {
        return m_Begin == nullptr || *m_Begin == (CharT)0;
    }
    return m_Length == 0;
}

template<typename CharT>
inline const CharT* str_view_template<CharT>::c_str() const
{
    static const CharT nullChar = (CharT)0;
	if(empty())
		return &nullChar;
	if(m_NullTerminatedPtr == m_Begin)
    {
		return m_Begin;
    }
	if(m_NullTerminatedPtr == nullptr)
    {
        CharT* nullTerminated = new CharT[m_Length + 1];
		memcpy(nullTerminated, begin(), m_Length * sizeof(CharT));
        nullTerminated[m_Length] = (CharT)0;
        m_NullTerminatedPtr = nullTerminated;
    }
	return m_NullTerminatedPtr;
}

void Format(const wstr_view& format, ...)
{
    va_list argList;
    va_start(argList, format.c_str());
    va_end(argList);
}
