#include "BaseUtils.hpp"
#include "Settings.hpp"
#define RAPIDJSON_HAS_STDSTRING 1
#include "../ThirdParty/rapidjson/include/rapidjson/document.h"
#include "../ThirdParty/rapidjson/include/rapidjson/writer.h"
#include "../ThirdParty/rapidjson/include/rapidjson/stringbuffer.h"
#include "../ThirdParty/rapidjson/include/rapidjson/error/en.h"

static void ThrowParseError(const rapidjson::Document& doc, const str_view& str)
{
    assert(doc.HasParseError());
    const auto errorCode = doc.GetParseError();
    const char* const errorMsg = rapidjson::GetParseError_En(errorCode);
    uint32_t row, col;
    StringOffsetToRowCol(row, col, str, doc.GetErrorOffset());
    FAIL(std::format(L"RapidJSON parsing error: row={}, column={}, code={} \"{}\"", row, col, (int)errorCode, str_view(errorMsg)));
}

static bool LoadValueFromJSON(bool& outVal, const rapidjson::Value& jsonVal)
{
    if(jsonVal.IsBool())
    {
        outVal = jsonVal.GetBool();
        return true;
    }
    return false;
}
static bool LoadValueFromJSON(uint32_t& outVal, const rapidjson::Value& jsonVal)
{
    if(jsonVal.IsUint())
    {
        outVal = jsonVal.GetUint();
        return true;
    }
    return false;
}
static bool LoadValueFromJSON(int32_t& outVal, const rapidjson::Value& jsonVal)
{
    if(jsonVal.IsInt())
    {
        outVal = jsonVal.GetInt();
        return true;
    }
    return false;
}
static bool LoadValueFromJSON(float& outVal, const rapidjson::Value& jsonVal)
{
    if(jsonVal.IsFloat())
    {
        outVal = jsonVal.GetFloat();
        return true;
    }
    else if(jsonVal.IsInt64())
    {
        outVal = (float)jsonVal.GetInt64();
        return true;
    }
    return false;
}

template<typename VecT>
bool LoadVecFromJSON(VecT& outVec, const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(!realVal->IsArray())
        return false;
    auto array = realVal->GetArray();
    if(array.Size() != (size_t)VecT::length())
        return false;
    for(uint32_t i = 0; i < array.Size(); ++i)
    {
        if(!LoadValueFromJSON(outVec[i], array[i]))
            return false;
    }
    return true;
}

template<typename MatT>
bool LoadMatFromJSON(MatT& outMat, const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(!realVal->IsArray())
        return false;
    auto array = realVal->GetArray();
    const uint32_t colCount = outMat.length();
    const uint32_t rowCount = outMat[0].length();
    if(array.Size() != colCount * rowCount)
        return false;
    for(uint32_t row = 0, i = 0; row < rowCount; ++row)
    {
        for(uint32_t col = 0; col < colCount; ++col, ++i)
        {
            if(!LoadValueFromJSON(outMat[col][row], array[i]))
                return false;
        }
    }
    return true;
}

template bool LoadVecFromJSON<glm::vec2>(glm::vec2& outVec, const void* jsonVal);
template bool LoadVecFromJSON<glm::vec3>(glm::vec3& outVec, const void* jsonVal);
template bool LoadVecFromJSON<glm::vec4>(glm::vec4& outVec, const void* jsonVal);
template bool LoadVecFromJSON<glm::uvec2>(glm::uvec2& outVec, const void* jsonVal);
template bool LoadVecFromJSON<glm::uvec3>(glm::uvec3& outVec, const void* jsonVal);
template bool LoadVecFromJSON<glm::uvec4>(glm::uvec4& outVec, const void* jsonVal);
template bool LoadVecFromJSON<glm::ivec2>(glm::ivec2& outVec, const void* jsonVal);
template bool LoadVecFromJSON<glm::ivec3>(glm::ivec3& outVec, const void* jsonVal);
template bool LoadVecFromJSON<glm::ivec4>(glm::ivec4& outVec, const void* jsonVal);
template bool LoadVecFromJSON<glm::bvec2>(glm::bvec2& outVec, const void* jsonVal);
template bool LoadVecFromJSON<glm::bvec3>(glm::bvec3& outVec, const void* jsonVal);
template bool LoadVecFromJSON<glm::bvec4>(glm::bvec4& outVec, const void* jsonVal);

