#include "pch.h"
#include "DspLimiter.h"

namespace SaneAudioRenderer
{
    void DspLimiter::Initialize(uint32_t rate, uint32_t channels, bool exclusive)
    {
        m_exclusive = exclusive;
        m_rate = rate > 0 ? rate : 48000;
        m_channels = channels > 0 ? channels : 2;

        m_active = false;
        m_attenuation = 1.0f;
        m_holdcounter = 0;
        m_increase = 0.0f;
    }

    bool DspLimiter::Active()
    {
        return m_active;
    }

    float DspLimiter::RunFrame(float highest)
    {
        float sample = highest;
        if (sample * m_attenuation > 1.0f)
        {
            m_attenuation = 1.0f / sample;
            m_holdcounter = static_cast<int>(std::floor(static_cast<double>(m_rate * m_limiterHold) + 0.5));
            m_increase = std::pow(std::min(sample, 10000.0f), 1.0f / (m_limiterRelease * m_rate));
        }

        float attenuation = m_attenuation;

        if (m_holdcounter > 0)
        {
            m_holdcounter--;
        }
        else if (m_increase > 0.0f)
        {
            m_attenuation *= m_increase;
            if (m_attenuation > 1.0f)
            {
                m_increase = 0.0f;
                m_attenuation = 1.0f;
            }
        }

        return attenuation;
    }

    void DspLimiter::Process(DspChunk& chunk)
    {
        if (chunk.IsEmpty())
            return;

        if ((chunk.GetFormat() != DspFormat::Float &&
             chunk.GetFormat() != DspFormat::Double) || m_channels == 0)
        {
            m_active = false;
            return;
        }

        const size_t totalSamples = chunk.GetSampleCount();
        const size_t frames = totalSamples / m_channels;
        bool activeInChunk = (m_attenuation < 1.0f || m_holdcounter > 0 || m_increase > 0.0f);

        if (chunk.GetFormat() == DspFormat::Double)
        {
            double* data = (double*)chunk.GetData();
            for (size_t f = 0; f < frames; f++)
            {
                double* frame = data + f * m_channels;
                double highest = 0.0;
                for (uint32_t ch = 0; ch < m_channels; ch++)
                {
                    highest = std::max(highest, std::abs(frame[ch]));
                }
                float gain = RunFrame((float)highest);
                if (gain < 1.0f || highest > 1.0)
                {
                    activeInChunk = true;
                    for (uint32_t ch = 0; ch < m_channels; ch++)
                    {
                        double v = frame[ch] * (double)gain;
                        frame[ch] = std::max(-1.0, std::min(v, 1.0));
                    }
                }
            }
        }
        else
        {
            assert(chunk.GetFormat() == DspFormat::Float);
            float* data = (float*)chunk.GetData();
            for (size_t f = 0; f < frames; f++)
            {
                float* frame = data + f * m_channels;
                float highest = 0.0f;
                for (uint32_t ch = 0; ch < m_channels; ch++)
                {
                    highest = std::max(highest, std::abs(frame[ch]));
                }
                float gain = RunFrame(highest);
                if (gain < 1.0f || highest > 1.0f)
                {
                    activeInChunk = true;
                    for (uint32_t ch = 0; ch < m_channels; ch++)
                    {
                        float v = frame[ch] * gain;
                        frame[ch] = std::max(-1.0f, std::min(v, 1.0f));
                    }
                }
            }
        }

        m_active = activeInChunk;
    }

    void DspLimiter::Finish(DspChunk& chunk)
    {
        Process(chunk);
    }
}
