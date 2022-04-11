#include "BaseUtils.hpp"
#include "Settings.hpp"
#include "SmallFileCache.hpp"
#include "ImGuiUtils.hpp"
#include "Streams.hpp"
#define RAPIDJSON_HAS_STDSTRING 1
#include "../ThirdParty/rapidjson/include/rapidjson/document.h"
#include "../ThirdParty/rapidjson/include/rapidjson/prettywriter.h"
#include "../ThirdParty/rapidjson/include/rapidjson/stringbuffer.h"
#include "../ThirdParty/rapidjson/include/rapidjson/allocators.h"
#include "../ThirdParty/rapidjson/include/rapidjson/error/en.h"

static const wchar_t* const STARTUP_SETTINGS_FILE_NAME = L"StartupSettings.json";
static const wchar_t* const LOAD_SETTINGS_FILE_NAME = L"LoadSettings.json";
static const wchar_t* const RUNTIME_SETTINGS_FILE_NAME = L"RuntimeSettings.json";

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

static void SaveValueToJSON(rapidjson::Value& jsonVal, bool val)
{
    jsonVal.SetBool(val);
}
static void SaveValueToJSON(rapidjson::Value& jsonVal, uint32_t val)
{
    jsonVal.SetUint(val);
}
static void SaveValueToJSON(rapidjson::Value& jsonVal, int32_t val)
{
    jsonVal.SetInt(val);
}
static void SaveValueToJSON(rapidjson::Value& jsonVal, float val)
{
    jsonVal.SetFloat(val);
}

template<typename VecT>
bool LoadVecFromJSON(VecT& outVec, const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(!realVal->IsArray())
        return false;
    auto array = realVal->GetArray();
    constexpr auto componentCount = VecT::length();
    if(array.Size() != (rapidjson::SizeType)componentCount)
        return false;
    for(rapidjson::SizeType i = 0; i < (rapidjson::SizeType)componentCount; ++i)
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
    constexpr auto colCount = outMat.length();
    const auto rowCount = outMat[0].length();
    if(array.Size() != (rapidjson::SizeType)(colCount * rowCount))
        return false;
    for(glm::length_t row = 0, i = 0; row < rowCount; ++row)
    {
        for(glm::length_t col = 0; col < colCount; ++col, ++i)
        {
            if(!LoadValueFromJSON(outMat[col][row], array[i]))
                return false;
        }
    }
    return true;
}

template<typename VecT>
void SaveVecToJSON(void* jsonVal, void* jsonAllocator, const VecT& vec)
{
    rapidjson::Value* realVal = (rapidjson::Value*)jsonVal;
    auto realAllocator = (rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>*)jsonAllocator;
    realVal->SetArray();
    constexpr auto componentCount = VecT::length();
    realVal->Reserve((rapidjson::SizeType)componentCount, *realAllocator);
    for(glm::length_t i = 0; i < (glm::length_t)componentCount; ++i)
    {
        rapidjson::Value itemVal;
        SaveValueToJSON(itemVal, vec[i]);
        realVal->PushBack(itemVal, *realAllocator);
    }
}

