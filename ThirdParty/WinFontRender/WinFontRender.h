/*

WinFontRender

Small single-header C++ library that renders Windows fonts in graphics applications.

Author:  Adam Sawicki - http://asawicki.info - adam__DELETE__@asawicki.info
Version: 1.0.0, 2019-03-14

Documentation: see README.md and comments in the code below.

# Version history

- Version 1.0.0, 2019-03-14
First version.

# License

Copyright (c) 2019 Adam Sawicki

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software. A notice about using the
Software shall be included in both textual documentation and in About/Credits
window or screen (if one exists) within the software that uses it.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifndef WIN_FONT_RENDER_H
#define WIN_FONT_RENDER_H

#ifndef NOMINMAX
#define NOMINMAX // For windows.h
#endif
#include <Windows.h>

#include <vector>
#include <string>
#include <memory>

#include <cstdint>

#pragma region str_view
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
// Copy of str_view library.
// See: https://github.com/sawickiap/str_view
//
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/*
str_view - null-termination-aware string-view class for C++.

Author:  Adam Sawicki - http://asawicki.info - adam__DELETE__@asawicki.info
Version: 1.1.1, 2019-01-22
License: MIT

Documentation: see README.md and comments in the code below.

# Version history

- Version 1.1.1, 2019-01-22
Added missing const to copy_to(), to_string() methods.
- Version 1.1.0, 2018-09-11
Added missing const to substr() method.
- Version 1.0.0, 2018-07-18
First version.

# License

Copyright 2018 Adam Sawicki

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/
//#pragma once

#include <string>
#include <atomic>
#include <algorithm> // for min, max
#include <memory> // for memcmp

#include <cassert>
#include <cstring>
#include <cstdint>

#pragma region Math
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
// Very basic vector types.
//
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

namespace WinFontRender
{

////////////////////////////////////////////////////////////////////////////////
// bvec2

struct bvec2
{
    bool x, y;

    // Uninitialized.
    bvec2() { }
    bvec2(bool newX, bool newY) : x(newX), y(newY) { }
    // Must be array of 2 elements.
    bvec2(const bool* arr) : x(arr[0]), y(arr[1]) { }

    operator bool*() { return &x; }
    operator const bool*() const { return &x; }

    bvec2 operator!() const { return bvec2(!x, !y); }
    bvec2 operator||(const bvec2& rhs) const { return bvec2(x || rhs.x, y || rhs.y); }
    bvec2 operator&&(const bvec2& rhs) const { return bvec2(x && rhs.x, y && rhs.y); }

    bool operator==(const bvec2& rhs) const { return x == rhs.x && y == rhs.y; }
    bool operator!=(const bvec2& rhs) const { return x != rhs.x || y != rhs.y; }

    bool& operator[](size_t index) { return (&x)[index]; }
    bool operator[](size_t index) const { return (&x)[index]; }
};

#define BVEC2_FALSE   vec2(false, false)
#define BVEC2_TRUE    vec2(true, true)

inline bool all(const bvec2& v) { return v.x && v.y; }
inline bool any(const bvec2& v) { return v.x || v.y; }

////////////////////////////////////////////////////////////////////////////////
// bvec4

struct bvec4
{
    bool x, y, z, w;

    // Uninitialized.
    bvec4() { }
    bvec4(bool newX, bool newY, bool newZ, bool newW) : x(newX), y(newY), z(newZ), w(newW) { }
    // Must be array of 4 elements.
    bvec4(const bool* arr) : x(arr[0]), y(arr[1]), z(arr[2]), w(arr[3]) { }

    operator bool*() { return &x; }
    operator const bool*() const { return &x; }

    bvec4 operator!() const { return bvec4(!x, !y, !z, !w); }
    bvec4 operator||(const bvec4& rhs) const { return bvec4(x || rhs.x, y || rhs.y, z || rhs.z, w || rhs.w); }
    bvec4 operator&&(const bvec4& rhs) const { return bvec4(x && rhs.x, y && rhs.y, z && rhs.z, w && rhs.w); }

    bool operator==(const bvec4& rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w; }
    bool operator!=(const bvec4& rhs) const { return x != rhs.x || y != rhs.y || z != rhs.z || w != rhs.w; }

    bool& operator[](size_t index) { return (&x)[index]; }
    bool operator[](size_t index) const { return (&x)[index]; }
};

#define BVEC4_FALSE   vec2(false, false, false, false)
#define BVEC4_TRUE    vec2(true, true, true, true)

inline bool all(const bvec4& v) { return v.x && v.y && v.z && v.w; }
inline bool any(const bvec4& v) { return v.x || v.y || v.z || v.w; }

////////////////////////////////////////////////////////////////////////////////
// vec2

template<typename T>
struct base_vec2
{
    T x, y;

    // Uninitialized.
    base_vec2() { }
    base_vec2(T newX, T newY) : x(newX), y(newY) { }
    // Must be array of 2 elements.
    base_vec2(const T* arr) : x(arr[0]), y(arr[1]) { }

    operator T*() { return &x; }
    operator const T*() const { return &x; }

    base_vec2<T> operator+() const { return *this; }
    base_vec2<T> operator-() const { return base_vec2<T>(-x, -y); }

    base_vec2<T> operator+(const base_vec2<T>& rhs) const { return base_vec2<T>(x + rhs.x, y + rhs.y); }
    base_vec2<T> operator-(const base_vec2<T>& rhs) const { return base_vec2<T>(x + rhs.x, y + rhs.y); }
    base_vec2<T> operator*(const base_vec2<T>& rhs) const { return base_vec2<T>(x * rhs.x, y * rhs.y); }
    base_vec2<T> operator/(const base_vec2<T>& rhs) const { return base_vec2<T>(x / rhs.x, y / rhs.y); }
    base_vec2<T> operator%(const base_vec2<T>& rhs) const { return base_vec2<T>(x % rhs.x, y % rhs.y); }

    base_vec2<T> operator*(T rhs) const { return base_vec2<T>(x * rhs, y * rhs); }
    base_vec2<T> operator/(T rhs) const { return base_vec2<T>(x / rhs, y / rhs); }
    base_vec2<T> operator%(T rhs) const { return base_vec2<T>(x % rhs, y % rhs); }

    base_vec2<T>& operator+=(const base_vec2<T>& rhs) { x += rhs.x; y += rhs.y; return *this; }
    base_vec2<T>& operator-=(const base_vec2<T>& rhs) { x -= rhs.x; y -= rhs.y; return *this; }
    base_vec2<T>& operator*=(const base_vec2<T>& rhs) { x *= rhs.x; y *= rhs.y; return *this; }
    base_vec2<T>& operator/=(const base_vec2<T>& rhs) { x /= rhs.x; y /= rhs.y; return *this; }
    base_vec2<T>& operator%=(const base_vec2<T>& rhs) { x %= rhs.x; y %= rhs.y; return *this; }

    base_vec2<T>& operator*=(T rhs) { x *= rhs; y *= rhs; return *this; }
    base_vec2<T>& operator/=(T rhs) { x /= rhs; y /= rhs; return *this; }
    base_vec2<T>& operator%=(T rhs) { x %= rhs; y %= rhs; return *this; }

    bvec2 operator==(const base_vec2<T>& rhs) const { return bvec2(x == rhs.x, y == rhs.y); }
    bvec2 operator!=(const base_vec2<T>& rhs) const { return bvec2(x != rhs.x, y != rhs.y); }
    bvec2 operator< (const base_vec2<T>& rhs) const { return bvec2(x <  rhs.x, y <  rhs.y); }
    bvec2 operator<=(const base_vec2<T>& rhs) const { return bvec2(x <= rhs.x, y <= rhs.y); }
    bvec2 operator> (const base_vec2<T>& rhs) const { return bvec2(x >  rhs.x, y >  rhs.y); }
    bvec2 operator>=(const base_vec2<T>& rhs) const { return bvec2(x >= rhs.x, y >= rhs.y); }

    T& operator[](size_t index) { return (&x)[index]; }
    T operator[](size_t index) const { return (&x)[index]; }
};

template<typename T>
inline base_vec2<T> operator*(T lhs, const base_vec2<T>& rhs) { return base_vec2<T>(lhs * rhs.x, lhs * rhs.y); }

typedef base_vec2<float> vec2;
typedef base_vec2<int32_t> ivec2;
typedef base_vec2<uint32_t> uvec2;

#define VEC2_ZERO    vec2(0.f, 0.f)
#define IVEC2_ZERO   ivec2(0, 0)
#define UVEC2_ZERO   uvec2(0u, 0u)

////////////////////////////////////////////////////////////////////////////////
// vec4

template<typename T>
struct base_vec4
{
    T x, y, z, w;

    // Uninitialized.
    base_vec4() { }
    base_vec4(T newX, T newY, T newZ, T newW) : x(newX), y(newY), z(newZ), w(newW) { }
    // Must be array of 4 elements.
    base_vec4(const T* arr) : x(arr[0]), y(arr[1]), z(arr[2]), w(arr[3]) { }

    base_vec4(const base_vec2<T>& newXY, T newZ, T newW) : x(newXY.x), y(newXY.y), z(newZ), w(newW) { }
    base_vec4(T newX, const base_vec2<T>& newYZ, T newW) : x(newX), y(newYZ.x), z(newYZ.y), w(newW) { }
    base_vec4(T newX, T newY, const base_vec2<T>& newZW) : x(newX), y(newY), z(newZW.x), w(newZW.y) { }
    base_vec4(const base_vec2<T>& newXY, const base_vec2<T>& newZW) : x(newXY.x), y(newXY.y), z(newZW.x), w(newZW.y) { }

    operator T*() { return &x; }
    operator const T*() const { return &x; }

    base_vec4<T> operator+() const { return *this; }
    base_vec4<T> operator-() const { return base_vec4<T>(-x, -y, -z, -w); }

    base_vec4<T> operator+(const base_vec4<T>& rhs) const { return base_vec4<T>(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w); }
    base_vec4<T> operator-(const base_vec4<T>& rhs) const { return base_vec4<T>(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w); }
    base_vec4<T> operator*(const base_vec4<T>& rhs) const { return base_vec4<T>(x * rhs.x, y * rhs.y, z * rhs.z, w * rhs.w); }
    base_vec4<T> operator/(const base_vec4<T>& rhs) const { return base_vec4<T>(x / rhs.x, y / rhs.y, z / rhs.z, w / rhs.w); }
    base_vec4<T> operator%(const base_vec4<T>& rhs) const { return base_vec4<T>(x % rhs.x, y % rhs.y, z % rhs.z, w % rhs.w); }

    base_vec4<T> operator*(T rhs) const { return base_vec4<T>(x * rhs, y * rhs, z * rhs, w * rhs); }
    base_vec4<T> operator/(T rhs) const { return base_vec4<T>(x / rhs, y / rhs, z / rhs, w / rhs); }
    base_vec4<T> operator%(T rhs) const { return base_vec4<T>(x % rhs, y % rhs, z % rhs, w % rhs); }

    base_vec4<T>& operator+=(const base_vec4<T>& rhs) { x += rhs.x; y += rhs.y; z += rhs.z; w += rhs.w; }
    base_vec4<T>& operator-=(const base_vec4<T>& rhs) { x -= rhs.x; y -= rhs.y; z -= rhs.z; w -= rhs.w; }
    base_vec4<T>& operator*=(const base_vec4<T>& rhs) { x *= rhs.x; y *= rhs.y; z *= rhs.z; w *= rhs.w; }
    base_vec4<T>& operator/=(const base_vec4<T>& rhs) { x /= rhs.x; y /= rhs.y; z /= rhs.z; w /= rhs.w; }
    base_vec4<T>& operator%=(const base_vec4<T>& rhs) { x %= rhs.x; y %= rhs.y; z %= rhs.z; w %= rhs.w; }

    base_vec4<T>& operator*=(T rhs) { x *= rhs; y *= rhs; z *= rhs; w *= rhs; }
    base_vec4<T>& operator/=(T rhs) { x /= rhs; y /= rhs; z /= rhs; w /= rhs; }
    base_vec4<T>& operator%=(T rhs) { x %= rhs; y %= rhs; z %= rhs; w %= rhs; }

    bvec4 operator==(const base_vec4<T>& rhs) const { return bvec4(x == rhs.x, y == rhs.y, z == rhs.z, w == rhs.w); }
    bvec4 operator!=(const base_vec4<T>& rhs) const { return bvec4(x != rhs.x, y != rhs.y, z != rhs.z, w != rhs.w); }
    bvec4 operator< (const base_vec4<T>& rhs) const { return bvec4(x <  rhs.x, y <  rhs.y, z <  rhs.z, w <  rhs.w); }
    bvec4 operator<=(const base_vec4<T>& rhs) const { return bvec4(x <= rhs.x, y <= rhs.y, z <= rhs.z, w <= rhs.w); }
    bvec4 operator> (const base_vec4<T>& rhs) const { return bvec4(x >  rhs.x, y >  rhs.y, z >  rhs.z, w >  rhs.w); }
    bvec4 operator>=(const base_vec4<T>& rhs) const { return bvec4(x >= rhs.x, y >= rhs.y, z >= rhs.z, w >= rhs.w); }

    T& operator[](size_t index) { return (&x)[index]; }
    T operator[](size_t index) const { return (&x)[index]; }
};

template<typename T>
inline base_vec4<T> operator*(T lhs, const base_vec4<T>& rhs) { return base_vec4<T>(lhs * rhs.x, lhs * rhs.y, lhs * rhs.z, lhs * rhs.w); }

template<typename T>
inline base_vec4<T> min(const base_vec4<T>& lhs, const base_vec4<T>& rhs) {
    return base_vec4<T>(std::min(lhs.x, rhs.x), std::min(lhs.y, rhs.y), std::min(lhs.z, rhs.z), std::min(lhs.w, rhs.w));
}
template<typename T>
inline base_vec4<T> max(const base_vec4<T>& lhs, const base_vec4<T>& rhs) {
    return base_vec4<T>(std::max(lhs.x, rhs.x), std::max(lhs.y, rhs.y), std::max(lhs.z, rhs.z), std::max(lhs.w, rhs.w));
}

typedef base_vec4<float> vec4;
typedef base_vec4<int32_t> ivec4;
typedef base_vec4<uint32_t> uvec4;

#define VEC4_ZERO    vec4(0.f, 0.f, 0.f, 0.f)
#define IVEC4_ZERO   ivec4(0, 0, 0, 0)
#define UVEC4_ZERO   uvec4(0u, 0u, 0u, 0u)

} // namespace WinFontRender

#pragma endregion

#pragma region WinFontRender Header
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
// WinFontRender library - header part
//
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

namespace WinFontRender
{

// Bit flags to be used as "vbFlags". They describe format of vertex + index buffer.
enum VERTEX_BUFFER_FLAGS
{
    // Index buffer is in use, with indices of type uint16_t.
    VERTEX_BUFFER_FLAG_USE_INDEX_BUFFER_16BIT = 0x1,
    // Index buffer is in use, with indices of type uint32_t.
    VERTEX_BUFFER_FLAG_USE_INDEX_BUFFER_32BIT = 0x2,
    // Primitive topology is triangle list. Each quad is made of 6 vertices or indices.
    VERTEX_BUFFER_FLAG_TRIANGLE_LIST = 0x10,
    // Primitive topology is triangle strip. Each quad is made of 4 vertices.
    // Quads are separated by primitive restart index = -1. Index buffer usage is required.
    VERTEX_BUFFER_FLAG_TRIANGLE_STRIP_WITH_RESTART_INDEX = 0x20,
    // Primitive topology is triangle strip. Each quad is made of 4 vertices.
    // Quads are separated by degenerate triangles created by duplicating 2 vertices or indices.
    VERTEX_BUFFER_FLAG_TRIANGLE_STRIP_WITH_DEGENERATE_TRIANGLES = 0x40,
};

// Describes specific vertex buffer and optional index buffer.
struct SVertexBufferDesc
{
    // Pointer to position attribute of first vertex.
    // Positions must be of type vec2 / float[2].
    void* FirstPosition;
    // Pointer to texture coordinate attribute of first vertex.
    // Texture coordinates must be of type vec2 / float[2].
    void* FirstTexCoord;
    // Step to take between positions of subsequent vertices, in bytes.
    size_t PositionStrideBytes;
    // Step to take between texture coordinates of subsequent vertices, in bytes.
    size_t TexCoordStrideBytes;
    // Pointer to first index in index buffer.
    // Ignored if vbFlags don't indicate that index buffer is in use.
    void* FirstIndex;
};

// Returns true if given combination of VERTEX_BUFFER_FLAG_* is valid.
bool ValidateVertexBufferFlags(uint32_t vbFlags);

// Converts number of quads to number of vertices and indices.
template<uint32_t vbFlags>
void QuadCountToVertexCount(size_t& outVertexCount, size_t& outIndexCount, size_t quadCount);

// Helper class that writes sequence of quads to a vartex buffer. Used internally.
template<uint32_t vbFlags>
class CQuadVertexWriter
{
public:
    // desc object must remain alive and unchanged as long as this object is in use.
    CQuadVertexWriter(const SVertexBufferDesc& desc) : m_Desc(desc) { }
    // positions/texCoords xy - left top, positions/texCoords.zw - right bottom
    __forceinline void PostQuad(const vec4& positions, const vec4& texCoords);

private:
    const SVertexBufferDesc& m_Desc;
    uint32_t m_QuadIndex = 0;

    __forceinline void SetVertex(size_t vertexIndex, const vec2& pos, const vec2& texCoord);
    __forceinline void SetPositionOnlyVertex(size_t vertexIndex, const vec2& pos);
    __forceinline const vec2& GetPosition(size_t vertexIndex) const;
    __forceinline void SetRestartIndex(size_t indexIndex);
    __forceinline void SetIndices(size_t firstIndexIndex, const int16_t* indices, size_t count, uint32_t vertexOffset);
};

// Describes parameters of font to be created.
struct SFontDesc
{
    // Bit flags that describe various parameters of created font.
    enum FLAGS
    {
        FLAG_BOLD   = 0x1,
        FLAG_ITALIC = 0x2,

        // Set this flag if texture coordinates start from left bottom corner as (0, 0), like in OpenGL.
        // Without this flag texture coordinates start from left top corner as (0, 0), like in DirectX and Vulkan.
        FLAG_TEXTURE_FROM_LEFT_BOTTOM = 0x10,
        // Texture extents must be rounded up to a power of 2.
        FLAG_TEXTURE_POW2 = 0x20,
    };

    // Name of the font as installed in the current system, e.g. "Arial".
    wstr_view FaceName;
    // Font size, in pixels, e.g. 32.
    int Height = 0;

    // Use FLAG_* bitflags.
    uint32_t Flags = 0;
    // You can just leave the default.
    UINT CharSet = DEFAULT_CHARSET;
    // You can just leave the default.
    DWORD PitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;

    /*
    Custom character ranges to render.
    If left to 0 and null, default range is used, which is 32..127.
    Otherwise, you have to specify own ranges. For each range, there must be 2 elements in CharRanges array.
    Range is inclusive both sides. Ranges must include character ' ' (space), '-', and '?'.
    */
    size_t CharRangeCount = 0;
    const wchar_t* CharRanges = nullptr;
};

