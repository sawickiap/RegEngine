/*
Define this macro to enable C++17 compatibility.
*/
#ifndef STR_VIEW_CPP17
    #if defined(_MSVC_LANG) && _MSVC_LANG >= 201703L
        #define STR_VIEW_CPP17 1
    #else
        #define STR_VIEW_CPP17 0
    #endif
#endif

#include <string>
#include <algorithm> // for min, max
#include <memory> // for memcmp
#if STR_VIEW_CPP17
    #include <string_view>
#endif

#include <cassert>
#include <cstring>
#include <cstdint>
#include <cstdarg>

inline size_t tstrlen(const char* sz) { return strlen(sz); }
inline size_t tstrlen(const wchar_t* sz) { return wcslen(sz); }
inline void tstrcpy(char* dst, size_t dstCapacity, const char* src) { strcpy_s(dst, dstCapacity, src); }
inline void tstrcpy(wchar_t* dst, size_t dstCapacity, const wchar_t* src) { wcscpy_s(dst, dstCapacity, src); }
inline int tstrncmp(const char* lhs, const char* rhs, size_t count) { return strncmp(lhs, rhs, count); }
inline int tstrncmp(const wchar_t* lhs, const wchar_t* rhs, size_t count) { return wcsncmp(lhs, rhs, count); }
inline int tstrnicmp(const char* lhs, const char* rhs, size_t count) { return _strnicmp(lhs, rhs, count); }
inline int tstrnicmp(const wchar_t* lhs, const wchar_t* rhs, size_t count) { return _wcsnicmp(lhs, rhs, count); }

template<typename CharT>
class str_view_template
{
public:
    typedef CharT CharT;
    typedef std::basic_string<CharT, std::char_traits<CharT>, std::allocator<CharT>> StringT;
#if STR_VIEW_CPP17
    typedef std::basic_string_view<CharT, std::char_traits<CharT>> StringViewT;
#endif

    inline str_view_template();
    
    inline str_view_template(const CharT* sz);
    inline str_view_template(const CharT* str, size_t length);
    template<size_t Length>
    inline str_view_template(const CharT str[Length]);
    struct StillNullTerminated { };
    inline str_view_template(const CharT* str, size_t length, StillNullTerminated);
    
    inline str_view_template(const StringT& str, size_t offset = 0, size_t length = SIZE_MAX);
#if STR_VIEW_CPP17
    inline str_view_template(const StringViewT& str, size_t offset = 0, size_t length = SIZE_MAX);
#endif

    inline str_view_template(const str_view_template<CharT>& src, size_t offset = 0, size_t length = SIZE_MAX);
    inline str_view_template(str_view_template<CharT>&& src);
    
    inline ~str_view_template();

    inline str_view_template<CharT>& operator=(const str_view_template<CharT>& src);

    inline str_view_template<CharT>& operator=(str_view_template<CharT>&& src);

    inline void swap(str_view_template<CharT>& rhs) noexcept;


    inline size_t length() const;

    inline size_t size() const { return length(); }

    inline bool empty() const;

    inline const CharT* data() const { return m_Begin; }

    inline const CharT* begin() const { return m_Begin; }

    inline const CharT* end() const { return m_Begin + length(); }

    inline const CharT front() const { return *m_Begin; }

    inline const CharT back() const { return m_Begin[length() - 1]; }
    
    inline CharT operator[](size_t index) const { return m_Begin[index]; }
    inline CharT at(size_t index) const { return m_Begin[index]; }

    inline const CharT* c_str() const;

    inline str_view_template<CharT> substr(size_t offset = 0, size_t length = SIZE_MAX) const;

    inline size_t copy_to(CharT* dst, size_t offset = 0, size_t length = SIZE_MAX) const;

    inline void to_string(StringT& dst, size_t offset = 0, size_t length = SIZE_MAX) const;

#if STR_VIEW_CPP17
    inline void to_string_view(StringViewT& dst, size_t offset = 0, size_t length = SIZE_MAX) const;
#endif

    inline int compare(const str_view_template<CharT>& rhs, bool case_sensitive = true) const;

    inline bool operator==(const str_view_template<CharT>& rhs) const { return compare(rhs) == 0; }
    inline bool operator!=(const str_view_template<CharT>& rhs) const { return compare(rhs) != 0; }
    inline bool operator< (const str_view_template<CharT>& rhs) const { return compare(rhs) <  0; }
    inline bool operator> (const str_view_template<CharT>& rhs) const { return compare(rhs) >  0; }
    inline bool operator<=(const str_view_template<CharT>& rhs) const { return compare(rhs) <= 0; }
    inline bool operator>=(const str_view_template<CharT>& rhs) const { return compare(rhs) >= 0; }

	inline bool starts_with(CharT prefix, bool case_sensitive = true) const;
    inline bool starts_with(const str_view_template<CharT>& prefix, bool case_sensitive = true) const;

