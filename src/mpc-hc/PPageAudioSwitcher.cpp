/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2016 see Authors.txt
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
#include <cmath>
#include "mplayerc.h"
#include "MainFrm.h"
#include "PPageAudioSwitcher.h"
#include "CMPCTheme.h"

// CPPageAudioSwitcher dialog (Kodi Audio Engine Settings)

IMPLEMENT_DYNAMIC(CPPageAudioSwitcher, CMPCThemePPageBase)
CPPageAudioSwitcher::CPPageAudioSwitcher(IFilterGraph* pFG)
    : CMPCThemePPageBase(CPPageAudioSwitcher::IDD, CPPageAudioSwitcher::IDD)
    , m_fEnableAudioSwitcher(TRUE)
    , m_AudioBoostPos(0)
    , m_nOrigAudioBoost(0)
    , m_CenterMixPos(0)
    , m_iOrigCenterMixLevel(0)
    , m_iDownmixLayout(0)
    , m_bMaintainOriginalVolume(TRUE)
    , m_bUntouchedStereo(TRUE)
    , m_tAudioTimeShift(0)
    , m_fAudioTimeShift(FALSE)
{
    UNREFERENCED_PARAMETER(pFG);
}

CPPageAudioSwitcher::~CPPageAudioSwitcher()
{
}

void CPPageAudioSwitcher::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_CHECK2, m_fEnableAudioSwitcher);
    DDX_Slider(pDX, IDC_SLIDER1, m_AudioBoostPos);
    DDX_Control(pDX, IDC_SLIDER1, m_AudioBoostCtrl);
    DDX_Slider(pDX, IDC_SLIDER2, m_CenterMixPos);
    DDX_Control(pDX, IDC_SLIDER2, m_CenterMixCtrl);
    DDX_Control(pDX, IDC_COMBO1, m_downmixLayoutCtrl);
    DDX_CBIndex(pDX, IDC_COMBO1, m_iDownmixLayout);
    DDX_Check(pDX, IDC_CHECK5, m_bMaintainOriginalVolume);
    DDX_Check(pDX, IDC_CHECK6, m_bUntouchedStereo);
    DDX_Control(pDX, IDC_EDIT2, m_tAudioTimeShiftCtrl);
    DDX_Control(pDX, IDC_SPIN2, m_tAudioTimeShiftSpin);
    DDX_Text(pDX, IDC_EDIT2, m_tAudioTimeShift);
    DDX_Check(pDX, IDC_CHECK4, m_fAudioTimeShift);
    DDX_Control(pDX, IDC_CHECK4, m_fAudioTimeShiftCtrl);
}

BEGIN_MESSAGE_MAP(CPPageAudioSwitcher, CMPCThemePPageBase)
    ON_UPDATE_COMMAND_UI(IDC_STATIC6, OnUpdateAudioSwitcher)
    ON_UPDATE_COMMAND_UI(IDC_SLIDER1, OnUpdateAudioSwitcher)
    ON_UPDATE_COMMAND_UI(IDC_STATIC1, OnUpdateAudioSwitcher)
    ON_UPDATE_COMMAND_UI(IDC_STATIC2, OnUpdateDownmix)
    ON_UPDATE_COMMAND_UI(IDC_SLIDER2, OnUpdateDownmix)
    ON_UPDATE_COMMAND_UI(IDC_STATIC3, OnUpdateDownmix)
    ON_UPDATE_COMMAND_UI(IDC_STATIC7, OnUpdateAudioSwitcher)
    ON_UPDATE_COMMAND_UI(IDC_COMBO1, OnUpdateAudioSwitcher)
    ON_UPDATE_COMMAND_UI(IDC_CHECK5, OnUpdateDownmix)
    ON_UPDATE_COMMAND_UI(IDC_CHECK6, OnUpdateDownmix)
    ON_UPDATE_COMMAND_UI(IDC_CHECK4, OnUpdateAudioSwitcher)
    ON_UPDATE_COMMAND_UI(IDC_EDIT2, OnUpdateTimeShift)
    ON_UPDATE_COMMAND_UI(IDC_SPIN2, OnUpdateTimeShift)
    ON_WM_HSCROLL()
    ON_NOTIFY_EX(TTN_NEEDTEXT, 0, OnToolTipNotify)