template<typename MatT>
void SaveMatToJSON(void* jsonVal, void* jsonAllocator, const MatT& mat)
{
    rapidjson::Value* realVal = (rapidjson::Value*)jsonVal;
    auto realAllocator = (rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>*)jsonAllocator;
    realVal->SetArray();
    constexpr auto colCount = mat.length();
    const auto rowCount = mat[0].length();
    realVal->Reserve((rapidjson::SizeType)(colCount * rowCount), *realAllocator);
    for(glm::length_t row = 0, i = 0; row < (glm::length_t)rowCount; ++row)
    {
        for(glm::length_t col = 0; col < (glm::length_t)colCount; ++col, ++i)
        {
            rapidjson::Value itemVal;
            SaveValueToJSON(itemVal, mat[col][row]);
            realVal->PushBack(itemVal, *realAllocator);
        }
    }
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

template void SaveVecToJSON<glm::vec2>(void* jsonVal, void* jsonAllocator, const glm::vec2& vec);
template void SaveVecToJSON<glm::vec3>(void* jsonVal, void* jsonAllocator, const glm::vec3& vec);
template void SaveVecToJSON<glm::vec4>(void* jsonVal, void* jsonAllocator, const glm::vec4& vec);
template void SaveVecToJSON<glm::uvec2>(void* jsonVal, void* jsonAllocator, const glm::uvec2& vec);
template void SaveVecToJSON<glm::uvec3>(void* jsonVal, void* jsonAllocator, const glm::uvec3& vec);
template void SaveVecToJSON<glm::uvec4>(void* jsonVal, void* jsonAllocator, const glm::uvec4& vec);
template void SaveVecToJSON<glm::ivec2>(void* jsonVal, void* jsonAllocator, const glm::ivec2& vec);
template void SaveVecToJSON<glm::ivec3>(void* jsonVal, void* jsonAllocator, const glm::ivec3& vec);
template void SaveVecToJSON<glm::ivec4>(void* jsonVal, void* jsonAllocator, const glm::ivec4& vec);
template void SaveVecToJSON<glm::bvec2>(void* jsonVal, void* jsonAllocator, const glm::bvec2& vec);
template void SaveVecToJSON<glm::bvec3>(void* jsonVal, void* jsonAllocator, const glm::bvec3& vec);
template void SaveVecToJSON<glm::bvec4>(void* jsonVal, void* jsonAllocator, const glm::bvec4& vec);

template void SaveMatToJSON<glm::mat4>(void* jsonVal, void* jsonAllocator, const glm::mat4& mat);

template<>
void ImGuiVectorSetting<glm::vec2>(const char* label, glm::vec2& inoutVec)
{
    ImGui::InputFloat2(label, glm::value_ptr(inoutVec));
}
template<>
void ImGuiVectorSetting<glm::vec3>(const char* label, glm::vec3& inoutVec)
{
    ImGui::InputFloat3(label, glm::value_ptr(inoutVec));
}
template<>
void ImGuiVectorSetting<glm::vec4>(const char* label, glm::vec4& inoutVec)
{
    ImGui::InputFloat4(label, glm::value_ptr(inoutVec));
}
template<>
void ImGuiVectorSetting<glm::uvec2>(const char* label, glm::uvec2& inoutVec)
{
    if(inoutVec.x <= (uint32_t)INT_MAX && inoutVec.y <= (uint32_t)INT_MAX)
    {
        ivec2 i{(int32_t)inoutVec.x, (int32_t)inoutVec.y};
        if(ImGui::InputInt2(label, glm::value_ptr(i)))
        {
            if(i.x >= 0 && i.y >= 0)
            {
                inoutVec.x = (uint32_t)i.x;
                inoutVec.y = (uint32_t)i.y;
            }
        }
    }
    else
        ImGui::LabelText(label, "VALUE OUT OF BOUNDS.");
}
template<>
void ImGuiVectorSetting<glm::uvec3>(const char* label, glm::uvec3& inoutVec)
{
    if(inoutVec.x <= (uint32_t)INT_MAX && inoutVec.y <= (uint32_t)INT_MAX &&
        inoutVec.z <= (uint32_t)INT_MAX)
    {
        ivec3 i{(int32_t)inoutVec.x, (int32_t)inoutVec.y, (int32_t)inoutVec.z};
        if(ImGui::InputInt3(label, glm::value_ptr(i)))
        {
            if(i.x >= 0 && i.y >= 0 && i.z >= 0)
            {
                inoutVec.x = (uint32_t)i.x;
                inoutVec.y = (uint32_t)i.y;
                inoutVec.z = (uint32_t)i.z;
            }
        }
    }
    else
        ImGui::LabelText(label, "VALUE OUT OF BOUNDS.");
}
template<>
void ImGuiVectorSetting<glm::uvec4>(const char* label, glm::uvec4& inoutVec)
{
    if(inoutVec.x <= (uint32_t)INT_MAX && inoutVec.y <= (uint32_t)INT_MAX &&
        inoutVec.z <= (uint32_t)INT_MAX && inoutVec.w <= (uint32_t)INT_MAX)
    {
        ivec4 i{(int32_t)inoutVec.x, (int32_t)inoutVec.y, (int32_t)inoutVec.z, (int32_t)inoutVec.w};
        if(ImGui::InputInt4(label, glm::value_ptr(i)))
        {
            if(i.x >= 0 && i.y >= 0 && i.z >= 0 && i.w >= 0)
            {
                inoutVec.x = (uint32_t)i.x;
                inoutVec.y = (uint32_t)i.y;
                inoutVec.z = (uint32_t)i.z;
                inoutVec.w = (uint32_t)i.w;
            }
        }
    }
    else
        ImGui::LabelText(label, "VALUE OUT OF BOUNDS.");
}
template<>
void ImGuiVectorSetting<glm::ivec2>(const char* label, glm::ivec2& inoutVec)
{
    ImGui::InputInt2(label, glm::value_ptr(inoutVec));
}
template<>
void ImGuiVectorSetting<glm::ivec3>(const char* label, glm::ivec3& inoutVec)
{
    ImGui::InputInt3(label, glm::value_ptr(inoutVec));
}
template<>
void ImGuiVectorSetting<glm::ivec4>(const char* label, glm::ivec4& inoutVec)
{
    ImGui::InputInt4(label, glm::value_ptr(inoutVec));
}

// If error, returns UINT8_MAX.
static uint8_t ParseHexadecimalDigit(char ch)
{
    if(ch >= '0' && ch <= '9')
        return (uint8_t)(ch - '0');
    else if(ch >= 'A' && ch <= 'Z')
        return (uint8_t)(ch - 'A' + 10);
    else if(ch >= 'a' && ch <= 'z')
        return (uint8_t)(ch - 'a' + 10);
    else
        return UINT8_MAX;
}

// If error, returns FLT_MAX.
static float ParseStringColorComponent(char upperChar, char lowerChar, bool convertSRGBToLinear)
{
    const uint8_t upperCharVal = ParseHexadecimalDigit(upperChar);
    if(upperCharVal == UINT8_MAX)
        return FLT_MAX;
    const uint8_t lowerCharVal = ParseHexadecimalDigit(lowerChar);
    if(lowerCharVal == UINT8_MAX)
        return FLT_MAX;
    float result = ((float)upperCharVal * 16.f + (float)lowerCharVal) / 255.f;
    if(convertSRGBToLinear)
        result = SRGBToLinear(result);
    return result;
}

static bool ParseStringColor(vec3& outColor_Linear, const str_view& str)
{
    if(str.empty())
        return false;
    str_view str2 = str;
    if(str2[0] == '#')
        str2 = str_view(str2, 1);
    const size_t len = str2.length();
    if(len != 6)
        return false;
    outColor_Linear.r = ParseStringColorComponent(str2[0], str2[1], true);
    if(outColor_Linear.r == FLT_MAX)
        return false;
    outColor_Linear.g = ParseStringColorComponent(str2[2], str2[3], true);
    if(outColor_Linear.g == FLT_MAX)
        return false;
    outColor_Linear.b = ParseStringColorComponent(str2[4], str2[5], true);
    if(outColor_Linear.b == FLT_MAX)
        return false;
    return true;
}

static bool ParseStringColor(vec4& outColor_Linear, const str_view& str)
{
    if(str.empty())
        return false;
    str_view str2 = str;
    if(str2[0] == '#')
        str2 = str_view(str2, 1);
    const size_t len = str2.length();
    bool hasAlpha = false;
    if(len == 8)
        hasAlpha = true;
    else if(len != 6)
        return false;
    outColor_Linear.r = ParseStringColorComponent(str2[0], str2[1], true);
    if(outColor_Linear.r == FLT_MAX)
        return false;
    outColor_Linear.g = ParseStringColorComponent(str2[2], str2[3], true);
    if(outColor_Linear.g == FLT_MAX)
        return false;
    outColor_Linear.b = ParseStringColorComponent(str2[4], str2[5], true);
    if(outColor_Linear.b == FLT_MAX)
        return false;
    if(hasAlpha)
    {
        outColor_Linear.a = ParseStringColorComponent(str2[6], str2[7], false);
        if(outColor_Linear.a == FLT_MAX)
            return false;
    }
    else
        outColor_Linear.a = 1.f;
    return true;
}

void Vec3ColorSetting::ImGui(bool readOnly)
{
    ImGui::ColorEdit3(GetName().c_str(), glm::value_ptr(m_Value), ImGuiColorEditFlags_Float);
}

void Vec3ColorSetting::LoadFromJSON(const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(realVal->IsString() &&
        ParseStringColor(m_Value, str_view(realVal->GetString(), realVal->GetStringLength())))
    {
        return;
    }
    vec3 vec;
    if(LoadVecFromJSON(vec, jsonVal))
    {
        m_Value = vec;
        return;
    }
    LogWarningF(L"Invalid vec3 color setting \"{}\".", str_view(GetName()));
}

void Vec4ColorSetting::ImGui(bool readOnly)
{
    ImGui::ColorEdit4(GetName().c_str(), glm::value_ptr(m_Value), ImGuiColorEditFlags_Float);
}

void Vec4ColorSetting::LoadFromJSON(const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(realVal->IsString() &&
        ParseStringColor(m_Value, str_view(realVal->GetString(), realVal->GetStringLength())))
    {
        return;
    }
    vec4 vec;
    if(LoadVecFromJSON(vec, jsonVal))
    {
        m_Value = vec;
        return;
    }
    vec3 vec3;
    if(LoadVecFromJSON(vec3, jsonVal))
    {
        m_Value = vec4(vec3.r, vec3.g, vec3.b, 1.f);
        return;
    }
    LogWarningF(L"Invalid vec4 color setting \"{}\".", str_view(GetName()));
}

class SettingCollection
{
public:
    void Register(Setting* setting)
    {
        m_Settings.push_back(setting);
    }

    void LoadFromFile(const wstr_view& filePath);
    void SaveToFile(const wstr_view& filePath) const;
    void ImGui(bool readOnly, const ImGuiTextFilter& filter);
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
        case SettingCategory::Runtime:
            m_RuntimeSettings.Register(setting);
            break;
        case SettingCategory::Volatile:
            m_VolatileSettings.Register(setting);
            break;
        default:
            assert(0);
        }
    }

    void LoadStartupSettings();
    void LoadLoadSettings();
    void LoadRuntimeSettings();
    void SaveRuntimeSettings();
    void ImGui();

