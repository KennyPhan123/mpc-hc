#pragma once

#include "DspBase.h"

namespace SaneAudioRenderer
{
    class DspLimiter final
        : public DspBase
    {
    public:

        DspLimiter() = default;
        DspLimiter(const DspLimiter&) = delete;
        DspLimiter& operator=(const DspLimiter&) = delete;

        void Initialize(uint32_t rate, uint32_t channels, bool exclusive);

        std::wstring Name() override { return L"Limiter"; }

        bool Active() override;

        void Process(DspChunk& chunk) override;
        void Finish(DspChunk& chunk) override;

    private:

        float RunFrame(float highest);

        bool m_exclusive = false;
        uint32_t m_rate = 48000;
        uint32_t m_channels = 2;

        bool m_active = false;
        float m_attenuation = 1.0f;
        int m_holdcounter = 0;
        float m_increase = 0.0f;

        static constexpr float m_limiterHold = 0.025f;
        static constexpr float m_limiterRelease = 0.1f;
    };
}
