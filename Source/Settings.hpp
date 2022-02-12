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
    const string& GetName() const { return m_Name; }

protected:
    friend class SettingCollection;
    // On error: prints warning.
    // jsonVal type: const rapidjson::Value*
    virtual void LoadFromJSON(const void* jsonVal) = 0;

private:
    const string m_Name;
};

template<typename T>
class ScalarSetting : public Setting
{
public:
    ScalarSetting(SettingCategory category, const str_view& name, T defaultValue) :
        Setting(category, name),
        m_Value(defaultValue)
    {
    }
    T GetValue() const { return m_Value; }
    void SetValue(T val) { m_Value = val; }

protected:
    T m_Value;
};

class BoolSetting : public ScalarSetting<bool>
{
public:
    BoolSetting(SettingCategory category, const str_view& name, bool defaultValue) :
        ScalarSetting<bool>(category, name, defaultValue)
    {
    }

protected:
    void LoadFromJSON(const void* jsonVal) override;
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
    void LoadFromJSON(const void* jsonVal) override;
};

class UintSetting : public NumericSetting<uint32_t>
{
public:
    UintSetting(SettingCategory category, const str_view& name, uint32_t defaultValue) :
        NumericSetting<uint32_t>(category, name, defaultValue)
    {
    }

protected:
    void LoadFromJSON(const void* jsonVal) override;
};

class IntSetting : public NumericSetting<int32_t>
{
public:
    IntSetting(SettingCategory category, const str_view& name, uint32_t defaultValue) :
        NumericSetting<int32_t>(category, name, defaultValue)
    {
    }

protected:
    void LoadFromJSON(const void* jsonVal) override;
};

template<typename VecT>
bool LoadVecFromJSON(VecT& outVec, const void* jsonVal);
template<typename MatT>
bool LoadMatFromJSON(MatT& outMat, const void* jsonVal);

template<typename VecT>
class VecSetting : public ScalarSetting<VecT>
{
public:
    VecSetting(SettingCategory category, const str_view& name, const VecT& defaultValue) :
        ScalarSetting<VecT>(category, name, defaultValue)
    {
    }

protected:
    void LoadFromJSON(const void* jsonVal) override
    {
        // `this` is needed due to a compiler bug!
        VecT vec;
        if(LoadVecFromJSON<VecT>(vec, jsonVal))
            this->m_Value = vec;
        else
            LogWarningF(L"Invalid vector setting \"{}\".", str_view(this->GetName()));
    }
};

/*
JSON can be either like for vector of floats e.g. [1.0, 0.0, 0.5, 1.0] or string
in format "RRGGBBAA" or "#RRGGBBAA", e.g. "FF0088FF".
Floats are assumed to be 0..1 but can have any values really.
Floats are in linear space and loaded as-is.
String format is assumed in sRGB space and converted to linear.
In both cases, alpha can be omitted, which is then assumed to be 1.0.
*/
class ColorSetting : public VecSetting<vec4>
{
public:
    ColorSetting(SettingCategory category, const str_view& name, const vec4& defaultValue) :
        VecSetting<vec4>(category, name, defaultValue)
    {
    }

protected:
    void LoadFromJSON(const void* jsonVal) override;
};

template<typename MatT>
class MatSetting : public ScalarSetting<MatT>
{
public:
    MatSetting(SettingCategory category, const str_view& name, const MatT& defaultValue) :
        ScalarSetting<MatT>(category, name, defaultValue)
    {
    }

protected:
    void LoadFromJSON(const void* jsonVal) override
    {
        // `this` is needed due to a compiler bug!
        MatT mat;
        if(LoadMatFromJSON<MatT>(mat, jsonVal))
            this->m_Value = mat;
        else
            LogWarningF(L"Invalid matrix setting \"{}\".", str_view(this->GetName()));
    }
};

class StringSetting : public Setting
{
public:
    StringSetting(SettingCategory category, const str_view& name, const wstr_view& defaultValue = wstr_view()) :
        Setting(category, name),
        m_Value{defaultValue.data(), defaultValue.size()}
    {
    }
    const wstring& GetValue() const { return m_Value; }
    void SetValue(const wstr_view& v) { v.to_string(m_Value); }

protected:
    void LoadFromJSON(const void* jsonVal) override;

private:
    wstring m_Value;
};

class StringSequenceSetting : public Setting
{
public:
    std::vector<string> m_Strings;

    StringSequenceSetting(SettingCategory category, const str_view& name);

protected:
    void LoadFromJSON(const void* jsonVal) override;
};

// On error throws Exception.
void LoadStartupSettings();
// On error prints error.
void LoadLoadSettings();