// Main class that keeps texture and parameters of created font.
class CFont
{
public:
    // Bit flags to be used with methods of CFont class as "flags".
    enum FLAGS
    {
        // # Word wrap modes. Use only one.

        // No line breaks, just a single line. Works fast.
        FLAG_WRAP_SINGLE_LINE = 0x1,
        // Break lines only on explicit line end character '\n'.
        FLAG_WRAP_NORMAL      = 0x2,
        // Also wrap lines automatically, on single character boundaries.
        FLAG_WRAP_CHAR        = 0x4,
        // Also wrap lines automatically, on whole word boundaries if posible.
        FLAG_WRAP_WORD        = 0x8,

        // # Use any combination.

        FLAG_UNDERLINE        = 0x10,
        FLAG_DOUBLE_UNDERLINE = 0x20,
        FLAG_OVERLINE         = 0x40,
        FLAG_STRIKEOUT        = 0x80,

        // # Horizontal alignmne.t Use only one.

        FLAG_HLEFT   = 0x100,
        FLAG_HCENTER = 0x200,
        FLAG_HRIGHT  = 0x400,

        // # Vertical alignment. Use only one.

        FLAG_VTOP    = 0x800,
        FLAG_VMIDDLE = 0x1000,
        FLAG_VBOTTOM = 0x2000,
    };

