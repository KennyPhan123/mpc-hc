/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2015, 2017 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"
#include <algorithm>
#include <math.h>
#include <MMReg.h>
#include "AudioSwitcher.h"
#include "../../../DSUtil/DSUtil.h"
#include "../../../DSUtil/AudioTools.h"

#ifdef STANDALONE_FILTER
#include <InitGuid.h>
#endif
#include "moreuuids.h"

#define NORMALIZATION_REGAIN_STEP      0.05
#define NORMALIZATION_REGAIN_THRESHOLD 0.75

#ifdef STANDALONE_FILTER

const AMOVIESETUP_MEDIATYPE sudPinTypesIn[] = {
    {&MEDIATYPE_Audio, &MEDIASUBTYPE_NULL}
};

const AMOVIESETUP_MEDIATYPE sudPinTypesOut[] = {
    {&MEDIATYPE_Audio, &MEDIASUBTYPE_NULL}
};

const AMOVIESETUP_PIN sudpPins[] = {
    {const_cast<LPWSTR>(L"Input"), FALSE, FALSE, FALSE, FALSE, &CLSID_NULL, nullptr, _countof(sudPinTypesIn), sudPinTypesIn},
    {const_cast<LPWSTR>(L"Output"), FALSE, TRUE, FALSE, FALSE, &CLSID_NULL, nullptr, _countof(sudPinTypesOut), sudPinTypesOut}
};

const AMOVIESETUP_FILTER sudFilter[] = {
    {&__uuidof(CAudioSwitcherFilter), AudioSwitcherName, MERIT_DO_NOT_USE, _countof(sudpPins), sudpPins, CLSID_LegacyAmFilterCategory}
};

CFactoryTemplate g_Templates[] = {
    {sudFilter[0].strName, sudFilter[0].clsID, CreateInstance<CAudioSwitcherFilter>, nullptr, &sudFilter[0]}
};

int g_cTemplates = _countof(g_Templates);

STDAPI DllRegisterServer()
{
    return AMovieDllRegisterServer2(TRUE);
}

STDAPI DllUnregisterServer()
{
    return AMovieDllRegisterServer2(FALSE);
}

#include "../../FilterApp.h"

CFilterApp theApp;

#endif

//
// CAELimiter (Kodi ActiveAE Limiter port: 0ms attack, 25ms hold, 100ms exponential release)
//

CAELimiter::CAELimiter()
    : m_amplify(1.0f)
    , m_attenuation(1.0f)
    , m_samplerate(48000)
    , m_holdcounter(0)
    , m_increase(0.0f)
{
}

void CAELimiter::SetAmplification(float amplify)
{
    m_amplify = std::max(0.0f, std::min(amplify, 1000.0f));
}

void CAELimiter::Reset()
{
    m_attenuation = 1.0f;
    m_holdcounter = 0;
    m_increase = 0.0f;
}

float CAELimiter::Run(const float* frame, int channels)
{
    float highest = 0.0f;
    for (int ch = 0; ch < channels; ch++) {
        float sample = fabsf(frame[ch]);
        if (sample > highest) {
            highest = sample;
        }
    }

    float sample = highest * m_amplify;
    if (sample * m_attenuation > 1.0f) {
        m_attenuation = 1.0f / sample;
        m_holdcounter = static_cast<int>(floor(static_cast<double>(m_samplerate * m_limiterHold) + 0.5));
        m_increase = powf(std::min(sample, 10000.0f), 1.0f / (m_limiterRelease * m_samplerate));
    }

    float attenuation = m_attenuation;

    if (m_holdcounter > 0) {
        m_holdcounter--;
    } else if (m_increase > 0.0f) {
        m_attenuation *= m_increase;
        if (m_attenuation > 1.0f) {
            m_increase = 0.0f;
            m_attenuation = 1.0f;
        }
    }

    return attenuation * m_amplify;
}

//
// CAudioSwitcherFilter (Kodi ActiveAE Engine: CAELimiter + 0..60 dB Volume Amplification + Audio Offset)
//

CAudioSwitcherFilter::CAudioSwitcherFilter(LPUNKNOWN lpunk, HRESULT* phr)
    : CStreamSwitcherFilter(lpunk, phr, __uuidof(this))
    , m_rtAudioTimeShift(0)
    , m_boostFactor(1.0)
    , m_boostdB(0)
    , m_rtNextStart(0)
    , m_rtNextStop(1)
    , m_rtSegmentStart(0)
{
    if (phr) {
        if (FAILED(*phr)) {
            return;
        } else {
            *phr = S_OK;
        }
    }
}

