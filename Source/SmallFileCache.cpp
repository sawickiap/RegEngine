#include "BaseUtils.hpp"
#include "SmallFileCache.hpp"
#include "Streams.hpp"
#include <unordered_map>

constexpr size_t MAX_CACHED_FILE_SIZE = 128llu * 1024;

SmallFileCache* g_SmallFileCache;

class SmallFileCachePimpl
{
public:
    struct Entry
    {
        std::filesystem::file_time_type m_LastWriteTime;
        std::vector<char> m_Contents;
    };
    // Key is file path converted to absolute-cannonical-uppercase
    using MapType = std::unordered_map<wstring, Entry>;
    MapType m_Entries;
};

SmallFileCache::SmallFileCache() :
    m_Pimpl{std::make_unique<SmallFileCachePimpl>()}
{

}

SmallFileCache::~SmallFileCache()
{

}

void SmallFileCache::Clear()
{
    m_Pimpl->m_Entries.clear();
}

std::vector<char> SmallFileCache::LoadFile(const wstr_view& path)
{
    ERR_TRY;

    std::filesystem::path pathP = StrToPath(path);
    std::filesystem::file_time_type lastWriteTime;
    if(!GetFileLastWriteTime(lastWriteTime, pathP))
        FAIL(L"File doesn't exist.");

    wstring key = std::filesystem::canonical(pathP);
    ToUpperCase(key);

    const auto it = m_Pimpl->m_Entries.find(key);
    // Found in cache.
    if(it != m_Pimpl->m_Entries.end())
    {
        // Last write time matches: Just return copy of the data.
        if(it->second.m_LastWriteTime == lastWriteTime)
            return it->second.m_Contents;
        // Last write time doesn't match: Really load the file, update cache entry.
        std::vector<char> contents = ::LoadFile(path);
        if(contents.size() <= MAX_CACHED_FILE_SIZE)
        {
            it->second.m_LastWriteTime = lastWriteTime;
            it->second.m_Contents = contents;
        }
        else
            m_Pimpl->m_Entries.erase(it);
        return contents;
    }
    // Not found in cache: Really load the file, create cache entry.
    std::vector<char> contents = ::LoadFile(path);
    if(contents.size() <= MAX_CACHED_FILE_SIZE)
    {
        m_Pimpl->m_Entries.insert(std::make_pair(std::move(key), SmallFileCachePimpl::Entry{
            .m_LastWriteTime = lastWriteTime,
            .m_Contents = contents}));
    }
    return contents;

    ERR_CATCH_MSG(std::format(L"Small file cache couldn't load file \"{}\".", path));
}