    // Information about single character.
    struct SCharInfo
    {
        // xy = left top, zw = right bottom.
        vec4 TexCoordsRect;
        // Step to next character. Scaled to font size = 1.0.
        float Advance;
        // Offset to left top corner of the quad to draw. Scaled to font size = 1.0.
        vec2 Offset;
        // Size of the quad to draw. Scaled to font size = 1.0.
        vec2 Size;
        // Index to first entry in m_KerningEntries which has First equal to this character. SIZE_MAX if no kerning for this character.
        size_t KerningEntryFirstIndex;
    };

    struct SKerningEntry
    {
        wchar_t First, Second;
        // Scaled to font size = 1.0.
        float Amount;
    };

    // Returns true if given set of CFont::FLAG_* flags is valid.
    static bool ValidateFlags(uint32_t flags);

    CFont();
    ~CFont();
    bool Init(const SFontDesc& desc);

    const SCharInfo& GetCharInfo(wchar_t ch) const { return m_CharInfo[ch]; }
    // Get texture coordinates of the place on the texture that is surely filled, so it can be used to draw filled rectangle using font texture.
    const vec2& GetFillTexCoords() const { return m_FillTexCoords; }

    float GetLineGap() const { return m_LineGap; }
    float GetLineGap(float fontSize) const { return m_LineGap * fontSize; }
    // Additional '_' is used because stupid Windows.h defines "GetCharWidth" as macro :(
    float GetCharWidth_(wchar_t ch) const { return m_CharInfo[(int)ch].Advance; }
    float GetCharWidth_(wchar_t ch, float fontSize) const { return m_CharInfo[(int)ch].Advance * fontSize; }
    float GetKerning(wchar_t firstCh, wchar_t secondCh) const;
    float GetKerning(wchar_t firstCh, wchar_t secondCh, float fontSize) const { return GetKerning(firstCh, secondCh) * fontSize; }

    /* Returns pointer and parameters of internal buffer with texture data.
    Pixels are row-major, from top to bottom, from left to right.
    Each pixel is single byte 0..255.
    outRowPitch is step between rows, in bytes.
    */
    void GetTextureData(const void*& outData, uvec2& outSize, size_t& outRowPitch) const;
    void FreeTextureData();

    float CalcSingleLineTextWidth(const wstr_view& text, float fontSize) const;
    /*
    Split text into lines. Call iteratively to get subsequent lines of text.
    inoutIndex is input-output index of character in text. It should be 0 on first call.
    outBegin, outEnd, outWidth are output.
    Returns false if we are alredy at the end and no new line could be found.
    flags: only FLAG_WRAP_* have meaning.
    If FLAG_WRAP_SINGLE_LINE or FLAG_WRAP_NORMAL is used, textWidth is ignored.
    */
    bool LineSplit(
        size_t *outBegin, size_t *outEnd, float *outWidth, size_t *inoutIndex,
        const wstr_view& text,
        float fontSize, uint32_t flags, float textWidth) const;
    // Calculates width and height of text that would be drawn with given parameters.
    void CalcTextExtent(vec2& outExtent, const wstr_view& text, float fontSize, uint32_t flags, float textWidth) const;
    // Calculates number of quads needed to draw given single line text.
    // flags: only FLAG_UNDERLINE, FLAG_DOUBLE_UNDERLINE, FLAG_OVERLINE, FLAG_STRIKEOUT have meaning.
    size_t CalcSingleLineQuadCount(const wstr_view& text, uint32_t flags) const;
    // Calculates number of quads needed to draw given text.
    size_t CalcQuadCount(const wstr_view& text, float fontSize, uint32_t flags, float textWidth) const;
    // Returns index of character, and percent of its width, of a single line text hit by point hitX.
    // Returns false if hitX is out of range of the text and hit cannot be found.
    // outPercent is optional. Pass null if you don't need this information.
    bool HitTestSingleLine(size_t& outIndex, float *outPercent,
        float posX, float hitX, const wstr_view& text, float fontSize, uint32_t flags) const;
    // Returns index of character, and percent of its width and height, of a text hit by point hit.
    // Returns false if hit is out of range of the text and hit cannot be found.
    // outPercent is optional. Pass null if you don't need this information.
    // Returned outPercent.y can be outside of 0..1 range when hit position is in the gap between lines.
    bool HitTest(size_t& outIndex, vec2 *outPercent,
        const vec2& pos, const vec2& hit, const wstr_view& text, float fontSize, uint32_t flags, float textWidth) const;

    // positions.xy = left top, positions.zw = right bottom (or the opposite, it doesn't matter).
    template<uint32_t vbFlags> void GetFillVertices(
        const SVertexBufferDesc& vbDesc, const vec4& positions) const;
    template<uint32_t vbFlags> void GetSingleLineTextVertices(
        const SVertexBufferDesc& vbDesc, const vec2& pos, const wstr_view& text, float fontSize) const;
    template<uint32_t vbFlags> void GetTextVertices(
        const SVertexBufferDesc& vbDesc, const vec2& pos, const wstr_view& text, float fontSize, uint32_t fontFlags, float textWidth) const;

private:
    static const size_t CHAR_COUNT = 0x10000;
    // Information about all characters.
    SCharInfo m_CharInfo[CHAR_COUNT];
    // Sorted by first, then second, ascending.
    std::vector<SKerningEntry> m_KerningEntries;
    // Texture coordinates for drawing filled rectangle.
    vec2 m_FillTexCoords = VEC2_ZERO;
    float m_LineGap = 0.f;

    uvec2 m_TextureSize;
    size_t m_TextureRowPitch;
    std::vector<uint8_t> m_TextureData;

    void SortKerningEntries();
};


template<uint32_t vbFlags>
void QuadCountToVertexCount(size_t& outVertexCount, size_t& outIndexCount, size_t quadCount)
{
    if(quadCount == 0)
    {
        outVertexCount = outIndexCount = 0;
        return;
    }

    constexpr uint32_t anyIbFlags = VERTEX_BUFFER_FLAG_USE_INDEX_BUFFER_16BIT | VERTEX_BUFFER_FLAG_USE_INDEX_BUFFER_32BIT;
    constexpr bool useIb = (vbFlags & anyIbFlags) != 0;
    if(useIb)
    {
        if(vbFlags & VERTEX_BUFFER_FLAG_TRIANGLE_LIST)
        {
            outVertexCount = quadCount * 4;
            outIndexCount = quadCount * 6;
        }
        else if(vbFlags & VERTEX_BUFFER_FLAG_TRIANGLE_STRIP_WITH_RESTART_INDEX)
        {
            outVertexCount = quadCount * 4;
            outIndexCount = quadCount * 4 + (quadCount - 1);
        }
        else if(vbFlags & VERTEX_BUFFER_FLAG_TRIANGLE_STRIP_WITH_DEGENERATE_TRIANGLES)
        {
            outVertexCount = quadCount * 4;
            outIndexCount = quadCount * 4 + (quadCount - 1) * 2;
        }
        else
            assert(0);
    }
    else
    {
        outIndexCount = 0;
        if(vbFlags & VERTEX_BUFFER_FLAG_TRIANGLE_LIST)
            outVertexCount = quadCount * 6;
        else if(vbFlags & VERTEX_BUFFER_FLAG_TRIANGLE_STRIP_WITH_DEGENERATE_TRIANGLES)
            outVertexCount = quadCount * 4 + (quadCount - 1) * 2;
        else
            assert(0);
    }
}