    inline bool ends_with(CharT suffix, bool case_sensitive = true) const;
    inline bool ends_with(const str_view_template<CharT>& suffix, bool case_sensitive = true) const;


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
inline str_view_template<CharT>::str_view_template(const CharT* sz) :
	m_Length(sz ? SIZE_MAX : 0),
	m_Begin(sz),
	m_NullTerminatedPtr(sz ? sz : nullptr)
{
}

template<typename CharT>
inline str_view_template<CharT>::str_view_template(const CharT* str, size_t length) :
	m_Length(length),
	m_Begin(length ? str : nullptr),
	m_NullTerminatedPtr(nullptr)
{
}

template<typename CharT>
template<size_t Length>
inline str_view_template<CharT>::str_view_template(const CharT str[Length]) :
    m_Length(Length),
    m_Begin(str),
    m_NullTerminatedPtr(nullptr)
{
}

template<typename CharT>
inline str_view_template<CharT>::str_view_template(const CharT* str, size_t length, StillNullTerminated) :
	m_Length(length),
	m_Begin(nullptr),
	m_NullTerminatedPtr(nullptr)
{
    if(length)
    {
        m_Begin = str;
        m_NullTerminatedPtr = str;
    }
    assert(m_Begin[m_Length] == (CharT)0); // Make sure it's really null terminated.
}

template<typename CharT>
inline str_view_template<CharT>::str_view_template(const StringT& str, size_t offset, size_t length) :
	m_Length(0),
	m_Begin(nullptr),
	m_NullTerminatedPtr(nullptr)
{
	assert(offset <= str.length());
    m_Length = std::min(length, str.length() - offset);
    if(m_Length)
    {
        if(m_Length == str.length() - offset)
        {
            m_Begin = str.c_str() + offset;
            m_NullTerminatedPtr = m_Begin;
        }
        else
            m_Begin = str.data() + offset;
    }
}

#if STR_VIEW_CPP17

template<typename CharT>
inline str_view_template<CharT>::str_view_template(const StringViewT& str, size_t offset, size_t length) :
    m_Length(0),
    m_Begin(nullptr),
    m_NullTerminatedPtr(nullptr)
{
    assert(offset <= str.length());
    m_Length = std::min(length, str.length() - offset);
    if(m_Length)
        m_Begin = str.data() + offset;
}

#endif // #if STR_VIEW_CPP17

template<typename CharT>
inline str_view_template<CharT>::str_view_template(const str_view_template<CharT>& src, size_t offset, size_t length) :
	m_Length(0),
	m_Begin(nullptr),
	m_NullTerminatedPtr(nullptr)
{
    // Source length is unknown, constructor doesn't limit the length - it may remain unknown.
    if(src.m_Length == SIZE_MAX && length == SIZE_MAX)
    {
        m_Length = SIZE_MAX;
        m_Begin = src.m_Begin + offset;
        assert(src.m_NullTerminatedPtr == src.m_Begin);
        m_NullTerminatedPtr = m_Begin;
    }
    else
    {
        const size_t srcLen = src.length();
	    assert(offset <= srcLen);
        m_Length = std::min(length, srcLen - offset);
        if(m_Length)
        {
            m_Begin = src.m_Begin + offset;
            if(src.m_NullTerminatedPtr == src.m_Begin && m_Length == srcLen - offset)
                m_NullTerminatedPtr = m_Begin;
        }
    }
}

template<typename CharT>
inline str_view_template<CharT>::str_view_template(str_view_template<CharT>&& src) :
	m_Length(src.m_Length),
	m_Begin(src.m_Begin),
	m_NullTerminatedPtr(src.m_NullTerminatedPtr)
{
    src.m_Length = 0;
	src.m_Begin = nullptr;
    src.m_NullTerminatedPtr = nullptr;
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
inline void str_view_template<CharT>::swap(str_view_template<CharT>& rhs) noexcept
{
    std::swap(m_Length, rhs.m_Length);
    std::swap(m_Begin, rhs.m_Begin);
    std::swap(m_NullTerminatedPtr, rhs.m_NullTerminatedPtr);
}

template<typename CharT>
inline size_t str_view_template<CharT>::length() const
{
    if(m_Length == SIZE_MAX)
    {
        assert(m_NullTerminatedPtr == m_Begin);
        m_Length = tstrlen(m_Begin);
    }
    return m_Length;
}

template<typename CharT>
inline bool str_view_template<CharT>::empty() const
{
    if(m_Length == SIZE_MAX)
    {
        // Length is unknown. String is null-terminated.
        // We still don't need to know the length. We just peek first character.
        assert(m_NullTerminatedPtr == m_Begin);
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
        //assert(m_Begin[length()] == (CharT)0); // Make sure it's really null terminated.
		return m_Begin;
    }
	if(m_NullTerminatedPtr == nullptr)
    {
        // Not null terminated, so length must be known.
        assert(m_Length != SIZE_MAX);
        CharT* nullTerminated = new CharT[m_Length + 1];
		memcpy(nullTerminated, begin(), m_Length * sizeof(CharT));
        nullTerminated[m_Length] = (CharT)0;
        m_NullTerminatedPtr = nullTerminated;
    }
	return m_NullTerminatedPtr;
}

template<typename CharT>
inline size_t str_view_template<CharT>::copy_to(CharT* dst, size_t offset, size_t length) const
{
    const size_t thisLen = this->length();
    assert(offset <= thisLen);
    length = std::min(length, thisLen - offset);
    memcpy(dst, m_Begin + offset, length * sizeof(CharT));
    return length;
}

template<typename CharT>
inline void str_view_template<CharT>::to_string(StringT& dst, size_t offset, size_t length) const
{
    const size_t thisLen = this->length();
    assert(offset <= thisLen);
    length = std::min(length, thisLen - offset);
    dst.assign(m_Begin + offset, m_Begin + (offset + length));
}

#if STR_VIEW_CPP17

template<typename CharT>
inline void str_view_template<CharT>::to_string_view(StringViewT& dst, size_t offset, size_t length) const
{
    const size_t thisLen = this->length();
    assert(offset <= thisLen);
    length = std::min(length, thisLen - offset);
    dst = StringViewT(m_Begin + offset, length);
}

#endif // #if STR_VIEW_CPP17

template<typename CharT>
inline str_view_template<CharT> str_view_template<CharT>::substr(size_t offset, size_t length) const
{
    // Length can remain unknown.
    if(m_Length == SIZE_MAX && length == SIZE_MAX)
    {
        assert(m_NullTerminatedPtr == m_Begin);
        return str_view_template<CharT>(m_Begin + offset);
    }

    const size_t thisLen = this->length();
    assert(offset <= thisLen);
	length = std::min(length, thisLen - offset);
	// Result will be null-terminated.
	if(m_NullTerminatedPtr == m_Begin && length == thisLen - offset)
		return str_view_template<CharT>(m_Begin + offset, length, StillNullTerminated());
	// Result will not be null-terminated.
	return str_view_template<CharT>(m_Begin + offset, length);
}

template<typename CharT>
inline int str_view_template<CharT>::compare(const str_view_template<CharT>& rhs, bool case_sensitive) const
{
    const size_t lhsLen = length();
    const size_t rhsLen = rhs.length();
    const size_t minLen = std::min(lhsLen, rhsLen);

    if(minLen > 0)
    {
        const int result = case_sensitive ?
            tstrncmp(data(), rhs.data(), minLen) :
            tstrnicmp(data(), rhs.data(), minLen);
        if(result != 0)
            return result;
    }

    if(lhsLen < rhsLen)
        return -1;
    if(lhsLen > rhsLen)
        return 1;
    return 0;
}

template<typename CharT>
inline bool str_view_template<CharT>::starts_with(CharT prefix, bool case_sensitive) const
{
    if(!empty())
    {
        if(case_sensitive)
            return *m_Begin == prefix;
        return tstrnicmp(m_Begin, &prefix, 1) == 0;
    }
    return false;
}

template<typename CharT>
inline bool str_view_template<CharT>::starts_with(const str_view_template<CharT>& prefix, bool case_sensitive) const
{
    const size_t prefixLen = prefix.length();
    if(length() >= prefixLen)
    {
        const int cmpResult = case_sensitive ?
            tstrncmp(m_Begin, prefix.m_Begin, prefixLen) :
            tstrnicmp(m_Begin, prefix.m_Begin, prefixLen);
        return cmpResult == 0;
    }
    return false;
}

template<typename CharT>
inline bool str_view_template<CharT>::ends_with(CharT suffix, bool case_sensitive) const
{
    const size_t thisLen = length();
    if(thisLen > 0)
    {
        if(case_sensitive)
            return m_Begin[thisLen - 1] == suffix;
        return tstrnicmp(m_Begin + (thisLen - 1), &suffix, 1) == 0;
    }
    return false;
}

template<typename CharT>
inline bool str_view_template<CharT>::ends_with(const str_view_template<CharT>& suffix, bool case_sensitive) const
{
    const size_t thisLen = length();
    const size_t suffixLen = suffix.length();
    if(thisLen >= suffixLen)
    {
        const int cmpResult = case_sensitive ?
            tstrncmp(m_Begin + (thisLen - suffixLen), suffix.m_Begin, suffixLen) :
            tstrnicmp(m_Begin + (thisLen - suffixLen), suffix.m_Begin, suffixLen);
        return cmpResult == 0;
    }
    return false;
}

void Format(const wstr_view& format, ...)
{
    va_list argList;
    va_start(argList, format.c_str());
    va_end(argList);
}