END_MESSAGE_MAP()

void CPPageAudioSwitcher::UpdateValueLabels()
{
    CString strBoost;
    strBoost.Format(_T("+%d dB (%.0f%%)"), m_AudioBoostPos, 100.0 * std::pow(10.0, m_AudioBoostPos / 20.0));
    SetDlgItemText(IDC_STATIC1, strBoost);

    CString strCenter;
    const double clev = 0.70710678118654752440 * std::pow(10.0, m_CenterMixPos / 20.0);
    if (m_CenterMixPos == 0) {
        strCenter.Format(_T("0 dB (%.2fx)"), clev);
    } else {
        strCenter.Format(_T("%+d dB (%.2fx)"), m_CenterMixPos, clev);
    }
    SetDlgItemText(IDC_STATIC3, strCenter);
}

BOOL CPPageAudioSwitcher::OnInitDialog()
{
    __super::OnInitDialog();

    const CAppSettings& s = AfxGetAppSettings();

    m_fEnableAudioSwitcher = s.fEnableAudioSwitcher;

    m_AudioBoostCtrl.SetRange(0, 60, TRUE);
    m_AudioBoostCtrl.SetPageSize(6);
    m_AudioBoostPos = std::clamp((int)s.nAudioBoost, 0, 60);
    m_nOrigAudioBoost = s.nAudioBoost;

    m_CenterMixCtrl.SetRange(-10, 30, TRUE);
    m_CenterMixCtrl.SetPageSize(2);
    m_CenterMixPos = std::clamp(s.iCenterMixLevel, -10, 30);
    m_iOrigCenterMixLevel = s.iCenterMixLevel;

    m_downmixLayoutCtrl.AddString(_T("Stereo (2.0) [Kodi Default]"));
    m_downmixLayoutCtrl.AddString(_T("5.1 Surround"));
    m_downmixLayoutCtrl.AddString(_T("7.1 Surround"));
    m_downmixLayoutCtrl.AddString(_T("Off (Original channels)"));
    CorrectComboListWidth(m_downmixLayoutCtrl);
    m_iDownmixLayout = std::clamp(s.iKodiDownmixLayout, 0, 3);

    m_bMaintainOriginalVolume = s.bKodiMaintainOriginalVolume;
    m_bUntouchedStereo = s.bKodiUntouchedStereo;

    m_fAudioTimeShift = s.fAudioTimeShift;
    m_tAudioTimeShift = s.iAudioTimeShift;
    m_tAudioTimeShiftSpin.SetRange32(-1000 * 60 * 60 * 24, 1000 * 60 * 60 * 24);

    EnableThemedDialogTooltips(this);
    m_tooltip.Create(this);
    m_tooltip.Activate(TRUE);

    UpdateData(FALSE);
    UpdateValueLabels();

    return TRUE;
}

BOOL CPPageAudioSwitcher::OnApply()
{
    UpdateData();

    CAppSettings& s = AfxGetAppSettings();

    s.fEnableAudioSwitcher = !!m_fEnableAudioSwitcher;
    s.fAudioNormalize = false;
    s.fAudioNormalizeRecover = false;
    s.fCustomChannelMapping = false;
    s.iReplayGainMode = 0;

    s.nAudioBoost = std::clamp(m_AudioBoostPos, 0, 60);
    m_nOrigAudioBoost = s.nAudioBoost;

    s.iCenterMixLevel = std::clamp(m_CenterMixPos, -10, 30);
    m_iOrigCenterMixLevel = s.iCenterMixLevel;

    s.iKodiDownmixLayout = std::clamp(m_iDownmixLayout, 0, 3);
    s.bKodiMaintainOriginalVolume = !!m_bMaintainOriginalVolume;
    s.bKodiUntouchedStereo = !!m_bUntouchedStereo;

    s.fAudioTimeShift = !!m_fAudioTimeShift;
    s.iAudioTimeShift = m_tAudioTimeShift;

    UpdateValueLabels();

    if (CMainFrame* pMainFrame = AfxGetMainFrame()) {
        pMainFrame->UpdateControlState(CMainFrame::UPDATE_AUDIO_SWITCHER);
    }

    return __super::OnApply();
}

