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
    FAIL(Format(L"RapidJSON parsing error: row=%u, column=%u, code=%i \"%hs\"", row, col, (int)errorCode, errorMsg));
}

static bool LoadValueFromJson(bool& outVal, const rapidjson::Value& jsonVal)
{
    if(jsonVal.IsBool())
    {
        outVal = jsonVal.GetBool();
        return true;
    }
    return false;
}
static bool LoadValueFromJson(uint32_t& outVal, const rapidjson::Value& jsonVal)
{
    if(jsonVal.IsUint())
    {
        outVal = jsonVal.GetUint();
        return true;
    }
    return false;
}
static bool LoadValueFromJson(int32_t& outVal, const rapidjson::Value& jsonVal)
{
    if(jsonVal.IsInt())
    {
        outVal = jsonVal.GetInt();
        return true;
    }
    return false;
}
static bool LoadValueFromJson(float& outVal, const rapidjson::Value& jsonVal)
{
    if(jsonVal.IsFloat())
    {
        outVal = jsonVal.GetFloat();
        return true;
    }
    return false;
}

template<typename VecT>
bool LoadVecFromJson(VecT& outVec, const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(!realVal->IsArray())
        return false;
    auto array = realVal->GetArray();
    if(array.Size() != (size_t)VecT::length())
        return false;
    for(uint32_t i = 0; i < array.Size(); ++i)
    {
        if(!LoadValueFromJson(outVec[i], array[i]))
            return false;
    }
    return true;
}

template bool LoadVecFromJson<glm::vec2>(glm::vec2& outVec, const void* jsonVal);
template bool LoadVecFromJson<glm::vec3>(glm::vec3& outVec, const void* jsonVal);
template bool LoadVecFromJson<glm::vec4>(glm::vec4& outVec, const void* jsonVal);
template bool LoadVecFromJson<glm::uvec2>(glm::uvec2& outVec, const void* jsonVal);
template bool LoadVecFromJson<glm::uvec3>(glm::uvec3& outVec, const void* jsonVal);
template bool LoadVecFromJson<glm::uvec4>(glm::uvec4& outVec, const void* jsonVal);
template bool LoadVecFromJson<glm::ivec2>(glm::ivec2& outVec, const void* jsonVal);
template bool LoadVecFromJson<glm::ivec3>(glm::ivec3& outVec, const void* jsonVal);
template bool LoadVecFromJson<glm::ivec4>(glm::ivec4& outVec, const void* jsonVal);
template bool LoadVecFromJson<glm::bvec2>(glm::bvec2& outVec, const void* jsonVal);
template bool LoadVecFromJson<glm::bvec3>(glm::bvec3& outVec, const void* jsonVal);
template bool LoadVecFromJson<glm::bvec4>(glm::bvec4& outVec, const void* jsonVal);

class SettingCollection
{
public:
    void Register(Setting* setting)
    {
        m_settings.push_back(setting);
    }

    void LoadFromFile(const wstr_view& filePath);
private:
    std::vector<Setting*> m_settings;
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
            m_startupSettings.Register(setting);
            break;
        case SettingCategory::Load:
            m_loadSettings.Register(setting);
            break;
        default:
            assert(0);
        }
    }

    void LoadStartupSettings();
    void LoadLoadSettings();

private:
    SettingCollection m_startupSettings;
    SettingCollection m_loadSettings;
};

Setting::Setting(SettingCategory category, const str_view& name) :
    m_name{name.data(), name.size()}
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
    for(Setting* setting : m_settings)
    {
        const auto memberIt = doc.FindMember(setting->GetName().c_str());
        if(memberIt != doc.MemberEnd())
            setting->LoadFromJson(&memberIt->value);
        else
            LogWarningF(L"Setting \"%.*hs\" not found. Leaving current value.", STR_TO_FORMAT(setting->GetName()));
    }
    ERR_CATCH_MSG(Format(L"Cannot load settings from file \"%.*s\".", STR_TO_FORMAT(filePath)));
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
    m_startupSettings.LoadFromFile(L"StartupSettings.json");
    ERR_CATCH_MSG(L"Cannot load startup settings.");
}

void SettingManager::LoadLoadSettings()
{
    try
    {
        ERR_TRY;
        LogMessage(L"Loading load settings...");
        m_loadSettings.LoadFromFile(L"LoadSettings.json");
        ERR_CATCH_MSG(L"Cannot load load settings.");
    }
    CATCH_PRINT_ERROR(;);
}

void BoolSetting::LoadFromJson(const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(!LoadValueFromJson(m_value, *realVal))
        LogWarningF(L"Invalid bool setting \"%.*hs\".", STR_TO_FORMAT(GetName()));
}

void FloatSetting::LoadFromJson(const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(!LoadValueFromJson(m_value, *realVal))
        LogWarningF(L"Invalid float setting \"%.*hs\".", STR_TO_FORMAT(GetName()));
}

void UintSetting::LoadFromJson(const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(!LoadValueFromJson(m_value, *realVal))
        LogWarningF(L"Invalid uint setting \"%.*hs\".", STR_TO_FORMAT(GetName()));
}

void IntSetting::LoadFromJson(const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(!LoadValueFromJson(m_value, *realVal))
        LogWarningF(L"Invalid int setting \"%.*hs\".", STR_TO_FORMAT(GetName()));
}

void StringSetting::LoadFromJson(const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(realVal->IsString())
        m_value = ConvertCharsToUnicode(str_view{realVal->GetString(), realVal->GetStringLength()}, CP_UTF8);
    else
        LogWarningF(L"Invalid string setting \"%.*hs\".", STR_TO_FORMAT(GetName()));
}

void LoadStartupSettings()
{
    SettingManager::GetSingleton().LoadStartupSettings();
}
void LoadLoadSettings()
{
    SettingManager::GetSingleton().LoadLoadSettings();
}
