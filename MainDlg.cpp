#include "stdafx.h"
#include "resource.h"

#include "MainDlg.h"
#include "RarFile.h"

BOOL CMainDlg::OnInitDialog(CWindow wndFocus, LPARAM lInitParam)
{
	// Center the dialog on the screen.
	CenterWindow();

	// Set icons.
	HICON hIcon = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON));
	SetIcon(hIcon, TRUE);
	HICON hIconSmall = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON));
	SetIcon(hIconSmall, FALSE);

	const WCHAR* archivePath = reinterpret_cast<const WCHAR*>(lInitParam);
	if(archivePath) {
		const WCHAR* errorMsg;
		if(!LoadArchive(archivePath, errorMsg)) {
			MessageBox(errorMsg, L"Error", MB_ICONERROR);
		}
	}

	return TRUE;
}

void CMainDlg::OnDropFiles(HDROP hDropInfo)
{
	if(DragQueryFile(hDropInfo, 0xFFFFFFFF, NULL, 0) == 1) {
		WCHAR fileName[MAX_PATH];
		DragQueryFile(hDropInfo, 0, fileName, MAX_PATH);

		const WCHAR* errorMsg;
		if(!LoadArchive(fileName, errorMsg)) {
			MessageBox(errorMsg, L"Error", MB_ICONERROR);
		}
	} else {
		MessageBox(L"Please drop one file at a time", L"Unsupported", MB_ICONINFORMATION);
	}

	DragFinish(hDropInfo);
}

void CMainDlg::OnBrowse(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	const WCHAR* filter =
		L"Rar archives (*.rar)\0*.rar\0"
		L"SFX archives (*.exe)\0*.exe\0"
		L"All files (*.*)\0*.*\0";
	CFileDialog fileDlg(TRUE, L"rar", NULL,
		OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, filter);

	fileDlg.m_ofn.lpstrTitle = L"Choose a RAR archive";

	// Fixes a visual styling issue, but triggers an assert.
	fileDlg.m_ofn.Flags &= ~OFN_ENABLEHOOK;
	fileDlg.m_ofn.lpfnHook = NULL;

	if(fileDlg.DoModal() == IDCANCEL) {
		return;
	}

	const WCHAR* errorMsg;
	if(!LoadArchive(fileDlg.m_szFileName, errorMsg)) {
		MessageBox(errorMsg, L"Error", MB_ICONERROR);
	}
}

void CMainDlg::OnAppAbout(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	CSimpleDialog<IDD_ABOUTBOX> dlg;
	dlg.DoModal();
}

void CMainDlg::OnOK(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	assert(m_canModifyArchive);

	CString archivePath;
	GetDlgItemText(IDC_PATH, archivePath);

	const WCHAR* errorMsg;
	if(!ModifyArchive(archivePath, errorMsg)) {
		MessageBox(errorMsg, L"Error", MB_ICONERROR);
	}
}

void CMainDlg::OnCancel(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	EndDialog(nID);
}

bool CMainDlg::LoadArchive(const WCHAR* archivePath, const WCHAR*& errorMsg)
{
	RarFile file;
	RarFile::error err = file.Open(archivePath);
	switch(err) {
	case RarFile::error::success:
		// Very good, continue.
		break;

	case RarFile::error::open_failed:
		errorMsg = L"Could not open file";
		return false;

	case RarFile::error::invalid_file:
		errorMsg = L"The file is not a valid RAR archive";
		return false;

	default:
		errorMsg = L"An unknown error occurred";
		return false;
	}

	bool encrypted = false;

	DWORD flags;
	err = file.GetFlags(flags);
	switch(err) {
	case RarFile::error::success:
		// OK.
		break;

	case RarFile::error::encrypted_archive:
		encrypted = true;
		break;

	case RarFile::error::invalid_file:
		errorMsg = L"The file is not a valid RAR archive";
		return false;

	default:
		errorMsg = L"An unknown error occurred while getting flags";
		return false;
	}

	SetInfoToGui(archivePath, file.GetRarVersion(), file.IsSFX(), encrypted, flags);

	if(encrypted) {
		m_canModifyArchive = false;
	} else {
		m_canModifyArchive = true;

		if(flags & RarFile::locked) {
			m_archiveLocked = true;
		} else {
			m_archiveLocked = false;
		}
	}

	return true;
}

