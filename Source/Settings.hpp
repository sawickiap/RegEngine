#pragma once

enum class SettingCategory
{
    Startup,
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

template<typename T>
class NumericSetting : public ScalarSetting<T>
{
public:
    NumericSetting(SettingCategory category, const str_view& name, T defaultValue,
        T minValue = std::numeric_limits<T>::min(),
        T maxValue = std::numeric_limits<T>::max()) :
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
    virtual void LoadFromJson(const void* jsonVal);
};

class UintSetting : public NumericSetting<uint32_t>
{
public:
    UintSetting(SettingCategory category, const str_view& name, uint32_t defaultValue) :
        NumericSetting<uint32_t>(category, name, defaultValue)
    {
    }

protected:
    virtual void LoadFromJson(const void* jsonVal);
};

class IntSetting : public NumericSetting<int32_t>
{
public:
    IntSetting(SettingCategory category, const str_view& name, uint32_t defaultValue) :
        NumericSetting<int32_t>(category, name, defaultValue)
    {
    }

protected:
    virtual void LoadFromJson(const void* jsonVal);
};

class StringSetting : public Setting
{
public:
    StringSetting(SettingCategory category, const str_view& name, const wstr_view& defaultValue) :
        Setting(category, name),
        m_value{defaultValue.data(), defaultValue.size()}
    {
    }
    const wstring& GetValue() const { return m_value; }
    void SetValue(const wstr_view& v) { v.to_string(m_value); }

protected:
    virtual void LoadFromJson(const void* jsonVal);

private:
    wstring m_value;
};


void LoadStartupSettings();