STDMETHODIMP CAudioSwitcherFilter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    return
        QI(IAudioSwitcherFilter)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

HRESULT CAudioSwitcherFilter::CheckMediaType(const CMediaType* pmt)
{
    if (pmt->majortype == MEDIATYPE_Audio && pmt->formattype == FORMAT_WaveFormatEx && pmt->pbFormat) {
        WAVEFORMATEX* wfe = (WAVEFORMATEX*)pmt->pbFormat;
        WORD wBitsPerSample = wfe->wBitsPerSample;
        WORD wFormatTag = wfe->wFormatTag;
        if (wfe->nChannels > 2 && wFormatTag != WAVE_FORMAT_EXTENSIBLE) {
            return VFW_E_INVALIDMEDIATYPE; // iviaudio tries to fool us
        }
        if (wfe->nSamplesPerSec == 0) {
            ASSERT(false);
            return VFW_E_INVALIDMEDIATYPE;
        }
        if (wBitsPerSample == 8 || wBitsPerSample == 16 || wBitsPerSample == 24 || wBitsPerSample == 32) {
            if (wFormatTag == WAVE_FORMAT_PCM || wFormatTag == WAVE_FORMAT_IEEE_FLOAT || wFormatTag == WAVE_FORMAT_DOLBY_AC3_SPDIF || wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
                return S_OK;
            }
        }
    }
    return VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT CAudioSwitcherFilter::Transform(IMediaSample* pIn, IMediaSample* pOut)
{
    CStreamSwitcherInputPin* pInPin = GetInputPin();
    CStreamSwitcherOutputPin* pOutPin = GetOutputPin();
    if (!pInPin || !pOutPin) {
        return __super::Transform(pIn, pOut);
    }

    WAVEFORMATEX* wfe = (WAVEFORMATEX*)pInPin->CurrentMediaType().pbFormat;
    WAVEFORMATEX* wfeout = (WAVEFORMATEX*)pOutPin->CurrentMediaType().pbFormat;
    WAVEFORMATEXTENSIBLE* wfex = (WAVEFORMATEXTENSIBLE*)wfe;

    int bps = wfe->wBitsPerSample >> 3;

    int len = pIn->GetActualDataLength() / (bps * wfe->nChannels);
    if (len < 0 || wfe->nSamplesPerSec == 0 || !wfeout) {
        return S_FALSE;
    }

    REFERENCE_TIME rtStart, rtStop;
    if (SUCCEEDED(pIn->GetTime(&rtStart, &rtStop))) {
        rtStart += m_rtAudioTimeShift;
        rtStop += m_rtAudioTimeShift;
        pOut->SetTime(&rtStart, &rtStop);

        m_rtNextStart = rtStart;
        m_rtNextStop = rtStop;
    } else {
        pOut->SetTime(&m_rtNextStart, &m_rtNextStop);
    }

    REFERENCE_TIME rtDur = 10000000i64 * len / wfe->nSamplesPerSec;

    m_rtNextStart += rtDur;
    m_rtNextStop += rtDur;

    if (pIn->IsDiscontinuity() == S_OK) {
        m_limiter.Reset();
    }

    WORD tag = wfe->wFormatTag;
    bool fPCM = tag == WAVE_FORMAT_PCM || tag == WAVE_FORMAT_EXTENSIBLE && wfex->SubFormat == KSDATAFORMAT_SUBTYPE_PCM;
    bool fFloat = tag == WAVE_FORMAT_IEEE_FLOAT || tag == WAVE_FORMAT_EXTENSIBLE && wfex->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    if (!fPCM && !fFloat) {
        return __super::Transform(pIn, pOut);
    }

    // len = 0 doesn't mean it's failed, return S_OK otherwise might screw the sound
    if (len == 0) {
        pOut->SetActualDataLength(0);
        return S_OK;
    }
    int lenout = (UINT64)len * wfeout->nSamplesPerSec / wfe->nSamplesPerSec;

    BYTE* pDataIn = nullptr;
    BYTE* pDataOut = nullptr;

    HRESULT hr;
    if (FAILED(hr = pIn->GetPointer(&pDataIn))) {
        return hr;
    }
    if (FAILED(hr = pOut->GetPointer(&pDataOut))) {
        return hr;
    }

    if (!pDataIn || !pDataOut) {
        return S_FALSE;
    }

    HRESULT hr2;
    if (S_OK != (hr2 = __super::Transform(pIn, pOut))) {
        return hr2;
    }

    {
        size_t samples = size_t(lenout) * wfeout->nChannels;
        ASSERT((wfe->wBitsPerSample == 24 ? samples * 4 : samples * wfe->wBitsPerSample / 8) <= pOut->GetSize());

        // Kodi ActiveAE amplification + CAELimiter (0ms attack, 25ms hold, 100ms release) + ClampArray [-1.0, 1.0]
        // Runs on every frame so both 0..60 dB Boost and un-normalized 5.1/7.1 -> 2.0 float downmix peaks > 1.0f
        // are smoothly limited without ever clipping or permanently ducking volume.
        m_limiter.SetAmplification((float)m_boostFactor);
        const int channels = wfeout->nChannels;
        const int frames = lenout;

        if (channels > 0 && channels <= AS_MAX_CHANNELS && frames > 0) {
            if (fFloat) {
                if (wfe->wBitsPerSample == 32) {
                    float* p = (float*)pDataOut;
                    for (int f = 0; f < frames; f++) {
                        float* frame = p + (size_t)f * channels;
                        float gain = m_limiter.Run(frame, channels);
                        for (int ch = 0; ch < channels; ch++) {
                            float v = frame[ch] * gain;
                            frame[ch] = std::max(-1.0f, std::min(v, 1.0f));
                        }
                    }
                } else if (wfe->wBitsPerSample == 64) {
                    double* p = (double*)pDataOut;
                    float frameBuf[AS_MAX_CHANNELS];
                    for (int f = 0; f < frames; f++) {
                        double* frame = p + (size_t)f * channels;
                        for (int ch = 0; ch < channels; ch++) {
                            frameBuf[ch] = (float)frame[ch];
                        }
                        float gain = m_limiter.Run(frameBuf, channels);
                        for (int ch = 0; ch < channels; ch++) {
                            double v = frame[ch] * (double)gain;
                            frame[ch] = std::max(-1.0, std::min(v, 1.0));
                        }
                    }
                }
            } else if (fPCM) {
                float frameBuf[AS_MAX_CHANNELS];
                if (wfe->wBitsPerSample == 8) {
                    uint8_t* p = (uint8_t*)pDataOut;
                    for (int f = 0; f < frames; f++) {
                        uint8_t* frame = p + (size_t)f * channels;
                        for (int ch = 0; ch < channels; ch++) {
                            frameBuf[ch] = (float)((int8_t)(frame[ch] ^ 0x80)) / 128.0f;
                        }
                        float gain = m_limiter.Run(frameBuf, channels);
                        if (gain != 1.0f) {
                            for (int ch = 0; ch < channels; ch++) {
                                double d = (double)((int8_t)(frame[ch] ^ 0x80)) * (double)gain;
                                limit((double)INT8_MIN, d, (double)INT8_MAX);
                                frame[ch] = (uint8_t)((int8_t)d ^ 0x80);
                            }
                        }
                    }
                } else if (wfe->wBitsPerSample == 16) {
                    int16_t* p = (int16_t*)pDataOut;
                    for (int f = 0; f < frames; f++) {
                        int16_t* frame = p + (size_t)f * channels;
                        for (int ch = 0; ch < channels; ch++) {
                            frameBuf[ch] = (float)frame[ch] / 32768.0f;
                        }
                        float gain = m_limiter.Run(frameBuf, channels);
                        if (gain != 1.0f) {
                            for (int ch = 0; ch < channels; ch++) {
                                double d = (double)frame[ch] * (double)gain;
                                limit((double)INT16_MIN, d, (double)INT16_MAX);
                                frame[ch] = (int16_t)d;
                            }
                        }
                    }
                } else if (wfe->wBitsPerSample == 24) {
                    for (int f = 0; f < frames; f++) {
                        for (int ch = 0; ch < channels; ch++) {
                            size_t idx = (size_t)f * channels + ch;
                            int32_t s32 = 0;
                            BYTE* b = (BYTE*)&s32;
                            b[1] = pDataOut[idx * 3];
                            b[2] = pDataOut[idx * 3 + 1];
                            b[3] = pDataOut[idx * 3 + 2];
                            frameBuf[ch] = (float)((double)s32 / 2147483648.0);
                        }
                        float gain = m_limiter.Run(frameBuf, channels);
                        if (gain != 1.0f) {
                            for (int ch = 0; ch < channels; ch++) {
                                size_t idx = (size_t)f * channels + ch;
                                int32_t s32 = 0;
                                BYTE* b = (BYTE*)&s32;
                                b[1] = pDataOut[idx * 3];
                                b[2] = pDataOut[idx * 3 + 1];
                                b[3] = pDataOut[idx * 3 + 2];
                                double d = (double)(s32 >> 8) * (double)gain;
                                limit((double)INT24_MIN, d, (double)INT24_MAX);
                                uint32_t u32 = (uint32_t)(int32_t)d;
                                pDataOut[idx * 3] = (BYTE)(u32 & 0xFF);
                                pDataOut[idx * 3 + 1] = (BYTE)((u32 >> 8) & 0xFF);
                                pDataOut[idx * 3 + 2] = (BYTE)((u32 >> 16) & 0xFF);
                            }
                        }
                    }
                } else if (wfe->wBitsPerSample == 32) {
                    int32_t* p = (int32_t*)pDataOut;
                    for (int f = 0; f < frames; f++) {
                        int32_t* frame = p + (size_t)f * channels;
                        for (int ch = 0; ch < channels; ch++) {
                            frameBuf[ch] = (float)((double)frame[ch] / 2147483648.0);
                        }
                        float gain = m_limiter.Run(frameBuf, channels);
                        if (gain != 1.0f) {
                            for (int ch = 0; ch < channels; ch++) {
                                double d = (double)frame[ch] * (double)gain;
                                limit((double)INT32_MIN, d, (double)INT32_MAX);
                                frame[ch] = (int32_t)d;
                            }
                        }
                    }
                }
            }
        }
    }

    pOut->SetActualDataLength(lenout * bps * wfeout->nChannels);

    return S_OK;
}

CMediaType CAudioSwitcherFilter::CreateNewOutputMediaType(CMediaType mt, long& cbBuffer)
{
    CStreamSwitcherInputPin* pInPin = GetInputPin();
    CStreamSwitcherOutputPin* pOutPin = GetOutputPin();
    if (!pInPin || !pOutPin || ((WAVEFORMATEX*)mt.pbFormat)->wFormatTag == WAVE_FORMAT_DOLBY_AC3_SPDIF) {
        return __super::CreateNewOutputMediaType(mt, cbBuffer);
    }

    WAVEFORMATEX* wfe = (WAVEFORMATEX*)pInPin->CurrentMediaType().pbFormat;
    WAVEFORMATEX* wfeout = (WAVEFORMATEX*)mt.pbFormat;
    int bps = wfe->wBitsPerSample >> 3;
    int len = cbBuffer / (bps * wfe->nChannels);
    int lenout = (UINT64)len * wfeout->nSamplesPerSec / wfe->nSamplesPerSec;
    cbBuffer = lenout * bps * wfeout->nChannels;

    return mt;
}

void CAudioSwitcherFilter::OnNewOutputMediaType(const CMediaType& mtIn, const CMediaType& mtOut)
{
    const WAVEFORMATEX* wfeout = (WAVEFORMATEX*)mtOut.pbFormat;

    TRACE(_T("CAudioSwitcherFilter::OnNewOutputMediaType\n"));
    if (wfeout) {
        m_limiter.SetSamplerate(wfeout->nSamplesPerSec);
    }
    m_limiter.Reset();
}

HRESULT CAudioSwitcherFilter::DeliverEndFlush()
{
    TRACE(_T("CAudioSwitcherFilter::DeliverEndFlush\n"));
    m_limiter.Reset();

    return __super::DeliverEndFlush();
}

HRESULT CAudioSwitcherFilter::DeliverNewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
    TRACE(_T("CAudioSwitcherFilter::DeliverNewSegment\n"));

    m_rtSegmentStart = tStart;
    m_limiter.Reset();

    return __super::DeliverNewSegment(tStart, tStop, dRate);
}

