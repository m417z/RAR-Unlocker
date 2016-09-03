#pragma once

class CMainDlg : public CDialogImpl<CMainDlg>
{
public:
	enum { IDD = IDD_MAINDLG };

	BEGIN_MSG_MAP_EX(CMainDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_DROPFILES(OnDropFiles)
		COMMAND_HANDLER_EX(IDC_BROWSE, BN_CLICKED, OnBrowse)
		COMMAND_ID_HANDLER_EX(IDOK, OnOK)
		COMMAND_ID_HANDLER_EX(ID_APP_ABOUT, OnAppAbout)
		COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
	END_MSG_MAP()

	BOOL OnInitDialog(CWindow wndFocus, LPARAM lInitParam);
	void OnDropFiles(HDROP hDropInfo);
	void OnBrowse(UINT uNotifyCode, int nID, CWindow wndCtl);
	void OnOK(UINT uNotifyCode, int nID, CWindow wndCtl);
	void OnAppAbout(UINT uNotifyCode, int nID, CWindow wndCtl);
	void OnCancel(UINT uNotifyCode, int nID, CWindow wndCtl);

private:
	bool LoadArchive(const WCHAR* archivePath, const WCHAR*& errorMsg);
	bool ModifyArchive(const WCHAR* archivePath, const WCHAR*& errorMsg);
	void SetInfoToGui(const WCHAR* archivePath, int rarVersion,
		bool sfx, bool encrypted, DWORD flags);

	bool m_canModifyArchive = false;
	bool m_archiveLocked;
};