void CPPageAudioSwitcher::OnUpdateAudioSwitcher(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(IsDlgButtonChecked(IDC_CHECK2));
}

void CPPageAudioSwitcher::OnUpdateDownmix(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(IsDlgButtonChecked(IDC_CHECK2) && m_downmixLayoutCtrl.GetCurSel() != 3);
}

void CPPageAudioSwitcher::OnUpdateTimeShift(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(IsDlgButtonChecked(IDC_CHECK2) && IsDlgButtonChecked(IDC_CHECK4));
}

void CPPageAudioSwitcher::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    if (*pScrollBar == m_AudioBoostCtrl) {
        UpdateData();
        UpdateValueLabels();
        if (CMainFrame* pMainFrame = AfxGetMainFrame()) {
            pMainFrame->SetVolumeBoost(m_AudioBoostPos);
        }
    } else if (*pScrollBar == m_CenterMixCtrl) {
        UpdateData();
        UpdateValueLabels();
        if (CMainFrame* pMainFrame = AfxGetMainFrame()) {
            pMainFrame->SetCenterMixLevel(m_CenterMixPos);
        }
    }

    RedrawDialogTooltipIfVisible();
    SetModified();

    __super::OnHScroll(nSBCode, nPos, pScrollBar);
}

BOOL CPPageAudioSwitcher::OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
    UNREFERENCED_PARAMETER(id);
    UNREFERENCED_PARAMETER(pResult);

    TOOLTIPTEXT* pTTT = (TOOLTIPTEXT*)pNMHDR;

    UINT_PTR nID = pNMHDR->idFrom;
    if (pTTT->uFlags & TTF_IDISHWND) {
        nID = ::GetDlgCtrlID((HWND)nID);
    }

    bool bRet = false;
    static CString strTipText;

    switch (nID) {
        case IDC_SLIDER1: {
            int pos = m_AudioBoostCtrl.GetPos();
            strTipText.Format(_T("+%d dB (%.0f%%)"), pos, 100.0 * std::pow(10.0, pos / 20.0));
            bRet = true;
            break;
        }
        case IDC_SLIDER2: {
            int pos = m_CenterMixCtrl.GetPos();
            const double clev = 0.70710678118654752440 * std::pow(10.0, pos / 20.0);
            strTipText.Format(_T("Center Mix: %+d dB (%.2fx)"), pos, clev);
            bRet = true;
            break;
        }
        case IDC_EDIT2:
        case IDC_SPIN2:
            strTipText.LoadString(IDS_TIME_SHIFT_TOOLTIP);
            bRet = true;
            break;
    }

    if (bRet) {
        pTTT->lpszText = (LPWSTR)(LPCWSTR)strTipText;
        PlaceThemedDialogTooltip(nID);
    }

    return bRet;
}

void CPPageAudioSwitcher::OnCancel()
{
    if (CMainFrame* pMainFrame = AfxGetMainFrame()) {
        if ((UINT)m_AudioBoostPos != m_nOrigAudioBoost) {
            pMainFrame->SetVolumeBoost(m_nOrigAudioBoost);
        }
        if (m_CenterMixPos != m_iOrigCenterMixLevel) {
            pMainFrame->SetCenterMixLevel(m_iOrigCenterMixLevel);
        }
    }

    __super::OnCancel();
}
