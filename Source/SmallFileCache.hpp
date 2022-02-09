#pragma once

class SmallFileCachePimpl;

/*
Offers loading entire content from a file.
Caches the content of small files indefinitely so they are not loaded again if not needed.
When asking for the same file again, it checks its modification date.
*/
class SmallFileCache
{
public:
    SmallFileCache();
    ~SmallFileCache();
    void Clear();
    std::vector<char> LoadFile(const wstr_view& path);

private:
    unique_ptr<SmallFileCachePimpl> m_Pimpl;
};

extern SmallFileCache* g_SmallFileCache;
