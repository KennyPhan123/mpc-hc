/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2013 see Authors.txt
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

#pragma once

#include "CMPCThemePPageBase.h"
#include "FloatEdit.h"
#include "../filters/switcher/AudioSwitcher/AudioSwitcher.h"
#include "CMPCThemeButton.h"
#include "CMPCThemeSliderCtrl.h"
#include "CMPCThemeEdit.h"
#include "CMPCThemeSpinButtonCtrl.h"
#include "CMPCThemeComboBox.h"
#include "CMPCThemeRadioOrCheck.h"

// CPPageAudioSwitcher dialog (Kodi Audio Engine Settings)

class CPPageAudioSwitcher : public CMPCThemePPageBase
{
    DECLARE_DYNAMIC(CPPageAudioSwitcher)

private:
    BOOL m_fEnableAudioSwitcher;

    int m_AudioBoostPos;
    UINT m_nOrigAudioBoost;
    CMPCThemeSliderCtrl m_AudioBoostCtrl;

    int m_CenterMixPos;
    int m_iOrigCenterMixLevel;
    CMPCThemeSliderCtrl m_CenterMixCtrl;

    int m_iDownmixLayout;
    CMPCThemeComboBox m_downmixLayoutCtrl;

    BOOL m_bMaintainOriginalVolume;
    BOOL m_bUntouchedStereo;

    int m_tAudioTimeShift;
    CMPCThemeRadioOrCheck m_fAudioTimeShiftCtrl;
    CMPCThemeIntEdit m_tAudioTimeShiftCtrl;
    CMPCThemeSpinButtonCtrl m_tAudioTimeShiftSpin;
    BOOL m_fAudioTimeShift;

    CToolTipCtrl m_tooltip;

    void UpdateValueLabels();

public:
    CPPageAudioSwitcher(IFilterGraph* pFG);
    virtual ~CPPageAudioSwitcher();

    enum { IDD = IDD_PPAGEAUDIOSWITCHER };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual BOOL OnApply();

    DECLARE_MESSAGE_MAP()

    afx_msg void OnUpdateAudioSwitcher(CCmdUI* pCmdUI);
    afx_msg void OnUpdateDownmix(CCmdUI* pCmdUI);
    afx_msg void OnUpdateTimeShift(CCmdUI* pCmdUI);
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg BOOL OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
    virtual void OnCancel();
};