private:
    SettingCollection m_StartupSettings;
    SettingCollection m_LoadSettings;
    SettingCollection m_RuntimeSettings;
    SettingCollection m_VolatileSettings;
    ImGuiTextFilter m_ImGuiFilter;
};

Setting::Setting(SettingCategory category, const str_view& name) :
    m_Name{name.data(), name.size()}
{
    SettingManager::GetSingleton().Register(category, this);
}

void Setting::ImGui(bool readOnly)
{
    ImGui::LabelText(GetName().c_str(), "");
}

void SettingCollection::LoadFromFile(const wstr_view& filePath)
{
    ERR_TRY;
    using namespace rapidjson;
    auto fileContents = g_SmallFileCache->LoadFile(filePath);
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

void SettingCollection::SaveToFile(const wstr_view& filePath) const
{
    ERR_TRY;
    using namespace rapidjson;
    Document doc(kObjectType);;
    for(Setting* setting : m_Settings)
    {
        Value val;
        setting->SaveToJSON(&val, &doc.GetAllocator());
        const auto settingName = StringRef(setting->GetName().data(), setting->GetName().length());
        doc.AddMember(settingName, val, doc.GetAllocator());
    }
    StringBuffer stringBuffer;
    {
        PrettyWriter<StringBuffer> writer(stringBuffer);
        doc.Accept(writer);
    }
    SaveFile(filePath, std::span<const char>(stringBuffer.GetString(), stringBuffer.GetLength()));
    ERR_CATCH_MSG(std::format(L"Cannot save settings to file \"{}\".", filePath));
}

void SettingCollection::ImGui(bool readOnly, const ImGuiTextFilter& filter)
{
    ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.6f);
    for(const auto& it : m_Settings)
    {
        if(filter.PassFilter(it->GetName().c_str()))
            it->ImGui(readOnly);
    }
    ImGui::PopItemWidth();
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
    m_StartupSettings.LoadFromFile(STARTUP_SETTINGS_FILE_NAME);
    ERR_CATCH_MSG(L"Cannot load startup settings.");
}