bool CMainDlg::ModifyArchive(const WCHAR* archivePath, const WCHAR*& errorMsg)
{
	RarFile file;
	RarFile::error err = file.Open(archivePath, true);
	switch(err) {
	case RarFile::error::success:
		// Very good, continue.
		break;

	case RarFile::error::open_failed:
		errorMsg = L"Could not open file for writing";
		return false;

	case RarFile::error::invalid_file:
		errorMsg = L"The file is not a valid RAR archive, perhaps the file was modified";
		return false;

	default:
		errorMsg = L"An unknown error occurred";
		return false;
	}

	DWORD flags;
	err = file.GetFlags(flags);
	switch(err) {
	case RarFile::error::success:
		// OK.
		break;

	case RarFile::error::encrypted_archive:
		errorMsg = L"The file has encrypted headers, cannot modify";
		return false;

	case RarFile::error::invalid_file:
		errorMsg = L"The file is not a valid RAR archive, perhaps the file was modified";
		return false;

	default:
		errorMsg = L"An unknown error occurred while getting flags";
		return false;
	}

	if(m_archiveLocked) {
		err = file.SetLocked(false);
	} else {
		err = file.SetLocked(true);
	}

	switch(err) {
	case RarFile::error::success:
		// OK.
		break;

	case RarFile::error::encrypted_archive:
		errorMsg = L"The file has encrypted headers, cannot modify";
		return false;

	case RarFile::error::invalid_file:
		errorMsg = L"The file is not a valid RAR archive, perhaps the file was modified";
		return false;

	default:
		errorMsg = L"An unknown error occurred while modifying file";
		return false;
	}

	m_archiveLocked = !m_archiveLocked;
	if(m_archiveLocked) {
		flags |= RarFile::locked;
	} else {
		flags &= ~RarFile::locked;
	}

	SetInfoToGui(nullptr, file.GetRarVersion(), file.IsSFX(), false, flags);

	return true;
}

void CMainDlg::SetInfoToGui(const WCHAR* archivePath, int rarVersion,
	bool sfx, bool encrypted, DWORD flags)
{
	if(archivePath) {
		CEdit(GetDlgItem(IDC_PATH)).SetWindowText(archivePath);
	}

	CString archiveVersion;
	archiveVersion.Format(L"RAR archive version: %d", rarVersion);
	if(sfx) {
		archiveVersion += L", SFX archive";
	}

	CStatic(GetDlgItem(IDC_VERSION)).SetWindowText(archiveVersion);

	if(encrypted) {
		CButton flagCheckbox = GetDlgItem(IDC_FLAG1);
		flagCheckbox.EnableWindow();
		flagCheckbox.SetCheck(BST_CHECKED);

		for(int id = IDC_FLAG2; id <= IDC_FLAG6; ++id) {
			flagCheckbox = GetDlgItem(id);
			flagCheckbox.EnableWindow(FALSE);
			flagCheckbox.SetCheck(BST_INDETERMINATE);
		}

		CButton okButton = GetDlgItem(IDOK);
		okButton.EnableWindow(FALSE);
		okButton.SetWindowText(L"Un&lock");
	} else {
		DWORD flagsToCheck[] = {
			RarFile::encrypted_headers,
			RarFile::locked,
			RarFile::solid,
			RarFile::multivolume,
			RarFile::first_volume,
			RarFile::recovery_record,
		};

		for(int id = IDC_FLAG1; id <= IDC_FLAG6; ++id) {
			CButton flagCheckbox = GetDlgItem(id);
			flagCheckbox.EnableWindow();

			if(flags & flagsToCheck[id - IDC_FLAG1]) {
				flagCheckbox.SetCheck(BST_CHECKED);
			} else {
				flagCheckbox.SetCheck(BST_UNCHECKED);
			}
		}

		CButton okButton = GetDlgItem(IDOK);
		okButton.EnableWindow();

		if(flags & RarFile::locked) {
			okButton.SetWindowText(L"Un&lock");
		} else {
			okButton.SetWindowText(L"&Lock");
		}
	}
}
