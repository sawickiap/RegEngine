#include "BaseUtils.hpp"
#include "Settings.hpp"
#define RAPIDJSON_HAS_STDSTRING 1
#include "../ThirdParty/rapidjson/include/rapidjson/document.h"
#include "../ThirdParty/rapidjson/include/rapidjson/writer.h"
#include "../ThirdParty/rapidjson/include/rapidjson/stringbuffer.h"

using namespace rapidjson;
void foo()
{
    auto fileContents = LoadFile(L"StartupSettings.json");

    Document doc;
    doc.Parse<kParseCommentsFlag | kParseTrailingCommasFlag | kParseNanAndInfFlag | kParseEscapedApostropheFlag>(
        fileContents.data(), fileContents.size());
    assert(doc.IsObject());
    assert(doc.HasMember("foo"));
    assert(doc["foo"].IsNumber());
    assert(doc["foo"].GetInt() == 1);

    /*
    // 1. Parse a JSON string into DOM.
    const char* json = "{\"project\":\"rapidjson\",\"stars\":10}";
    Document d;
    d.Parse(json);
 
    // 2. Modify it by DOM.
    Value& s = d["stars"];
    s.SetInt(s.GetInt() + 1);
 
    // 3. Stringify the DOM
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    d.Accept(writer);
 
    // Output {"project":"rapidjson","stars":11}
    */
}

class SettingCollection
{
public:
    void Register(Setting* setting)
    {
        m_settings.push_back(setting);
    }
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

private:
    SettingCollection m_startupSettings;
};

SettingManager& SettingManager::GetSingleton()
{
    static SettingManager mgr;
    return mgr;
}

Setting::Setting(SettingCategory category, const wstr_view& name) :
    m_name{name.data(), name.size()}
{
    SettingManager::GetSingleton().Register(category, this);
}

template<typename T>
ScalarSetting<T>::ScalarSetting(SettingCategory category, const wstr_view& name, T defaultValue) :
    Setting(category, name),
    m_value(defaultValue)
{
}

template<typename T>
NumericSetting<T>::NumericSetting(SettingCategory category, const wstr_view& name, T defaultValue, T minValue, T maxValue) :
    ScalarSetting<T>(category, name, defaultValue),
    m_minValue{minValue},
    m_maxValue{maxValue}
{
}
