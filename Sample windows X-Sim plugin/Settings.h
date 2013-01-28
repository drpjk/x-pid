#pragma once
#include "stdafx.h"
#include "resource.h"		// Hauptsymbole
#include "afxwin.h"
#include "afxcmn.h"


// CSettings-Dialogfeld

class CSettings : public CDialog
{
	DECLARE_DYNAMIC(CSettings)

public:
	CSettings(CWnd* pParent = NULL);   // Standardkonstruktor
	virtual ~CSettings();
	CBrush m_DialogBrush;
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);

// Dialogfelddaten
	enum { IDD = IDD_SETTINGS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV-Unterstützung
	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnBnClickedRenameport();
	afx_msg void OnCbnSelendokXpidCombo();
	afx_msg void OnClose();
	afx_msg void OnBnClickedTimeoutCheck();
	afx_msg void OnEnChangeTimeoutEdit();
	afx_msg void OnBnClickedSetButtonMinimum1();
	afx_msg void OnBnClickedSetButtonMaximum1();
	afx_msg void OnBnClickedSetButtonMinimum2();
	afx_msg void OnBnClickedSetButtonMaximum2();
	afx_msg void OnBnClickedSetnameButton();
	afx_msg void OnBnClickedCloseButton();
	void SaveSingleXpidRegistry(int comportnumer);
	void PrintSettings();
	CStatic m_comportname;
	CStatic m_devicepath;
	CStatic m_comportdescription;
	CComboBox m_xpidcombo;
	CButton m_renamecomport;
	CButton m_setnamebutton;
	bool delayedupdate;
	unsigned int oldpidcounter;
	int olddiff;
	CStatic m_statuswritten;
	CStatic m_updatefrequency;
	CString textout;
	CStatic m_positiontext;
	CComboBox m_renametext;
	CButton m_enable_timeout;
	CEdit m_timeout_delay;
	CStatic m_pid_counter;
	CStatic m_feedback1;
	CStatic m_feedback2;
	CSpinButtonCtrl m_deadzone1_spin;
	CSpinButtonCtrl m_deadzone2_spin;
	CStatic m_deadzone1_text;
	CStatic m_deadzone2_text;
	CStatic m_minimum1_text;
	CStatic m_maximum1_text;
	CStatic m_maximum2_text;
	CStatic m_minimum2_text;
	CStatic m_feedback_scaled_1;
	CStatic m_feedback_scaled_2;
	afx_msg void OnDeltaposDeadzone1Spin(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnDeltaposDeadzone2Spin(NMHDR *pNMHDR, LRESULT *pResult);
	CSpinButtonCtrl m_p1_spin;
	CSpinButtonCtrl m_i1_spin;
	CSpinButtonCtrl m_d1_spin;
	CSpinButtonCtrl m_p2_spin;
	CSpinButtonCtrl m_i2_spin;
	CSpinButtonCtrl m_d2_spin;
	afx_msg void OnDeltaposP1Spin(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnDeltaposI1Spin(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnDeltaposD1Spin(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnDeltaposP2Spin(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnDeltaposI2Spin(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnDeltaposD2Spin(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedReset1();
	afx_msg void OnBnClickedReset2();
	CStatic m_debugbyte;
	CStatic m_debugdouble;
	CStatic m_debuginteger;
	afx_msg void OnBnClickedEnableDebug();
	CStatic m_p1_text;
	CStatic m_d1_text;
	CStatic m_i1_text;
	CStatic m_p2_text;
	CStatic m_d2_text;
	CStatic m_i2_text;
	afx_msg void OnBnClickedGraphCheck();
};
