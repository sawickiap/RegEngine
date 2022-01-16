#pragma once

/*
An abstract algorithm that ensures linear allocation of anything of various size
in a ring-buffer fashion with a twist that calling NewFrame() bulk-frees memory
that has been allocated during frame (current - FrameCount).
*/
template<typename T>
class MultiFrameRingBuffer
{
public:
    void Init(T capacity, uint32_t frameCount)
    {
        assert(m_FrameCount == 0 && frameCount > 0);
        m_Capacity = capacity;
        m_FrameCount = frameCount;
    }

    void NewFrame()
    {
        m_FrameIndex = (m_FrameIndex + 1) % m_FrameCount;
        const T sizeToFree = m_PerFrameSize[m_FrameIndex];
        m_Back = (m_Back + sizeToFree) % m_Capacity;
        m_Size -= sizeToFree;
        m_PerFrameSize[m_FrameIndex] = 0;
    }

    bool Allocate(T size, T& outOffset)
    {
        // Such allocation could never be done with this capacity - early return.
        if(size > m_Capacity)
            return false;
        /*
        Allocated is Back...Front:

        |.....|###############|............|
             Back           Front
        */
        if(m_Front > m_Back || (m_Front == m_Back && m_Size == 0))
        {
            // Special empty case - move pointers to 0 to make room for bigger allocation.
            if(m_Size == 0)
                m_Front = m_Back = 0;

            /*
            OK to allocate after Front:

            |.....|###############|aaaaa|......|
                 Back           Front
            */
            if(m_Front + size <= m_Capacity)
            {
                outOffset = m_Front;
                m_Front = (m_Front + size) % m_Capacity;
                m_Size += size;
                m_PerFrameSize[m_FrameIndex] += size;
                return true;
            }
            else
            {
                /*
                * OK to allocate after wrapping around:
                
                |aaaaaaa|...|###############|RRRRR|
                           Back           Front
                */
                if(size <= m_Back)
                {
                    const T unusedReminderAtEnd = m_Capacity - m_Front;
                    m_Size += unusedReminderAtEnd;
                    m_PerFrameSize[m_FrameIndex] += unusedReminderAtEnd;
                    outOffset = size;
                    m_Front = size % m_Capacity;
                    m_Size += size;
                    m_PerFrameSize[m_FrameIndex] += size;
                    return true;
                }
                // Wouldn't fit, neither at the end before Capacity nor at the beginning before Back.
                else
                    return false;
            }
        }
        /*
        Allocated is 0..Front and Back..Capacity:
        
        |#####|...............|############|
             Front           Back
        */
        else
        {
            /*
            OK to allocate after Front:

            |#####|aaaaaa|........|############|
                 Front           Back
            */
            if(m_Front + size <= m_Back)
            {
                outOffset = m_Front;
                m_Front = (m_Front + size) % m_Capacity;
                m_Size += size;
                m_PerFrameSize[m_FrameIndex] += size;
                return true;
            }
            // Woudln't fit.
            else
                return false;
        }
    }

private:
    T m_Capacity = 0;
    T m_Back = 0;
    T m_Front = 0;
    T m_Size = 0;
    T m_PerFrameSize[MAX_FRAME_COUNT] = {};
    uint32_t m_FrameCount = 0;
    uint32_t m_FrameIndex = 0;
};
