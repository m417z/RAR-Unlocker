#pragma once

class RarFile {
public:
	enum class error {
		success,
		open_failed,
		invalid_file,
		encrypted_archive
	};

	enum flags {
		multivolume = 0x01,
		first_volume = 0x02,
		solid = 0x04,
		recovery_record = 0x08,
		locked = 0x10,
		encrypted_headers = 0x20
	};

	RarFile() = default;
	~RarFile() = default;

	RarFile(const RarFile&) = delete;
	RarFile& operator=(const RarFile&) = delete;

	error Open(const TCHAR* fileName,
		bool writable = false, size_t maxSearchSize = m_defaultMaxSearchSize);
	int GetRarVersion();
	bool IsSFX();
	error GetFlags(DWORD& fileFlags);
	error SetLocked(bool locked);
	void Close();

private:
	bool FindSignature();
	error GetFlags4(DWORD& fileFlags);
	error GetFlags5(DWORD& fileFlags);
	error SetLocked4(bool locked);
	error SetLocked5(bool locked);
	static bool GetVint(const BYTE* dataBegin, const BYTE* dataEnd,
		ULONGLONG& value, size_t& bytesRead);

	bool m_open = false;
	CAtlFileMapping<BYTE> m_fileMapping;
	bool m_writable;
	size_t m_fileRarOffset;
	int m_rarVersion;

	static const size_t m_defaultMaxSearchSize = 1024 * 1024 * 10;
};