template<uint32_t vbFlags>
__forceinline void CQuadVertexWriter<vbFlags>::PostQuad(const vec4& positions, const vec4& texCoords)
{
    constexpr uint32_t anyIbFlags = VERTEX_BUFFER_FLAG_USE_INDEX_BUFFER_16BIT | VERTEX_BUFFER_FLAG_USE_INDEX_BUFFER_32BIT;
    constexpr bool useIb = (vbFlags & anyIbFlags) != 0;
    if(useIb)
    {
        if(vbFlags & VERTEX_BUFFER_FLAG_TRIANGLE_LIST)
        {
            SetVertex(m_QuadIndex * 4 + 0, vec2(positions.x, positions.y), vec2(texCoords.x, texCoords.y));
            SetVertex(m_QuadIndex * 4 + 1, vec2(positions.z, positions.y), vec2(texCoords.z, texCoords.y));
            SetVertex(m_QuadIndex * 4 + 2, vec2(positions.x, positions.w), vec2(texCoords.x, texCoords.w));
            SetVertex(m_QuadIndex * 4 + 3, vec2(positions.z, positions.w), vec2(texCoords.z, texCoords.w));

            const int16_t indices[] = {0, 1, 2, 2, 1, 3};
            SetIndices(m_QuadIndex * 6, indices, _countof(indices), m_QuadIndex * 4);
        }
        else if(vbFlags & VERTEX_BUFFER_FLAG_TRIANGLE_STRIP_WITH_RESTART_INDEX)
        {
            SetVertex(m_QuadIndex * 4 + 0, vec2(positions.x, positions.y), vec2(texCoords.x, texCoords.y));
            SetVertex(m_QuadIndex * 4 + 1, vec2(positions.z, positions.y), vec2(texCoords.z, texCoords.y));
            SetVertex(m_QuadIndex * 4 + 2, vec2(positions.x, positions.w), vec2(texCoords.x, texCoords.w));
            SetVertex(m_QuadIndex * 4 + 3, vec2(positions.z, positions.w), vec2(texCoords.z, texCoords.w));

            if(m_QuadIndex > 0)
                SetRestartIndex(m_QuadIndex * 5 - 1);
            const int16_t indices[] = {0, 1, 2, 3};
            SetIndices(m_QuadIndex * 5, indices, _countof(indices), m_QuadIndex * 4);
        }
        else if(vbFlags & VERTEX_BUFFER_FLAG_TRIANGLE_STRIP_WITH_DEGENERATE_TRIANGLES)
        {
            SetVertex(m_QuadIndex * 4 + 0, vec2(positions.x, positions.y), vec2(texCoords.x, texCoords.y));
            SetVertex(m_QuadIndex * 4 + 1, vec2(positions.z, positions.y), vec2(texCoords.z, texCoords.y));
            SetVertex(m_QuadIndex * 4 + 2, vec2(positions.x, positions.w), vec2(texCoords.x, texCoords.w));
            SetVertex(m_QuadIndex * 4 + 3, vec2(positions.z, positions.w), vec2(texCoords.z, texCoords.w));

            if(m_QuadIndex > 0)
            {
                const int16_t indices[] = {-1, 0};
                SetIndices(m_QuadIndex * 6 - 2, indices, _countof(indices), m_QuadIndex * 4);
            }
            const int16_t indices[] = {0, 1, 2, 3};
            SetIndices(m_QuadIndex * 6, indices, _countof(indices), m_QuadIndex * 4);
        }
        else
            assert(0);
    }
    else
    {
        if(vbFlags & VERTEX_BUFFER_FLAG_TRIANGLE_LIST)
        {
            SetVertex(m_QuadIndex * 6 + 0, vec2(positions.x, positions.y), vec2(texCoords.x, texCoords.y));
            SetVertex(m_QuadIndex * 6 + 1, vec2(positions.z, positions.y), vec2(texCoords.z, texCoords.y));
            SetVertex(m_QuadIndex * 6 + 2, vec2(positions.x, positions.w), vec2(texCoords.x, texCoords.w));

            SetVertex(m_QuadIndex * 6 + 3, vec2(positions.x, positions.w), vec2(texCoords.x, texCoords.w));
            SetVertex(m_QuadIndex * 6 + 4, vec2(positions.z, positions.y), vec2(texCoords.z, texCoords.y));
            SetVertex(m_QuadIndex * 6 + 5, vec2(positions.z, positions.w), vec2(texCoords.z, texCoords.w));
        }
        else if(vbFlags & VERTEX_BUFFER_FLAG_TRIANGLE_STRIP_WITH_DEGENERATE_TRIANGLES)
        {
            if(m_QuadIndex > 0)
            {
                SetPositionOnlyVertex(m_QuadIndex * 6 - 2, GetPosition(m_QuadIndex * 6 - 3));
                SetPositionOnlyVertex(m_QuadIndex * 6 - 1, vec2(positions.x, positions.y));
            }

            SetVertex(m_QuadIndex * 6 + 0, vec2(positions.x, positions.y), vec2(texCoords.x, texCoords.y));
            SetVertex(m_QuadIndex * 6 + 1, vec2(positions.z, positions.y), vec2(texCoords.z, texCoords.y));
            SetVertex(m_QuadIndex * 6 + 2, vec2(positions.x, positions.w), vec2(texCoords.x, texCoords.w));
            SetVertex(m_QuadIndex * 6 + 3, vec2(positions.z, positions.w), vec2(texCoords.z, texCoords.w));
        }
        else
            assert(0);
    }

    ++m_QuadIndex;
}

template<uint32_t vbFlags>
__forceinline void CQuadVertexWriter<vbFlags>::SetVertex(size_t vertexIndex, const vec2& pos, const vec2& texCoord)
{
    *(vec2*)( (char*)m_Desc.FirstPosition + vertexIndex * m_Desc.PositionStrideBytes ) = pos;
    *(vec2*)( (char*)m_Desc.FirstTexCoord + vertexIndex * m_Desc.TexCoordStrideBytes ) = texCoord;
}

template<uint32_t vbFlags>
__forceinline void CQuadVertexWriter<vbFlags>::SetPositionOnlyVertex(size_t vertexIndex, const vec2& pos)
{
    *(vec2*)( (char*)m_Desc.FirstPosition + vertexIndex * m_Desc.PositionStrideBytes ) = pos;
}

template<uint32_t vbFlags>
__forceinline const vec2& CQuadVertexWriter<vbFlags>::GetPosition(size_t vertexIndex) const
{
    return *(const vec2*)( (char*)m_Desc.FirstPosition + vertexIndex * m_Desc.PositionStrideBytes );
}

template<uint32_t vbFlags>
__forceinline void CQuadVertexWriter<vbFlags>::SetRestartIndex(size_t indexIndex)
{
    constexpr bool ib32 = (vbFlags & VERTEX_BUFFER_FLAG_USE_INDEX_BUFFER_32BIT) != 0;
    if(ib32)
    {
        *(uint32_t*)( (char*)m_Desc.FirstIndex + indexIndex * sizeof(uint32_t) ) = UINT32_MAX;
    }
    else
    {
        *(uint16_t*)( (char*)m_Desc.FirstIndex + indexIndex * sizeof(uint16_t) ) = UINT16_MAX;
    }
}

template<uint32_t vbFlags>
__forceinline void CQuadVertexWriter<vbFlags>::SetIndices(size_t firstIndexIndex, const int16_t* indices, size_t count, uint32_t vertexOffset)
{
    constexpr bool ib32 = (vbFlags & VERTEX_BUFFER_FLAG_USE_INDEX_BUFFER_32BIT) != 0;
    if(ib32)
    {
        uint32_t* ib = (uint32_t*)( (char*)m_Desc.FirstIndex + firstIndexIndex * sizeof(uint32_t) );
        for(size_t i = 0; i < count; ++i)
        {
            ib[i] = vertexOffset + indices[i];
        }
    }
    else
    {
        uint16_t* ib = (uint16_t*)( (char*)m_Desc.FirstIndex + firstIndexIndex * sizeof(uint16_t) );
        for(size_t i = 0; i < count; ++i)
        {
            ib[i] = (uint16_t)(vertexOffset + indices[i]);
        }
    }
}

template<uint32_t vbFlags> void CFont::GetFillVertices(const SVertexBufferDesc& vbDesc,
    const vec4& positions) const
{
    assert(ValidateVertexBufferFlags(vbFlags) && vbDesc.FirstPosition && vbDesc.FirstTexCoord);
    CQuadVertexWriter<vbFlags> writer(vbDesc);
    writer.PostQuad(positions, vec4(m_FillTexCoords, m_FillTexCoords));
}

template<uint32_t vbFlags>
void CFont::GetSingleLineTextVertices(const SVertexBufferDesc& vbDesc,
    const vec2& pos, const wstr_view& text, float fontSize) const
{
    GetTextVertices<vbFlags>(vbFlags, vbDesc, pos, text, fontSize,
        FLAG_HLEFT | FLAG_VTOP | FLAG_WRAP_SINGLE_LINE, FLT_MAX);
}

