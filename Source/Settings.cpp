#include "BaseUtils.hpp"
#include "Settings.hpp"
#define RAPIDJSON_HAS_STDSTRING 1
#include "../ThirdParty/rapidjson/include/rapidjson/document.h"
#include "../ThirdParty/rapidjson/include/rapidjson/writer.h"
#include "../ThirdParty/rapidjson/include/rapidjson/stringbuffer.h"

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
        if(category == SettingCategory::Startup)
            m_startupSettings.Register(setting);
        else
            assert(0);
    }

    void LoadStartupSettings();

private:
    SettingCollection m_startupSettings;
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
    doc.Parse<kParseCommentsFlag | kParseTrailingCommasFlag | kParseNanAndInfFlag | kParseEscapedApostropheFlag | kParseValidateEncodingFlag>(
        fileContents.data(), fileContents.size());
    // TODO check error
    for(Setting* setting : m_settings)
    {
        const auto memberIt = doc.FindMember(setting->GetName().c_str());
        if(memberIt != doc.MemberEnd())
        {
            setting->LoadFromJson(&memberIt->value);
        }
        else
        {
            wprintf(L"WARNING: Setting \"%.*hs\" not found. Leaving current value.\n",
                (int)setting->GetName().size(), setting->GetName().data());
        }
    }
    ERR_CATCH_MSG(Format(L"Cannot load settings from file \"%.*s\".", (int)filePath.size(), filePath.data()));
}

SettingManager& SettingManager::GetSingleton()
{
    static SettingManager mgr;
    return mgr;
}

void SettingManager::LoadStartupSettings()
{
    ERR_TRY;
    wprintf(L"Loading statup settings...\n");
    m_startupSettings.LoadFromFile(L"StartupSettings.json");
    ERR_CATCH_MSG(L"Cannot load startup settings.");
}

void FloatSetting::LoadFromJson(const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(realVal->IsFloat())
        m_value = realVal->GetFloat();
    else
        wprintf(Format(L"WARNING: Invalid float setting \"%.*hs\".\n", (int)GetName().size(), GetName().data()).c_str());
}

void UintSetting::LoadFromJson(const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(realVal->IsUint())
        m_value = realVal->GetUint();
    else
        wprintf(Format(L"WARNING: Invalid uint setting \"%.*hs\".\n", (int)GetName().size(), GetName().data()).c_str());
}

void IntSetting::LoadFromJson(const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(realVal->IsInt())
        m_value = realVal->GetInt();
    else
        wprintf(Format(L"WARNING: Invalid int setting \"%.*hs\".\n", (int)GetName().size(), GetName().data()).c_str());
}

void StringSetting::LoadFromJson(const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(realVal->IsString())
        m_value = ConvertCharsToUnicode(str_view{realVal->GetString(), realVal->GetStringLength()}, CP_UTF8);
    else
        wprintf(Format(L"WARNING: Invalid string setting \"%.*hs\".\n", (int)GetName().size(), GetName().data()).c_str());
}

void LoadStartupSettings()
{
    SettingManager::GetSingleton().LoadStartupSettings();
}