void SettingManager::LoadLoadSettings()
{
    try
    {
        ERR_TRY;
        LogMessage(L"Loading load settings...");
        m_LoadSettings.LoadFromFile(LOAD_SETTINGS_FILE_NAME);
        ERR_CATCH_MSG(L"Cannot load load settings.");
    }
    CATCH_PRINT_ERROR(;);
}

void SettingManager::LoadRuntimeSettings()
{
    try
    {
        ERR_TRY;
        LogMessage(L"Loading runtime settings...");
        m_RuntimeSettings.LoadFromFile(RUNTIME_SETTINGS_FILE_NAME);
        ERR_CATCH_MSG(L"Cannot load runtime settings.");
    }
    CATCH_PRINT_ERROR(;);
}

void SettingManager::SaveRuntimeSettings()
{
    try
    {
        ERR_TRY;
        LogMessage(L"Saving runtime settings...");
        m_RuntimeSettings.SaveToFile(RUNTIME_SETTINGS_FILE_NAME);
        ERR_CATCH_MSG(L"Cannot save runtime settings.");
    }
    CATCH_PRINT_ERROR(;);
}

void SettingManager::ImGui()
{
    if(ImGui::Begin("Settings", &g_SettingsImGuiWindowVisible, 0))
    {
        m_ImGuiFilter.Draw(ICON_FA_MAGNIFYING_GLASS);
        ImGui::SameLine();
        if(ImGui::Button(ICON_FA_XMARK))
            m_ImGuiFilter.Clear();

        if(ImGui::CollapsingHeader("Startup settings"))
        {
            ImGui::TextColored(
                ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled),
                "Loaded from \"StartupSettings.json\" at program startup.");
            ImGui::PushID("StartupSettings");
            ImGui::BeginDisabled();
            m_StartupSettings.ImGui(true, m_ImGuiFilter);
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        if(ImGui::CollapsingHeader("Load settings"))
        {
            ImGui::TextColored(
                ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled),
                "Loaded from \"LoadSettings.json\". Reloaded on [F5].");
            ImGui::PushID("LoadSettings");
            ImGui::BeginDisabled();
            m_LoadSettings.ImGui(true, m_ImGuiFilter);
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        if(ImGui::CollapsingHeader("Runtime settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextColored(
                ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled),
                "Loaded from \"RuntimeSettings.json\" at program startup, saved at exit.");
            ImGui::PushID("RuntimeSettings");
            m_RuntimeSettings.ImGui(true, m_ImGuiFilter);
            ImGui::PopID();
        }
        if(ImGui::CollapsingHeader("Volatile settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextColored(
                ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled),
                "Initialized with default values at program startup, never loaded or saved to a file.");
            ImGui::PushID("VolatileSettings");
            m_VolatileSettings.ImGui(true, m_ImGuiFilter);
            ImGui::PopID();
        }
    }
    ImGui::End();
}