template<uint32_t vbFlags>
void CFont::GetTextVertices(const SVertexBufferDesc& vbDesc,
    const vec2& pos, const wstr_view& text,
    float fontSize, uint32_t fontFlags, float textWidth) const
{
    assert(ValidateVertexBufferFlags(vbFlags));
    assert(ValidateFlags(fontFlags));
    assert(vbDesc.FirstPosition && vbDesc.FirstTexCoord);
    CQuadVertexWriter<vbFlags> writer(vbDesc);

    size_t lineBeg, lineEnd, lineIndex = 0, i;
    float lineWidth;
    float startX, currX, currY;
    float lineY1, lineY2;

    static const float lineHeight = 0.075f;
    static const float underlinePosPercent = 0.95f;
    static const float strikeoutPosPercent = 0.6f;
    static const float overlinePosPercent = 0.05f;

    static const float doubleLineHeight = 0.06666666667f;
    static const float doubleUnderlinePosPercent = 0.98f;

    if (fontFlags & FLAG_VTOP)
    {
        currY = pos.y;
        while (LineSplit(&lineBeg, &lineEnd, &lineWidth, &lineIndex, text, fontSize, fontFlags, textWidth))
        {
            if (fontFlags & FLAG_HLEFT)
            {
                startX = currX = pos.x;
            }
            else if (fontFlags & FLAG_HRIGHT)
            {
                startX = currX = pos.x - lineWidth;
            }
            else // fontFlags & FLAG_HCENTER
            {
                startX = currX = pos.x - lineWidth * 0.5f;
            }

            // Characters
            wchar_t prevCh = 0;
            for (i = lineBeg; i < lineEnd; i++)
            {
                const wchar_t currCh = text[i];
                const SCharInfo& charInfo = GetCharInfo(currCh);
                if (currCh != L' ')
                {
                    writer.PostQuad(
                        vec4(
                            currX + charInfo.Offset.x*fontSize,
                            currY + charInfo.Offset.y*fontSize,
                            currX + (charInfo.Offset.x+charInfo.Size.x)*fontSize,
                            currY + (charInfo.Offset.y+charInfo.Size.y)*fontSize),
                        charInfo.TexCoordsRect);
                }
                currX += charInfo.Advance * fontSize;
                if(prevCh)
                {
                    currX += GetKerning(prevCh, currCh, fontSize);
                }
                prevCh = currCh;
            }

            // Underlines
            if (fontFlags & (FLAG_UNDERLINE | FLAG_DOUBLE_UNDERLINE | FLAG_OVERLINE | FLAG_STRIKEOUT))
            {
                if (fontFlags & FLAG_UNDERLINE)
                {
                    lineY2 = currY + fontSize * underlinePosPercent;
                    lineY1 = lineY2 - fontSize * lineHeight;
                    writer.PostQuad(
                        vec4(startX, lineY1, startX+lineWidth, lineY2),
                        vec4(GetFillTexCoords(), GetFillTexCoords()));
                }
                else if (fontFlags & FLAG_DOUBLE_UNDERLINE)
                {
                    lineY2 = currY + fontSize * doubleUnderlinePosPercent;
                    lineY1 = lineY2 - fontSize * doubleLineHeight;
                    writer.PostQuad(
                        vec4(startX, lineY1, startX+lineWidth, lineY2),
                        vec4(GetFillTexCoords(), GetFillTexCoords()));
                    lineY2 -= fontSize * doubleLineHeight * 2.f;
                    lineY1 -= fontSize * doubleLineHeight * 2.f;
                    writer.PostQuad(
                        vec4(startX, lineY1, startX+lineWidth, lineY2),
                        vec4(GetFillTexCoords(), GetFillTexCoords()));
                }
                if (fontFlags & FLAG_OVERLINE)
                {
                    lineY1 = currY + fontSize * overlinePosPercent;
                    lineY2 = lineY1 + fontSize * lineHeight;
                    writer.PostQuad(
                        vec4(startX, lineY1, startX+lineWidth, lineY2),
                        vec4(GetFillTexCoords(), GetFillTexCoords()));
                }
                if (fontFlags & FLAG_STRIKEOUT)
                {
                    lineY1 = currY + fontSize * strikeoutPosPercent;
                    lineY2 = lineY1 + fontSize * lineHeight;
                    writer.PostQuad(
                        vec4(startX, lineY1, startX+lineWidth, lineY2),
                        vec4(GetFillTexCoords(), GetFillTexCoords()));
                }
            }

            currY += (1.f + GetLineGap()) * fontSize;
        }
    }
    // Not FLAG_VTOP
    else
    {
        // Divide into lines, remember, see number of lines.
        size_t lineCount = 0;
        std::vector<size_t> begs, ends;
        std::vector<float> widths;
        while (LineSplit(&lineBeg, &lineEnd, &lineWidth, &lineIndex, text, fontSize, fontFlags, textWidth))
        {
            begs.push_back(lineBeg);
            ends.push_back(lineEnd);
            widths.push_back(lineWidth);
            lineCount++;
        }
        // Calculate new beginning pos.y.
        if (fontFlags & FLAG_VBOTTOM)
        {
            currY = pos.y - lineCount * fontSize;
        }
        else // fontFlags & FLAG_VMIDDLE
        {
            currY = pos.y - lineCount * fontSize * 0.5f;
        }
        // Rest is like with FLAG_VTOP:
        for (size_t Line = 0; Line < lineCount; Line++)
        {
            if (fontFlags & FLAG_HLEFT)
                startX = currX = pos.x;
            else if (fontFlags & FLAG_HRIGHT)
                startX = currX = pos.x - widths[Line];
            else // fontFlags & FLAG_HCENTER
                startX = currX = pos.x - widths[Line] * 0.5f;

            // Characters
            wchar_t prevCh = 0;
            for (i = begs[Line]; i < ends[Line]; i++)
            {
                const wchar_t currCh = text[i];
                const SCharInfo& charInfo = GetCharInfo(currCh);
                if (currCh != L' ')
                {
                    writer.PostQuad(
                        vec4(
                            currX + charInfo.Offset.x*fontSize,
                            currY + charInfo.Offset.y*fontSize,
                            currX + (charInfo.Offset.x+charInfo.Size.x)*fontSize,
                            currY + (charInfo.Offset.y+charInfo.Size.y)*fontSize),
                        charInfo.TexCoordsRect);
                }
                currX += charInfo.Advance * fontSize;
                if(prevCh)
                {
                    currX += GetKerning(prevCh, currCh, fontSize);
                }
                prevCh = currCh;
            }

            // Underlines
            if (fontFlags & (FLAG_UNDERLINE | FLAG_DOUBLE_UNDERLINE | FLAG_OVERLINE | FLAG_STRIKEOUT))
            {
                if (fontFlags & FLAG_UNDERLINE)
                {
                    lineY2 = currY + fontSize * underlinePosPercent;
                    lineY1 = lineY2 - fontSize * lineHeight;
                    writer.PostQuad(
                        vec4(startX, lineY1, startX+widths[Line], lineY2),
                        vec4(GetFillTexCoords(), GetFillTexCoords()));
                }
                else if (fontFlags & FLAG_DOUBLE_UNDERLINE)
                {
                    lineY2 = currY + fontSize * doubleUnderlinePosPercent;
                    lineY1 = lineY2 - fontSize * doubleLineHeight;
                    writer.PostQuad(
                        vec4(startX, lineY1, startX+widths[Line], lineY2),
                        vec4(GetFillTexCoords(), GetFillTexCoords()));
                    lineY2 -= fontSize * doubleLineHeight * 2.f;
                    lineY1 -= fontSize * doubleLineHeight * 2.f;
                    writer.PostQuad(
                        vec4(startX, lineY1, startX+widths[Line], lineY2),
                        vec4(GetFillTexCoords(), GetFillTexCoords()));
                }
                if (fontFlags & FLAG_OVERLINE)
                {
                    lineY1 = currY + fontSize * overlinePosPercent;
                    lineY2 = lineY1 + fontSize * lineHeight;
                    writer.PostQuad(
                        vec4(startX, lineY1, startX+widths[Line], lineY2),
                        vec4(GetFillTexCoords(), GetFillTexCoords()));
                }
                if (fontFlags & FLAG_STRIKEOUT)
                {
                    lineY1 = currY + fontSize * strikeoutPosPercent;
                    lineY2 = lineY1 + fontSize * lineHeight;
                    writer.PostQuad(
                        vec4(startX, lineY1, startX+widths[Line], lineY2),
                        vec4(GetFillTexCoords(), GetFillTexCoords()));
                }
            }

            currY += (1.f + GetLineGap()) * fontSize;
        }
    }
}

} // namespace WinFontRender

#pragma endregion

#endif // #ifdef WIN_FONT_RENDER_H

// For Visual Studio IntelliSense.
#if defined(__INTELLISENSE__)
#define WIN_FONT_RENDER_IMPLEMENTATION
#endif

#ifdef WIN_FONT_RENDER_IMPLEMENTATION
#undef WIN_FONT_RENDER_IMPLEMENTATION

#pragma region WinFontRender Implementation
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
// WinFontRender library - CPP part
//
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#include <cassert>

// Just in case <Windows.h> was included before without #define NOMINMAX
#undef min
#undef max

