#pragma once
void foo();

enum class SettingCategory
{
    Startup,
    Count,
};

class Setting
{
public:
    Setting(SettingCategory category, const wstr_view& name);
    virtual ~Setting() = default;
private:
    const wstring m_name;
};

template<typename T>
class ScalarSetting : public Setting
{
public:
    ScalarSetting(SettingCategory category, const wstr_view& name, T defaultValue);
    T GetValue() const { return m_value; }
    void SetValue(T val) { m_value = val; }
private:
    T m_value;
};

template<typename T>
class NumericSetting : public ScalarSetting<T>
{
public:
    NumericSetting(SettingCategory category, const wstr_view& name, T defaultValue, T minValue, T maxValue);
private:
    T m_minValue, m_maxValue;
};
