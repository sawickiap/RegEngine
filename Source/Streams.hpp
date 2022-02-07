#pragma once

enum FILE_STREAM_FLAGS
{
    FILE_STREAM_FLAG_SEQUENTIAL = 0x1,
};

class FileReadStream
{
public:
    void Init(const wstr_view& path, uint32_t flags);
    ~FileReadStream();

    // Returns number of bytes read.
    size_t ReadSomeData(void* data, size_t len);

    void ReadData(std::span<char> data);
    void ReadData(void* data, size_t len)
    {
        ReadData(std::span<char>((char*)data, len));
    }
    template<typename T>
    void ReadValue(T& outValue)
    {   
        ReadData((char*)&outValue, sizeof(T));
    }

private:
    unique_ptr<HANDLE, CloseHandleDeleter> m_Handle;
};

class FileWriteStream
{
public:
    void Init(const wstr_view& path, uint32_t flags);
    ~FileWriteStream();

    void WriteData(std::span<const char> data);
    void WriteData(const void* data, size_t len)
    {
        WriteData(std::span<const char>((const char*)data, len));
    }
    template<typename T>
    void WriteValue(const T& value)
    {   
        WriteData((const char*)&value, sizeof(T));
    }
    void WriteString(const str_view& s)
    {
        WriteData(s.data(), s.length());
    }
    void WriteString(const wstr_view& s)
    {
        WriteData(s.data(), s.length() * sizeof(wchar_t));
    }

private:
    unique_ptr<HANDLE, CloseHandleDeleter> m_Handle;
};
