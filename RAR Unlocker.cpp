#include "stdafx.h"
#include "resource.h"
#include "MainDlg.h"
#include "RarFile.h"

CAppModule _Module;

namespace
{
	enum class Action {
		DEFAULT,
		HELP,
		UNLOCK,
		LOCK,
	};

	int Default(HINSTANCE hInstance, const WCHAR* archive);
	int Help(HINSTANCE hInstance, const WCHAR* archive);
	int SetLock(HINSTANCE hInstance, const WCHAR* archive, bool lock);
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR /*lpstrCmdLine*/, int /*nCmdShow*/)
{
	HRESULT hRes = ::CoInitialize(NULL);
// If you are running on NT 4.0 or higher you can use the following call instead to 
// make the EXE free threaded. This means that calls come in on a random RPC thread.
//	HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
	ATLASSERT(SUCCEEDED(hRes));

	// this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
	::DefWindowProc(NULL, 0, 0, 0L);

	//AtlInitCommonControls(ICC_BAR_CLASSES);	// add flags to support other controls

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	Action action = Action::DEFAULT;
	const WCHAR* archive = nullptr;
	for(int i = 1; i < __argc; i++) {
		if(_wcsicmp(__wargv[i], L"--help") == 0 ||
			_wcsicmp(__wargv[i], L"-h") == 0) {
			action = Action::HELP;
		} else if(_wcsicmp(__wargv[i], L"--unlock") == 0 ||
			_wcsicmp(__wargv[i], L"-u") == 0) {
			action = Action::UNLOCK;
		} else if(_wcsicmp(__wargv[i], L"--lock") == 0 ||
			_wcsicmp(__wargv[i], L"-l") == 0) {
			action = Action::LOCK;
		} else if(__wargv[i][0] != '-') {
			archive = __wargv[i];
		} else {
			CString str;
			str.Format(L"Unknown command line option, skipping:\n%s\n\n"
				L"For a list of available command line parameters, run:\n"
				L"rar_unlocker.exe --help", __wargv[i]);
			::MessageBox(NULL, str, L"Warning", MB_ICONWARNING);
		}
	}

	int nRet = 0;
	switch(action) {
	case Action::DEFAULT:
		nRet = Default(hInstance, archive);
		break;

	case Action::HELP:
		nRet = Help(hInstance, archive);
		break;

	case Action::LOCK:
		nRet = SetLock(hInstance, archive, true);
		break;

	case Action::UNLOCK:
		nRet = SetLock(hInstance, archive, false);
		break;
	}

	_Module.Term();
	::CoUninitialize();

	return nRet;
}

namespace
{
	int Default(HINSTANCE hInstance, const WCHAR* archive)
	{
		CMainDlg dlgMain;
		LPARAM param = reinterpret_cast<LPARAM>(archive);
		return static_cast<int>(dlgMain.DoModal(::GetActiveWindow(), param));
	}

	int Help(HINSTANCE hInstance, const WCHAR* archive)
	{
		const WCHAR* usageText =
			L"Usage:\nrar_unlocker.exe archive.rar [-unlock | -lock]";

		::MessageBox(NULL, usageText, L"Usage", MB_ICONINFORMATION);

		return 0;
	}

	int SetLock(HINSTANCE hInstance, const WCHAR* archive, bool lock)
	{
		RarFile file;
		RarFile::error err = file.Open(archive, true);
		switch(err) {
		case RarFile::error::success:
			// Very good, continue.
			break;

		case RarFile::error::open_failed:
			::MessageBox(NULL, L"Could not open file for writing", L"Error", MB_ICONHAND);
			return 1;

		case RarFile::error::invalid_file:
			::MessageBox(NULL, L"The file is not a valid RAR archive", L"Error", MB_ICONHAND);
			return 1;

		default:
			::MessageBox(NULL, L"An unknown error occurred", L"Error", MB_ICONHAND);
			return 1;
		}

		DWORD flags;
		err = file.GetFlags(flags);
		switch(err) {
		case RarFile::error::success:
			// OK.
			break;

		case RarFile::error::encrypted_archive:
			::MessageBox(NULL, L"The file has encrypted headers, cannot modify", L"Error", MB_ICONHAND);
			return 1;

		case RarFile::error::invalid_file:
			::MessageBox(NULL, L"The file is not a valid RAR archive", L"Error", MB_ICONHAND);
			return 1;

		default:
			::MessageBox(NULL, L"An unknown error occurred while getting flags", L"Error", MB_ICONHAND);
			return 1;
		}

		if(lock) {
			err = file.SetLocked(true);
		} else {
			err = file.SetLocked(false);
		}

		switch(err) {
		case RarFile::error::success:
			// OK.
			break;

		case RarFile::error::encrypted_archive:
			::MessageBox(NULL, L"The file has encrypted headers, cannot modify", L"Error", MB_ICONHAND);
			return 1;

		case RarFile::error::invalid_file:
			::MessageBox(NULL, L"The file is not a valid RAR archive", L"Error", MB_ICONHAND);
			return 1;

		default:
			::MessageBox(NULL, L"An unknown error occurred while modifying file", L"Error", MB_ICONHAND);
			return 1;
		}

		return 0;
	}
}