// IAudioSwitcherFilter

STDMETHODIMP CAudioSwitcherFilter::GetInputSpeakerConfig(DWORD* pdwChannelMask)
{
    CheckPointer(pdwChannelMask, E_POINTER);

    *pdwChannelMask = 0;

    CStreamSwitcherInputPin* pInPin = GetInputPin();
    if (!pInPin || !pInPin->IsConnected()) {
        return E_UNEXPECTED;
    }

    WAVEFORMATEX* wfe = (WAVEFORMATEX*)pInPin->CurrentMediaType().pbFormat;

    if (wfe->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        WAVEFORMATEXTENSIBLE* wfex = (WAVEFORMATEXTENSIBLE*)wfe;
        *pdwChannelMask = wfex->dwChannelMask;
    } else {
        *pdwChannelMask = 0;
    }

    return S_OK;
}

STDMETHODIMP CAudioSwitcherFilter::GetSpeakerConfig(bool* pfCustomChannelMapping, DWORD pSpeakerToChannelMap[AS_MAX_CHANNELS][AS_MAX_CHANNELS])
{
    if (pfCustomChannelMapping) {
        *pfCustomChannelMapping = false;
    }
    if (pSpeakerToChannelMap) {
        ZeroMemory(pSpeakerToChannelMap, sizeof(DWORD) * AS_MAX_CHANNELS * AS_MAX_CHANNELS);
    }
    return S_OK;
}

