#pragma once

class BaseStream
{
public:
    // Values match WinAPI constants FILE_BEGIN, FILE_CURRENT, FILE_END.
    enum class MoveMethod { Begin = 0, Current = 1, End = 2 };

    virtual ~BaseStream() = default;

    // Default implementation asserts as not supported and returns SIZE_MAX.
    virtual size_t GetSize();
    // Default implementation asserts as not supported and returns SIZE_MAX.
    virtual size_t GetPosition();
    // Returns new position.
    // Default implementation asserts as not supported and returns SIZE_MAX.
    virtual size_t SetPosition(ptrdiff_t distance, MoveMethod method);

    // Returns number of bytes read.
    // Default implementation asserts as not supported and returns 0.
    virtual size_t TryRead(void* outBytes, size_t maxBytesToRead);
    // If not all bytes could be written, throws error.
    // Default implementation asserts as not supported.
    virtual void Write(const void* bytes, size_t bytesToWrite);

    // If not all bytes could be read, throws error.
    void Read(void* outBytes, size_t bytesToRead);
    void Read(std::span<char> outBytes)
    {
        Read(outBytes.data(), outBytes.size());
    }
    template<typename T>
    void ReadValue(T& outValue)
    {   
        Read(&outValue, sizeof(T));
    }
    template<typename T>
    T ReadValue()
    {   
        T result;
        Read(&result, sizeof(T));
        return result;
    }

    void Write(std::span<const char> bytes)
    {
        Write(bytes.data(), bytes.size());
    }
    template<typename T>
    void WriteValue(const T& value)
    {
        Write(&value, sizeof(T));
    }
    void WriteString(const str_view& str)
    {
        Write(str.data(), str.length());
    }
    void WriteString(const wstr_view& str)
    {
        Write(str.data(), str.length() * sizeof(wchar_t));
    }
};

class FileStream : public BaseStream
{
public:
    enum Flags
    {
        Flag_Read = 0x1,
        Flag_Write = 0x2,
        Flag_Sequential = 0x4,
    };

    FileStream(const wstr_view& path, uint32_t flags);
    size_t GetSize() override;
    size_t GetPosition() override;
    size_t SetPosition(ptrdiff_t distance, MoveMethod method) override;
    size_t TryRead(void* outBytes, size_t maxBytesToRead) override;
    void Write(const void* bytes, size_t bytesToWrite) override;
    using BaseStream::Write;

private:
    unique_ptr<HANDLE, CloseHandleDeleter> m_Handle;
};

std::vector<char> LoadFile(const wstr_view& path);
void SaveFile(const wstr_view& path, std::span<const char> bytes);
