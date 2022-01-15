#pragma once

enum class SettingCategory
{
    // Loaded from file "StartupSettings.json" on startup, only once.
    // Should not be changed during runtime. Never saved to a file.
    Startup,
    // Loaded from file "LoadSettings.json" on startup and after refresh with F5.
    // Should not be changed during runtime. Never saved to a file.
    Load,
    Count,
};

class Setting
{
public:
    Setting(SettingCategory category, const str_view& name);
    virtual ~Setting() = default;
    const string& GetName() const { return m_name; }

protected:
    friend class SettingCollection;
    // On error: prints warning.
    // jsonVal type: const rapidjson::Value*
    virtual void LoadFromJson(const void* jsonVal) = 0;

private:
    const string m_name;
};

template<typename T>
class ScalarSetting : public Setting
{
public:
    ScalarSetting(SettingCategory category, const str_view& name, T defaultValue) :
        Setting(category, name),
        m_value(defaultValue)
    {
    }
    T GetValue() const { return m_value; }
    void SetValue(T val) { m_value = val; }

protected:
    T m_value;
};

class BoolSetting : public ScalarSetting<bool>
{
public:
    BoolSetting(SettingCategory category, const str_view& name, bool defaultValue) :
        ScalarSetting<bool>(category, name, defaultValue)
    {
    }

protected:
    virtual void LoadFromJson(const void* jsonVal) override;
};

template<typename T>
class NumericSetting : public ScalarSetting<T>
{
public:
    NumericSetting(SettingCategory category, const str_view& name, T defaultValue) :
        ScalarSetting<T>(category, name, defaultValue)
    {
    }
};

class FloatSetting : public NumericSetting<float>
{
public:
    FloatSetting(SettingCategory category, const str_view& name, float defaultValue) :
        NumericSetting<float>(category, name, defaultValue)
    {
    }

protected:
    virtual void LoadFromJson(const void* jsonVal) override;
};

class UintSetting : public NumericSetting<uint32_t>
{
public:
    UintSetting(SettingCategory category, const str_view& name, uint32_t defaultValue) :
        NumericSetting<uint32_t>(category, name, defaultValue)
    {
    }

protected:
    virtual void LoadFromJson(const void* jsonVal) override;
};

class IntSetting : public NumericSetting<int32_t>
{
public:
    IntSetting(SettingCategory category, const str_view& name, uint32_t defaultValue) :
        NumericSetting<int32_t>(category, name, defaultValue)
    {
    }

protected:
    virtual void LoadFromJson(const void* jsonVal) override;
};

template<typename VecT>
bool LoadVecFromJson(VecT& outVec, const void* jsonVal);

template<typename VecT>
class VecSetting : public ScalarSetting<VecT>
{
public:
    VecSetting(SettingCategory category, const str_view& name, const VecT& defaultValue) :
        ScalarSetting<VecT>(category, name, defaultValue)
    {
    }

protected:
    virtual void LoadFromJson(const void* jsonVal) override
    {
        // `this` is needed due to a compiler bug!
        VecT vec;
        if(LoadVecFromJson<VecT>(vec, jsonVal))
            this->m_value = vec;
        else
            LogWarningF(L"Invalid vector setting \"%.*hs\".", STR_TO_FORMAT(this->GetName()));
    }
};

class StringSetting : public Setting
{
public:
    StringSetting(SettingCategory category, const str_view& name, const wstr_view& defaultValue = wstr_view()) :
        Setting(category, name),
        m_value{defaultValue.data(), defaultValue.size()}
    {
    }
    const wstring& GetValue() const { return m_value; }
    void SetValue(const wstr_view& v) { v.to_string(m_value); }

protected:
    virtual void LoadFromJson(const void* jsonVal) override;

private:
    wstring m_value;
};

// On error throws Exception.
void LoadStartupSettings();
// On error prints error.
void LoadLoadSettings();