STDMETHODIMP CAudioSwitcherFilter::SetSpeakerConfig(bool /*fCustomChannelMapping*/, DWORD /*pSpeakerToChannelMap*/[AS_MAX_CHANNELS][AS_MAX_CHANNELS])
{
    return S_OK;
}

STDMETHODIMP_(int) CAudioSwitcherFilter::GetNumberOfInputChannels()
{
    CStreamSwitcherInputPin* pInPin = GetInputPin();
    if (pInPin) {
        WAVEFORMATEX* wfex = (WAVEFORMATEX*)pInPin->CurrentMediaType().pbFormat;
        if (wfex) {
            return wfex->nChannels;
        }
    }
    return 0;
}

STDMETHODIMP_(REFERENCE_TIME) CAudioSwitcherFilter::GetAudioTimeShift()
{
    return m_rtAudioTimeShift;
}

STDMETHODIMP CAudioSwitcherFilter::SetAudioTimeShift(REFERENCE_TIME rtAudioTimeShift)
{
    m_rtAudioTimeShift = rtAudioTimeShift;
    return S_OK;
}

STDMETHODIMP CAudioSwitcherFilter::GetNormalizeBoost(bool& fNormalize, bool& fNormalizeRecover, float& boost_dB)
{
    fNormalize = false;
    fNormalizeRecover = false;
    boost_dB = (float)m_boostdB;
    return S_OK;
}

