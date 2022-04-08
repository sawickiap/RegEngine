#pragma once

struct Time
{
    int64_t m_Value = 0;

    bool IsZero() const { return m_Value == 0; }
    Time operator+(Time rhs) const { return {m_Value + rhs.m_Value}; }
    Time operator-(Time rhs) const { return {m_Value - rhs.m_Value}; }
    Time& operator+=(Time rhs) { m_Value += rhs.m_Value; return *this; }
    Time& operator-=(Time rhs) { m_Value -= rhs.m_Value; return *this; }
    bool operator==(Time rhs) const { return m_Value == rhs.m_Value; }
    bool operator!=(Time rhs) const { return m_Value != rhs.m_Value; }
    bool operator<(Time rhs) const { return m_Value < rhs.m_Value; }
    bool operator>(Time rhs) const { return m_Value > rhs.m_Value; }
    bool operator<=(Time rhs) const { return m_Value <= rhs.m_Value; }
    bool operator>=(Time rhs) const { return m_Value >= rhs.m_Value; }
};

void InitTime();

Time Now();

// Use T = float, double.
template<typename T>
T TimeToSeconds(Time t);
template<typename T>
Time SecondsToTime(T sec);

// Use T = float, double, int32_t, int64_t, uint32_t, uint64_t.
template<typename T>
T TimeToMilliseconds(Time t);
template<typename T>
Time MillisecondsToTime(T ms);

// m_AbsoluteStartTime = fetched using Now() or 0 if uninitialized. Every other member is relative to this one.
// Time = time since AbsoluteStartTime.
// DeltaTime = time since previous frame.
struct TimeData
{
    Time m_AbsoluteStartTime = {0};
    Time m_PreviousTime = {0};
    Time m_Time = {0};
    Time m_DeltaTime = {0};
    float m_Time_Float = 0.f;
    float m_DeltaTime_Float = 0.f;
    uint32_t m_FrameIndex = 0;

    void Start(Time now);
    void NewFrameFromNow(Time now);
    void NewFrameFromDelta(Time delta);
};