void BoolSetting::ImGui(bool readOnly)
{
    ImGui::Checkbox(GetName().c_str(), &m_Value);
}

void BoolSetting::LoadFromJSON(const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(!LoadValueFromJSON(m_Value, *realVal))
        LogWarningF(L"Invalid bool setting \"{}\".", str_view(GetName()));
}

void BoolSetting::SaveToJSON(void* jsonVal, void* jsonAllocator)
{
    rapidjson::Value* realVal = (rapidjson::Value*)jsonVal;
    realVal->SetBool(m_Value);
}

void FloatSetting::ImGui(bool readOnly)
{
    ImGui::InputFloat(GetName().c_str(), &m_Value, 0.1f, 1.f, "%.3f");
}

void FloatSetting::LoadFromJSON(const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(!LoadValueFromJSON(m_Value, *realVal))
        LogWarningF(L"Invalid float setting \"{}\".", str_view(GetName()));
}

void FloatSetting::SaveToJSON(void* jsonVal, void* jsonAllocator)
{
    rapidjson::Value* realVal = (rapidjson::Value*)jsonVal;
    realVal->SetFloat(m_Value);
}

void UintSetting::ImGui(bool readOnly)
{
    if(m_Value <= (uint32_t)INT_MAX)
    {
        int i = (int)m_Value;
        if(ImGui::InputInt(GetName().c_str(), &i))
        {
            if(i >= 0)
                m_Value = (uint32_t)i;
        }
    }
    else
        ImGui::LabelText(GetName().c_str(), "VALUE OUT OF RANGE FOR INT.");
}