namespace WinFontRender
{

static void BlitGray8Bitmap(
    uint8_t* dstBitmap, size_t dstRowPitch, const uvec2& dstPos,
    const uint8_t* srcBitmap, size_t srcRowPitch, const uvec2& srcPos, const uvec2& size)
{
    assert(dstPos.x + size.x <= dstRowPitch);
    uint8_t* dst = dstBitmap + dstPos.y * dstRowPitch + dstPos.x;
    const uint8_t* src = srcBitmap + srcPos.y * srcRowPitch + srcPos.x;
    for(uint32_t iy = 0; iy < size.y; ++iy)
    {
        for(uint32_t ix = 0; ix < size.x; ++ix)
        {
            const uint8_t val = src[ix];
            // Input range is 0..64, output is 0..255.
            dst[ix] = val >= 64 ? 255 : val * 4;
        }
        dst += dstRowPitch;
        src += srcRowPitch;
    }
}

bool ValidateVertexBufferFlags(uint32_t vbFlags)
{
    const bool useIb16 = (vbFlags & VERTEX_BUFFER_FLAG_USE_INDEX_BUFFER_16BIT) != 0;
    const bool useIb32 = (vbFlags & VERTEX_BUFFER_FLAG_USE_INDEX_BUFFER_32BIT) != 0;
    if(useIb16 && useIb32)
        return false;

    uint8_t topologyCounter = 0;
    if(vbFlags & VERTEX_BUFFER_FLAG_TRIANGLE_LIST)
        ++topologyCounter;
    if(vbFlags & VERTEX_BUFFER_FLAG_TRIANGLE_STRIP_WITH_RESTART_INDEX)
    {
        ++topologyCounter;
        if(!useIb16 && !useIb32)
            return false;
    }
    if(vbFlags & VERTEX_BUFFER_FLAG_TRIANGLE_STRIP_WITH_DEGENERATE_TRIANGLES)
        ++topologyCounter;
    return topologyCounter == 1;
}

// Find smallest power of two greater or equal to given number.
static inline uint32_t NextPow2(uint32_t v)
{
    v--;
    v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

// Align given value up to nearest multiply of align value.
// For example: AlignUp(11, 8) = 16. Use types like uint32_t, uint64_t as T.
template <typename T>
constexpr inline T AlignUp(T val, T align)
{
    return (val + align - 1) / align * align;
}

////////////////////////////////////////////////////////////////////////////////
// Internal class CSpritePacker

class CSpritePacker
{
public:
    CSpritePacker(uint32_t textureSizeX, uint32_t margin, bool pow2) :
        m_TextureSizeX(pow2 ? NextPow2(textureSizeX) : textureSizeX),
        m_Margin(margin),
        m_CurrPos(margin, margin),
        m_TextureSizeY(margin),
        m_Pow2(pow2)
    {
    }

    uint32_t GetTextureSizeY() const { return m_Pow2 ? NextPow2(m_TextureSizeY + m_Margin) : m_TextureSizeY + m_Margin; }

    void AddSprite(uvec2& outPos, const uvec2& size);

private:
    const uint32_t m_TextureSizeX;
    const uint32_t m_Margin;
    uvec2 m_CurrPos;
    uint32_t m_TextureSizeY;
    bool m_Pow2;
};

void CSpritePacker::AddSprite(uvec2& outPos, const uvec2& size)
{
    assert(size.x + 2 * m_Margin <= m_TextureSizeX);
    m_CurrPos.x += m_Margin;
    if(m_CurrPos.x + size.x + m_Margin <= m_TextureSizeX)
    {
        outPos = m_CurrPos;
        m_TextureSizeY = std::max(m_TextureSizeY, m_CurrPos.y + size.y);
        m_CurrPos.x += size.x;
    }
    else
    {
        // Start a new row.
        m_CurrPos = uvec2(0, m_TextureSizeY + m_Margin);
        AddSprite(outPos, size);
    }
}

////////////////////////////////////////////////////////////////////////////////
// class CFont

bool CFont::ValidateFlags(uint32_t flags)
{
    uint8_t count = 0;
    if(flags & FLAG_WRAP_SINGLE_LINE) ++count;
    if(flags & FLAG_WRAP_NORMAL) ++count;
    if(flags & FLAG_WRAP_CHAR) ++count;
    if(flags & FLAG_WRAP_WORD) ++count;
    if(count != 1) return false;

    count = 0;
    if(flags & FLAG_HLEFT) ++count;
    if(flags & FLAG_HCENTER) ++count;
    if(flags & FLAG_HRIGHT) ++count;
    if(count != 1) return false;

    count = 0;
    if(flags & FLAG_VTOP) ++count;
    if(flags & FLAG_VMIDDLE) ++count;
    if(flags & FLAG_VBOTTOM) ++count;
    if(count != 1) return false;

    return true;
}

CFont::CFont()
{
}

bool CFont::Init(const SFontDesc& desc)
{
    assert(!desc.FaceName.empty() && desc.Height > 0);

    ZeroMemory(m_CharInfo, sizeof m_CharInfo);

    const ivec2 dummyBitmapSize = ivec2(32, 32);
    // Rows top-down,
    // for each row pixels from left to right,
    // for each pixel components B, G, R = 0..255.
    BITMAPINFO dummyBitmapInfo = { {
            sizeof(BITMAPINFOHEADER), // biSize
            dummyBitmapSize.x, -dummyBitmapSize.y, // biWidth, biHeight
            1, // biPlanes
            24, // biBitCount
            BI_RGB, // biCompression
            0, // biSizeImage
            72, 72, // biXPelsPerMeter, biYPelsPerMeter
            0, 0, // biClrUsed, biClrImportant
        } };
    unsigned char *dummyBitmapData = nullptr;
    HBITMAP dummyBitmap = CreateDIBSection(NULL, &dummyBitmapInfo, DIB_RGB_COLORS, (void**)&dummyBitmapData, NULL, 0);
    if(dummyBitmap == NULL)
        return false;
    HDC dc = CreateCompatibleDC(NULL);
    if(dc == NULL)
        return false;
    HGDIOBJ oldBitmap = SelectObject(dc, dummyBitmap);
    HGDIOBJ oldFont = NULL;
    const float fontSizeInv = 1.f / (float)desc.Height;
    HFONT font = CreateFont(
        desc.Height, // cHeight
        0, // cWidth
        0, // cEscapement
        0, // cOrientation
        (desc.Flags & SFontDesc::FLAG_BOLD     ) ? FW_BOLD : FW_NORMAL, // cWeight
        (desc.Flags & SFontDesc::FLAG_ITALIC   ) ? TRUE : FALSE, // bItalic
        FALSE, // bUnderline. Doesn't seem to work when I set TRUE.
        FALSE, //bStrikeOut. Doesn't seem to work when I set TRUE.
        desc.CharSet, // iCharSet
        OUT_DEFAULT_PRECIS, // iOutPrecision
        CLIP_DEFAULT_PRECIS, // iClipPrecision
        ANTIALIASED_QUALITY, // iQuality
        desc.PitchAndFamily, // iPitchAndFamily
        desc.FaceName.c_str());
    if(font == NULL)
        return false;
    oldFont = SelectObject(dc, font);

    LONG ascent = 0, descent = 0;
    {
        UINT size = GetOutlineTextMetrics(dc, 0, NULL);
        assert(size >= sizeof(OUTLINETEXTMETRIC));
        std::vector<char> buf(size);
        GetOutlineTextMetrics(dc, size, (LPOUTLINETEXTMETRIC)buf.data());
        const OUTLINETEXTMETRIC* outlineTextMetric = (const OUTLINETEXTMETRIC*)buf.data();

        ascent  = outlineTextMetric->otmTextMetrics.tmAscent;
        descent = outlineTextMetric->otmTextMetrics.tmDescent;

        m_LineGap = outlineTextMetric->otmLineGap * fontSizeInv;
    }

    struct SGlyphInfo
    {
        bool Requested = false;
        size_t DataOffset = SIZE_MAX; // SIZE_MAX if glyph not present.
        uvec2 BlackBoxSize = uvec2(0, 0); // (0, 0) if no actual glyph available.
        uvec2 TexturePos = uvec2(0, 0);

        bool GlyphExists() const { return DataOffset != SIZE_MAX; }
        bool HasSprite() const { return GlyphExists() && BlackBoxSize.x && BlackBoxSize.y; }
    };
    std::vector<uint8_t> glyphData;
    std::vector<SGlyphInfo> glyphInfo(CHAR_COUNT);

    uint32_t requestedCount = 0;
    if(desc.CharRangeCount && desc.CharRanges)
    {
        for(size_t rangeIndex = 0; rangeIndex < desc.CharRangeCount; ++rangeIndex)
        {
            for(uint32_t i = desc.CharRanges[rangeIndex * 2]; i <= desc.CharRanges[rangeIndex * 2 + 1]; ++i)
            {
                if(!glyphInfo[i].Requested)
                {
                    glyphInfo[i].Requested = true;
                    ++requestedCount;
                }
            }
        }
    }
    else
    {
        const size_t charMin = 32, charMax = 127;
        for(size_t i = charMin; i <= charMax; ++i)
        {
            glyphInfo[i].Requested = true;
            ++requestedCount;
        }
    }
    assert(requestedCount);

    float existingGlyphCount = 0.f;

    const MAT2 mat2 = { {0, 1}, {0, 0}, {0, 0}, {0, 1} };
    for(size_t i = 1; i < CHAR_COUNT; ++i)
    {
        if(glyphInfo[i].Requested)
        {
            GLYPHMETRICS metrics = {};
            DWORD res = GetGlyphOutline(dc, (UINT)i, GGO_METRICS, &metrics, 0, NULL, &mat2);
            if(res != GDI_ERROR)
            {
                const size_t currGlyphDataOffset = glyphData.size();
                glyphInfo[i].DataOffset = currGlyphDataOffset;

                SCharInfo& charInfo = m_CharInfo[i];
                //charInfo.TexCoordsRect still uninitialized.
                charInfo.Advance = (float)metrics.gmCellIncX * fontSizeInv;
                charInfo.Offset = vec2(
                    (float)metrics.gmptGlyphOrigin.x * fontSizeInv,
                    (float)(ascent - metrics.gmptGlyphOrigin.y) * fontSizeInv);
                charInfo.Size = vec2(
                    (float)metrics.gmBlackBoxX * fontSizeInv,
                    (float)metrics.gmBlackBoxY * fontSizeInv);
                charInfo.KerningEntryFirstIndex = SIZE_MAX;

                if(metrics.gmBlackBoxX && metrics.gmBlackBoxY)
                {
                    DWORD currGlyphDataSize = GetGlyphOutline(dc, (UINT)i, GGO_GRAY8_BITMAP, &metrics, 0, NULL, &mat2);
                    if(currGlyphDataSize > 0 && currGlyphDataSize != GDI_ERROR)
                    {
                        glyphData.resize(currGlyphDataOffset + currGlyphDataSize);
                        glyphInfo[i].BlackBoxSize = uvec2(metrics.gmBlackBoxX, metrics.gmBlackBoxY);

                        uint8_t* currGlyphData = glyphData.data() + currGlyphDataOffset;
                        res = GetGlyphOutline(dc, (UINT)i, GGO_GRAY8_BITMAP, &metrics, currGlyphDataSize, currGlyphData, &mat2);
                        if(res == 0 || res == GDI_ERROR)
                            return false;

                        ++existingGlyphCount;
                    }
                }
            }
        }
    }

    DWORD kerningPairCount = GetKerningPairs(dc, 0, NULL);
    if(kerningPairCount)
    {
        std::vector<KERNINGPAIR> kerningPairs(kerningPairCount);
        DWORD res = GetKerningPairs(dc, kerningPairCount, kerningPairs.data());
        assert(res);

        for(size_t i = 0; i < kerningPairCount; ++i)
        {
            if(kerningPairs[i].iKernAmount)
            {
                SKerningEntry entry;
                entry.First = kerningPairs[i].wFirst;
                entry.Second = kerningPairs[i].wSecond;
                if(glyphInfo[entry.First].GlyphExists() && glyphInfo[entry.Second].GlyphExists() && kerningPairs[i].iKernAmount)
                {
                    entry.Amount = (float)kerningPairs[i].iKernAmount * fontSizeInv;
                    m_KerningEntries.push_back(entry);
                }
            }
        }
        SortKerningEntries();

        for(size_t i = 0, count = m_KerningEntries.size(); i < count; ++i)
        {
            const SKerningEntry& kerningEntry = m_KerningEntries[i];
            if(glyphInfo[kerningEntry.First].GlyphExists() && m_CharInfo[kerningEntry.First].KerningEntryFirstIndex == SIZE_MAX)
            {
                m_CharInfo[kerningEntry.First].KerningEntryFirstIndex = i;
            }
        }
    }

    assert(glyphInfo[L'-'].HasSprite() && glyphInfo[L'?'].HasSprite());

    SelectObject(dc, oldFont);
    DeleteObject(font);
    SelectObject(dc, oldBitmap);
    DeleteDC(dc);
    DeleteObject(dummyBitmap);

    std::vector<uint16_t> sortIndex;
    sortIndex.reserve(requestedCount);
    for(size_t i = 1; i < CHAR_COUNT; ++i)
    {
        if(glyphInfo[i].HasSprite())
        {
            sortIndex.push_back((uint16_t)i);
        }
    }
    std::sort(sortIndex.begin(), sortIndex.end(), [&glyphInfo](uint16_t lhs, uint16_t rhs) -> bool {
        return glyphInfo[lhs].BlackBoxSize.y > glyphInfo[rhs].BlackBoxSize.y;
    });

    m_TextureSize.x = (uint32_t)desc.Height * 8;
    const bool pow2 = (desc.Flags & SFontDesc::FLAG_TEXTURE_POW2) != 0;
    CSpritePacker packer{m_TextureSize.x, 1, pow2};
    for(uint32_t i = 0; i < sortIndex.size(); ++i)
    {
        const size_t glyphIndex = sortIndex[i];
        assert(glyphInfo[glyphIndex].HasSprite());
        packer.AddSprite(glyphInfo[glyphIndex].TexturePos, glyphInfo[glyphIndex].BlackBoxSize);
    }

    m_TextureSize.y = packer.GetTextureSizeY();
    const vec2 textureSizeInv = vec2(1.f / (float)m_TextureSize.x, 1.f / (float)m_TextureSize.y);
    m_TextureRowPitch = AlignUp<uint32_t>(m_TextureSize.x, 4);
    m_TextureData.resize(m_TextureRowPitch * m_TextureSize.y);

    for(size_t i = 1; i < CHAR_COUNT; ++i)
    {
        if(glyphInfo[i].HasSprite())
        {
            const uint32_t glyphDataRowPitch = AlignUp<uint32_t>(glyphInfo[i].BlackBoxSize.x, 4);
            BlitGray8Bitmap(m_TextureData.data(), m_TextureRowPitch, glyphInfo[i].TexturePos,
                glyphData.data() + glyphInfo[i].DataOffset, glyphDataRowPitch, uvec2(0, 0), glyphInfo[i].BlackBoxSize);
            m_CharInfo[i].TexCoordsRect = vec4(
                (float)glyphInfo[i].TexturePos.x * textureSizeInv.x,
                (float)glyphInfo[i].TexturePos.y * textureSizeInv.y,
                (float)(glyphInfo[i].TexturePos.x + glyphInfo[i].BlackBoxSize.x) * textureSizeInv.x,
                (float)(glyphInfo[i].TexturePos.y + glyphInfo[i].BlackBoxSize.y) * textureSizeInv.y);
            if(desc.Flags & SFontDesc::FLAG_TEXTURE_FROM_LEFT_BOTTOM)
            {
                m_CharInfo[i].TexCoordsRect.y = 1.f - m_CharInfo[i].TexCoordsRect.y;
                m_CharInfo[i].TexCoordsRect.w = 1.f - m_CharInfo[i].TexCoordsRect.w;
            }
        }
    }

    if (!glyphInfo[L'?'].GlyphExists())
        return false;
    if (!glyphInfo[L'-'].GlyphExists())
        return false;

    // Take constant position from the center of '-' character as fill texcoord.
    const SCharInfo& charInfo = m_CharInfo[L'-'];
    m_FillTexCoords.x = (charInfo.TexCoordsRect.x + charInfo.TexCoordsRect.z) * 0.5f;
    m_FillTexCoords.y = (charInfo.TexCoordsRect.y + charInfo.TexCoordsRect.w) * 0.5f;

    // Replace unknown characters with '?' character.
    for (size_t i = 0; i < CHAR_COUNT; i++)
    {
        if (!glyphInfo[i].GlyphExists())
            m_CharInfo[i] = m_CharInfo[L'?'];
    }

    return true;
}

CFont::~CFont()
{
}

float CFont::GetKerning(wchar_t firstCh, wchar_t secondCh) const
{
    size_t index = m_CharInfo[firstCh].KerningEntryFirstIndex;
    if(index == SIZE_MAX)
    {
        return 0.f;
    }
    const size_t kerningEntryCount = m_KerningEntries.size();
    for(; index < kerningEntryCount && m_KerningEntries[index].First == firstCh; ++index)
    {
        if(m_KerningEntries[index].Second == secondCh)
        {
            return m_KerningEntries[index].Amount;
        }
        if(m_KerningEntries[index].Second > secondCh)
        {
            break;
        }
    }
    return 0.f;
}

float CFont::CalcSingleLineTextWidth(const wstr_view& text, float fontSize) const
{
    float textWidth = 0.f;
    wchar_t prevCh = 0;
    for (size_t i = 0; i < text.length(); i++)
    {
        const wchar_t currCh = text[i];
        textWidth += m_CharInfo[currCh].Advance;
        if(prevCh)
        {
            textWidth += GetKerning(prevCh, currCh);
        }
        prevCh = currCh;
    }
    return textWidth * fontSize;
}

void CFont::GetTextureData(const void*& outData, uvec2& outSize, size_t& outRowPitch) const
{
    if(!m_TextureData.empty())
    {
        outData = m_TextureData.data();
        outSize = m_TextureSize;
        outRowPitch = m_TextureRowPitch;
    }
    else
    {
        outData = nullptr;
        outSize = UVEC2_ZERO;
        outRowPitch = 0;
    }
}

void CFont::FreeTextureData()
{
    std::vector<uint8_t> tmp;
    m_TextureData.swap(tmp);
}

bool CFont::LineSplit(
    size_t *outBegin, size_t *outEnd, float *outWidth, size_t *inoutIndex,
    const wstr_view& text,
    float fontSize, uint32_t flags, float textWidth) const
{
    assert(ValidateFlags(flags));

    const size_t textLen = text.length();

    if (*inoutIndex >= textLen)
    {
        return false;
    }

    *outBegin = *inoutIndex;
    *outWidth = 0.f;

    // Single line - special fast mode.
    if (flags & FLAG_WRAP_SINGLE_LINE)
    {
        wchar_t prevCh = 0;
        while (*inoutIndex < textLen)
        {
            const wchar_t currCh = text[*inoutIndex];
            *outWidth += m_CharInfo[currCh].Advance;
            if(prevCh)
            {
                *outWidth += GetKerning(prevCh, currCh);
            }
            prevCh = currCh;
            (*inoutIndex)++;
        }
        *outEnd = *inoutIndex;
        *outWidth *= fontSize;
        return true;
    }

    wchar_t prevCh = 0;
    // Remembered state from last occurence ofspace.
    // Could be useful in case of wrapping on word boundary.
    size_t lastSpaceIndex = std::wstring::npos;
    float widthWhenLastSpace = 0.f;
    for (;;)
    {
        // End of text
        if (*inoutIndex >= textLen)
        {
            *outEnd = textLen;
            break;
        }

        // Fetch character
        const wchar_t currCh = text[*inoutIndex];

        // End of line
        if (currCh == L'\n')
        {
            *outEnd = *inoutIndex;
            (*inoutIndex)++;
            break;
        }
        // End of line '\result'
        else if (currCh == L'\r')
        {
            *outEnd = *inoutIndex;
            (*inoutIndex)++;
            // Sequenfce "\result\n" - skip '\n'
            if (*inoutIndex < textLen && text[*inoutIndex] == L'\n')
            {
                (*inoutIndex)++;
            }
            break;
        }
        // Other character
        else
        {
            // Character width
            const float charWidth = GetCharWidth_(currCh, fontSize);
            const float kerning = prevCh ? GetKerning(prevCh, currCh, fontSize) : 0.f;

            /*
            If automatic word wrap is not enabled or
            if it all fits or
            it is a first character (protection against infinite loop when textWidth < width of first character) -
            include it no matter what.
            */
            if ((flags & FLAG_WRAP_NORMAL) || *outWidth + charWidth + kerning <= textWidth || *inoutIndex == *outBegin)
            {
                // If this is space - remembers its data.
                if (currCh == L' ')
                {
                    lastSpaceIndex = *inoutIndex;
                    widthWhenLastSpace = *outWidth;
                }
                *outWidth += charWidth + kerning;
                (*inoutIndex)++;
            }
            // If automatic word wrap is enabled and it doesn't fit
            else
            {
                // The character that doesn't fit is space
                if (currCh == L' ')
                {
                    *outEnd = *inoutIndex;
                    // We can just skip it
                    (*inoutIndex)++;
                    break;
                }
                // Previous character before this one is space
                else if (*inoutIndex > *outBegin && text[(*inoutIndex)-1] == L' ')
                {
                    // End will be at this space
                    *outEnd = lastSpaceIndex;
                    *outWidth = widthWhenLastSpace;
                    break;
                }

                // Wrapping lines on word boundaries
                if (flags & FLAG_WRAP_WORD)
                {
                    // There was a space
                    if (lastSpaceIndex != std::wstring::npos)
                    {
                        // The end will be at that space
                        *outEnd = lastSpaceIndex;
                        *inoutIndex = lastSpaceIndex+1;
                        *outWidth = widthWhenLastSpace;
                        break;
                    }
                    // There was no space - well, it will wrap on character boundary
                }

                *outEnd = *inoutIndex;
                break;
            }
        }
        prevCh = currCh;
    }

    return true;
}

void CFont::CalcTextExtent(vec2& outExtent, const wstr_view& text, float fontSize, uint32_t flags, float textWidth) const
{
    assert(ValidateFlags(flags));

    if(text.empty() || fontSize == 0.f)
    {
        outExtent = VEC2_ZERO;
        return;
    }

    size_t lineBeg, lineEnd, index = 0;
    float lineWidth;
    float lineCount = 0.f;
    outExtent.x = 0.0f;
    while (LineSplit(&lineBeg, &lineEnd, &lineWidth, &index, text, fontSize, flags, textWidth))
    {
        lineCount += 1.f;
        outExtent.x = std::max(outExtent.x, lineWidth);
    }

    if(lineCount > 0.f)
    {
        outExtent.y = (lineCount + (lineCount - 1.f) * m_LineGap) * fontSize;
    }
    else
        outExtent.y = 0.f;
}

size_t CFont::CalcSingleLineQuadCount(const wstr_view& text, uint32_t flags) const
{
    assert(ValidateFlags(flags));

    const size_t textLen = text.length();
    size_t result = 0;

    for (size_t i = 0; i < textLen; i++)
    {
        if (text[i] != L' ')
            result++;
    }

    if (flags & FLAG_DOUBLE_UNDERLINE)
        result += 2;
    else if (flags & FLAG_UNDERLINE)
        result++;
    if (flags & FLAG_OVERLINE)
        result++;
    if (flags & FLAG_STRIKEOUT)
        result++;

    return result;
}

size_t CFont::CalcQuadCount(const wstr_view& text, float fontSize, uint32_t flags, float textWidth) const
{
    assert(ValidateFlags(flags));

    size_t result = 0;
    size_t beg, end, index = 0;
    float width;
    int lineCount = 0;

    while (LineSplit(&beg, &end, &width, &index, text, fontSize, flags, textWidth))
    {
        for (size_t i = beg; i < end; ++i)
        {
            if (text[i] != L' ')
                result++;
        }
        lineCount++;
    }

    if (flags & FLAG_DOUBLE_UNDERLINE)
        result += 2 * lineCount;
    else if (flags & FLAG_UNDERLINE)
        result += lineCount;
    if (flags & FLAG_OVERLINE)
        result += lineCount;
    if (flags & FLAG_STRIKEOUT)
        result += lineCount;

    return result;
}

bool CFont::HitTestSingleLine(size_t& outIndex, float *outPercent,
    float posX, float hitX, const wstr_view& text, float fontSize, uint32_t flags) const
{
    assert(ValidateFlags(flags));
    const size_t textLen = text.length();

    if (flags & FLAG_HLEFT)
    {
        float currX = posX;
        // On the left
        if (hitX < currX)
            return false;
        // Traverse characters
        wchar_t prevCh = 0;
        for (size_t i = 0; i < textLen; i++)
        {
            const wchar_t currCh = text[i];
            const float charWidth = GetCharWidth_(currCh, fontSize);
            const float kerning = prevCh ? GetKerning(prevCh, currCh, fontSize) : 0.f;
            const float newX = currX + charWidth;
            // Found
            if (hitX < newX)
            {
                outIndex = i;
                if(outPercent)
                    *outPercent = (hitX-currX) / charWidth;
                return true;
            }
            currX = newX + kerning;
            prevCh = currCh;
        }
        // Not found
        return false;
    }
    else if (flags & FLAG_HRIGHT)
    {
        float currX = posX;
        // On the right
        if (hitX > currX)
            return false;
        // Traverse characters
        wchar_t prevCh = 0;
        for (size_t i = textLen; i--; )
        {
            const wchar_t currCh = text[i];
            const float charWidth = GetCharWidth_(currCh, fontSize);
            const float kerning = prevCh ? GetKerning(currCh, prevCh, fontSize) : 0.f;
            const float newX = currX - charWidth;
            // Found
            if (hitX >= newX)
            {
                outIndex = i;
                if(outPercent)
                    *outPercent = (hitX-newX) / charWidth;
                return true;
            }
            currX = newX - kerning;
            prevCh = currCh;
        }
        // Not found
        return false;
    }
    else // flags & FLAG_HCENTER
    {
        // Calculate width of whole line
        const float lineWidth = CalcSingleLineTextWidth(text, fontSize);
        // Calculate position of the beginning
        posX -= lineWidth * 0.5f;
        // Rest is the same as with alignment to the left:

        float currX = posX;
        // On the left
        if (hitX < currX)
            return false;
        // Traverse characters
        wchar_t prevCh = 0;
        for (size_t i = 0; i < textLen; i++)
        {
            const wchar_t currCh = text[i];
            const float charWidth = GetCharWidth_(currCh, fontSize);
            const float kerning = prevCh ? GetKerning(prevCh, currCh, fontSize) : 0.f;
            const float newX = currX + charWidth;
            // Found
            if (hitX < newX)
            {
                outIndex = i;
                if(outPercent)
                    *outPercent = (hitX-currX) / charWidth;
                return true;
            }
            currX = newX + kerning;
            prevCh = currCh;
        }
        // Not found
        return false;
    }
}

bool CFont::HitTest(size_t& outIndex, vec2 *outPercent,
    const vec2& pos, const vec2& hit, const wstr_view& text, float fontSize, uint32_t flags, float textWidth) const
{
    assert(ValidateFlags(flags));

    if (flags & FLAG_VTOP)
    {
        size_t beg, end, index = 0;
        float width;
        float currY = pos.y;
        // Above
        if (hit.y < currY)
            return false;
        // Traverse lines
        while (LineSplit(&beg, &end, &width, &index, text, fontSize, flags, textWidth))
        {
            // Found
            if (hit.y < currY + (1.f + m_LineGap * 0.5f) * fontSize)
            {
                // Check x
                if (HitTestSingleLine(
                    outIndex, outPercent ? &outPercent->x : nullptr,
                    pos.x, hit.x,
                    text.substr(beg, end - beg),
                    fontSize, flags))
                {
                    outIndex += beg;
                    if(outPercent)
                        outPercent->y = (hit.y-currY) / fontSize;
                    return true;
                }
                else
                    return false;
            }
            currY += (1.f + m_LineGap) * fontSize;
        }
        // Not found
        return false;
    }
    // Bottom of vertical center
    else
    {
        // Split into lines, remember them, see how many lines do we have.
        size_t lineCount = 0;
        std::vector<size_t> begs, ends;
        size_t beg, end, index = 0;
        float width;
        while (LineSplit(&beg, &end, &width, &index, text, fontSize, flags, textWidth))
        {
            begs.push_back(beg);
            ends.push_back(end);
            lineCount++;
        }

        float currY = pos.y;
        // Calculate new beginning Y.
        if (flags & FLAG_VBOTTOM)
            currY -= lineCount * fontSize;
        else // flags & FLAG_VMIDDLE
            currY -= lineCount * fontSize * 0.5f;
        // Rest is the same as with alignment to the top:

        // Above
        if (hit.y < currY)
            return false;
        // Traverse lines
        for (size_t Line = 0; Line < lineCount; Line++)
        {
            const float newY = currY + (1.f + m_LineGap) * fontSize;
            // Found
            if (hit.y < currY + (1.f + m_LineGap * 0.5f) * fontSize)
            {
                // Check x
                if (HitTestSingleLine(
                    outIndex, outPercent ? &outPercent->x : nullptr,
                    pos.x, hit.x,
                    text.substr(begs[Line], ends[Line] - begs[Line]),
                    fontSize, flags))
                {
                    outIndex += begs[Line];
                    if(outPercent)
                        outPercent->y = (hit.y - currY) / fontSize;
                    return true;
                }
                else
                    return false;
            }
            currY += (1.f + m_LineGap) * fontSize;
        }
        // Not found
        return false;
    }
}

void CFont::SortKerningEntries()
{
    std::sort(m_KerningEntries.begin(), m_KerningEntries.end(), [](const SKerningEntry& lhs, const SKerningEntry& rhs) -> bool {
        if(lhs.First < rhs.First) { return true; }
        if(lhs.First > rhs.First) { return false; }
        return lhs.Second < rhs.Second;
    });
}

} // namespace WinFontRender

#pragma endregion

#endif // #ifdef WIN_FONT_RENDER_IMPLEMENTATION