STDMETHODIMP CAudioSwitcherFilter::SetNormalizeBoost(bool /*fNormalize*/, bool /*fNormalizeRecover*/, float boost_dB)
{
    float clamped_dB = std::max(0.0f, std::min(boost_dB, 60.0f));
    m_boostdB = (UINT)(clamped_dB + 0.5f);
    m_boostFactor = pow(10.0, (double)clamped_dB / 20.0);
    return S_OK;
}

STDMETHODIMP CAudioSwitcherFilter::GetNormalizeBoost2(bool& fNormalize, UINT& nMaxNormFactor, bool& fNormalizeRecover, UINT& boost)
{
    fNormalize = false;
    nMaxNormFactor = 100;
    fNormalizeRecover = false;
    boost = m_boostdB;
    return S_OK;
}

STDMETHODIMP CAudioSwitcherFilter::SetNormalizeBoost2(bool /*fNormalize*/, UINT /*nMaxNormFactor*/, bool /*fNormalizeRecover*/, UINT boost)
{
    m_boostdB = std::min<UINT>(boost, 60);
    m_boostFactor = pow(10.0, (double)m_boostdB / 20.0);
    return S_OK;
}

STDMETHODIMP CAudioSwitcherFilter::GetReplayGain(bool& bEnabled, float& gain_dB)
{
    bEnabled = false;
    gain_dB = 0.0f;
    return S_OK;
}

STDMETHODIMP CAudioSwitcherFilter::SetReplayGain(bool /*bEnable*/, float /*gain_dB*/)
{
    return S_OK;
}

// IAMStreamSelect

STDMETHODIMP CAudioSwitcherFilter::Enable(long lIndex, DWORD dwFlags)
{
    HRESULT hr = __super::Enable(lIndex, dwFlags);
    if (S_OK == hr) {
        m_limiter.Reset();
    }
    return hr;
}
