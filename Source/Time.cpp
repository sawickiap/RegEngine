#include "BaseUtils.hpp"
#include "Time.hpp"

static LARGE_INTEGER g_PerformanceFrequency;
static double g_PerformanceFrequency_Double;

void InitTime()
{
    CHECK_BOOL(QueryPerformanceFrequency(&g_PerformanceFrequency));
    g_PerformanceFrequency_Double = (double)g_PerformanceFrequency.QuadPart;
}

Time Now()
{
    LARGE_INTEGER i;
    QueryPerformanceCounter(&i);
    return {i.QuadPart};
}

template<typename T>
T TimeToSeconds(Time t)
{
    const double t_double = (double)t.m_Value / g_PerformanceFrequency_Double;
    return (T)t_double;
}
template float TimeToSeconds<float>(Time t);
template double TimeToSeconds<double>(Time t);

template<typename T>
Time SecondsToTime(T sec)
{
    return {(int64_t)((double)sec * g_PerformanceFrequency_Double)};
}
template Time SecondsToTime<float>(float sec);
template Time SecondsToTime<double>(double sec);

template<>
float TimeToMilliseconds<float>(Time t)
{
    return TimeToSeconds<float>(t) * 1000.f;
}
template<>
double TimeToMilliseconds<double>(Time t)
{
    return TimeToSeconds<double>(t) * 1000.;
}
template<>
int32_t TimeToMilliseconds<int32_t>(Time t)
{
    return (int32_t)(t.m_Value * 1000 / g_PerformanceFrequency.QuadPart);
}
template<>
int64_t TimeToMilliseconds<int64_t>(Time t)
{
    return t.m_Value * 1000 / g_PerformanceFrequency.QuadPart;
}
template<>
uint32_t TimeToMilliseconds<uint32_t>(Time t)
{
    return (uint32_t)(t.m_Value * 1000 / g_PerformanceFrequency.QuadPart);
}
template<>
uint64_t TimeToMilliseconds<uint64_t>(Time t)
{
    return (uint64_t)(t.m_Value * 1000 / g_PerformanceFrequency.QuadPart);
}

template<>
Time MillisecondsToTime<float>(float ms)
{
    return {(int64_t)((double)ms * 1e-3 * g_PerformanceFrequency_Double)};
}
template<>
Time MillisecondsToTime<double>(double ms)
{
    return {(int64_t)(ms * 1e-3 * g_PerformanceFrequency_Double)};
}
template<>
Time MillisecondsToTime<int32_t>(int32_t ms)
{
    return {(int64_t)ms * g_PerformanceFrequency.QuadPart / 1000};
}
template<>
Time MillisecondsToTime<uint32_t>(uint32_t ms)
{
    return {(int64_t)ms * g_PerformanceFrequency.QuadPart / 1000};
}
template<>
Time MillisecondsToTime<int64_t>(int64_t ms)
{
    return {ms * g_PerformanceFrequency.QuadPart / 1000};
}
template<>
Time MillisecondsToTime<uint64_t>(uint64_t ms)
{
    return {(int64_t)ms * g_PerformanceFrequency.QuadPart / 1000};
}

void TimeData::Start(Time now)
{
    *this = {};
    m_AbsoluteStartTime = now;
}

void TimeData::NewFrame(Time now)
{
    assert(!m_AbsoluteStartTime.IsZero());
    // Cannot be smaller than previous time.
    Time newTime = std::max(now - m_AbsoluteStartTime, m_PreviousTime);
    m_DeltaTime = newTime - m_Time;
    m_PreviousTime = m_Time;
    m_Time = newTime;
    m_Time_Float = TimeToSeconds<float>(newTime);
    m_DeltaTime_Float = TimeToSeconds<float>(m_DeltaTime);
    ++m_FrameIndex;
}