void UintSetting::LoadFromJSON(const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(!LoadValueFromJSON(m_Value, *realVal))
        LogWarningF(L"Invalid uint setting \"{}\".", str_view(GetName()));
}

void UintSetting::SaveToJSON(void* jsonVal, void* jsonAllocator)
{
    rapidjson::Value* realVal = (rapidjson::Value*)jsonVal;
    realVal->SetUint(m_Value);
}

void IntSetting::ImGui(bool readOnly)
{
    ImGui::InputInt(GetName().c_str(), &m_Value);
}

void IntSetting::LoadFromJSON(const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(!LoadValueFromJSON(m_Value, *realVal))
        LogWarningF(L"Invalid int setting \"{}\".", str_view(GetName()));
}

void IntSetting::SaveToJSON(void* jsonVal, void* jsonAllocator)
{
    rapidjson::Value* realVal = (rapidjson::Value*)jsonVal;
    realVal->SetInt(m_Value);
}

void StringSetting::ImGui(bool readOnly)
{
    ImGui::InputText(GetName().c_str(), &m_Value);
}

void StringSetting::LoadFromJSON(const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(realVal->IsString())
        m_Value.assign(realVal->GetString(), realVal->GetStringLength());
    else
        LogWarningF(L"Invalid string setting \"{}\".", str_view(GetName()));
}

void StringSetting::SaveToJSON(void* jsonVal, void* jsonAllocator)
{
    rapidjson::Value* realVal = (rapidjson::Value*)jsonVal;
    realVal->SetString(rapidjson::StringRef(m_Value));
}

StringSequenceSetting::StringSequenceSetting(SettingCategory category, const str_view& name) :
    Setting(category, name)
{

}

void StringSequenceSetting::ImGui(bool readOnly)
{
    if(ImGui::BeginListBox(GetName().c_str()))
    {
        for(const auto& it : m_Strings)
        {
            bool selected = false;
            ImGui::Selectable(it.c_str(), &selected);
        }
        ImGui::EndListBox();
    }
}

void StringSequenceSetting::LoadFromJSON(const void* jsonVal)
{
    const rapidjson::Value* realVal = (const rapidjson::Value*)jsonVal;
    if(realVal->IsArray())
    {
        const auto arr = realVal->GetArray();
        m_Strings.resize(arr.Size());
        bool ok = true;
        for(uint32_t i = 0; i < arr.Size() && ok; ++i)
        {
            if(arr[i].IsString())
            {
                m_Strings[i].assign(arr[i].GetString(), arr[i].GetStringLength());
            }
            else
                ok = false;
        }
        if(ok)
            return;
    }
    m_Strings.clear();
    LogWarningF(L"Invalid string array setting \"{}\".", str_view(GetName()));
}

void StringSequenceSetting::SaveToJSON(void* jsonVal, void* jsonAllocator)
{
    rapidjson::Value* realVal = (rapidjson::Value*)jsonVal;
    auto realAllocator = (rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>*)jsonAllocator;
    realVal->SetArray();
    realVal->Reserve((rapidjson::SizeType)m_Strings.size(), *realAllocator);
    for(const auto& s : m_Strings)
    {
        realVal->PushBack(rapidjson::StringRef(s.data(), s.length()), *realAllocator);
    }
}

void LoadStartupSettings()
{
    SettingManager::GetSingleton().LoadStartupSettings();
}
void LoadLoadSettings()
{
    SettingManager::GetSingleton().LoadLoadSettings();
}
void LoadRuntimeSettings()
{
    SettingManager::GetSingleton().LoadRuntimeSettings();
}

void SaveRuntimeSettings()
{
    SettingManager::GetSingleton().SaveRuntimeSettings();
}

bool g_SettingsImGuiWindowVisible = false;

void SettingsImGui()
{
    if(!g_SettingsImGuiWindowVisible)
        return;
    SettingManager::GetSingleton().ImGui();
}