template bool LoadMatFromJSON<glm::mat4>(glm::mat4& outMat, const void* jsonVal);

class SettingCollection
{
public:
    void Register(Setting* setting)
    {
        m_Settings.push_back(setting);
    }

    void LoadFromFile(const wstr_view& filePath);
private:
    std::vector<Setting*> m_Settings;
};

class SettingManager
{
public:
    static SettingManager& GetSingleton();

    void Register(SettingCategory category, Setting* setting)
    {
        switch(category)
        {
        case SettingCategory::Startup:
            m_StartupSettings.Register(setting);
            break;
        case SettingCategory::Load:
            m_LoadSettings.Register(setting);
            break;
        default:
            assert(0);
        }
    }

    void LoadStartupSettings();
    void LoadLoadSettings();

private:
    SettingCollection m_StartupSettings;
    SettingCollection m_LoadSettings;
};

Setting::Setting(SettingCategory category, const str_view& name) :
    m_Name{name.data(), name.size()}
{
    SettingManager::GetSingleton().Register(category, this);
}

void SettingCollection::LoadFromFile(const wstr_view& filePath)
{
    ERR_TRY;
    using namespace rapidjson;
    auto fileContents = LoadFile(filePath);
    Document doc;
    doc.Parse<kParseCommentsFlag | kParseTrailingCommasFlag | kParseNanAndInfFlag | kParseValidateEncodingFlag>(
        fileContents.data(), fileContents.size());
    if(doc.HasParseError())
        ThrowParseError(doc, str_view{fileContents.data(), fileContents.size()});
    CHECK_BOOL(doc.IsObject());
    for(Setting* setting : m_Settings)
    {
        const auto memberIt = doc.FindMember(setting->GetName().c_str());
        if(memberIt != doc.MemberEnd())
            setting->LoadFromJSON(&memberIt->value);
        else
            LogWarningF(L"Setting \"{}\" not found. Leaving current value.", str_view(setting->GetName()));
    }
    ERR_CATCH_MSG(std::format(L"Cannot load settings from file \"{}\".", filePath));
}

SettingManager& SettingManager::GetSingleton()
{
    static SettingManager mgr;
    return mgr;
}

void SettingManager::LoadStartupSettings()
{
    ERR_TRY;
    LogMessage(L"Loading statup settings...");
    m_StartupSettings.LoadFromFile(L"StartupSettings.json");
    ERR_CATCH_MSG(L"Cannot load startup settings.");
}

void SettingManager::LoadLoadSettings()
{
    try
    {
        ERR_TRY;
        LogMessage(L"Loading load settings...");
        m_LoadSettings.LoadFromFile(L"LoadSettings.json");
        ERR_CATCH_MSG(L"Cannot load load settings.");
    }
    CATCH_PRINT_ERROR(;);
}

void BoolSetting::LoadFromJSON(const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(!LoadValueFromJSON(m_Value, *realVal))
        LogWarningF(L"Invalid bool setting \"{}\".", str_view(GetName()));
}

void FloatSetting::LoadFromJSON(const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(!LoadValueFromJSON(m_Value, *realVal))
        LogWarningF(L"Invalid float setting \"%.*hs\".", str_view(GetName()));
}

void UintSetting::LoadFromJSON(const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(!LoadValueFromJSON(m_Value, *realVal))
        LogWarningF(L"Invalid uint setting \"%.*hs\".", str_view(GetName()));
}

void IntSetting::LoadFromJSON(const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(!LoadValueFromJSON(m_Value, *realVal))
        LogWarningF(L"Invalid int setting \"%.*hs\".", str_view(GetName()));
}

void StringSetting::LoadFromJSON(const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(realVal->IsString())
        m_Value = ConvertCharsToUnicode(str_view{realVal->GetString(), realVal->GetStringLength()}, CP_UTF8);
    else
        LogWarningF(L"Invalid string setting \"%.*hs\".", str_view(GetName()));
}

void LoadStartupSettings()
{
    SettingManager::GetSingleton().LoadStartupSettings();
}
void LoadLoadSettings()
{
    SettingManager::GetSingleton().LoadLoadSettings();
}
