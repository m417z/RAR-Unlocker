#include "stdafx.h"
#include "RarFile.h"
#include "crc32.h"

RarFile::error RarFile::Open(const TCHAR* fileName,
	bool writable /*= false*/, size_t maxSearchSize /*= m_defaultMaxSearchSize*/)
{
	assert(!m_open);

	CAtlFile fileHandle;
	HRESULT hr = fileHandle.Create(fileName,
		writable ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ,
		FILE_SHARE_READ,
		OPEN_EXISTING);

	if(FAILED(hr)) {
		return error::open_failed;
	}

	ULONGLONG len;
	fileHandle.GetSize(len);

	hr = m_fileMapping.MapFile(fileHandle,
		std::min(static_cast<size_t>(len), maxSearchSize),
		0,
		writable ? PAGE_READWRITE : PAGE_READONLY,
		writable ? FILE_MAP_WRITE : FILE_MAP_READ);

	if(FAILED(hr)) {
		return error::open_failed;
	}

	if(!FindSignature()) {
		m_fileMapping.Unmap();
		return error::invalid_file;
	}

	m_open = true;
	m_writable = writable;

	return error::success;
}

int RarFile::GetRarVersion()
{
	assert(m_open);
	return m_rarVersion;
}

bool RarFile::IsSFX()
{
	assert(m_open);
	return m_fileRarOffset != 0;
}

RarFile::error RarFile::GetFlags(DWORD& fileFlags)
{
	assert(m_open);

	switch(m_rarVersion) {
	case 4:
		return GetFlags4(fileFlags);

	case 5:
		return GetFlags5(fileFlags);

	default:
		assert(0);
		return error::invalid_file;
	}
}

RarFile::error RarFile::SetLocked(bool locked)
{
	assert(m_open && m_writable);

	switch(m_rarVersion) {
	case 4:
		return SetLocked4(locked);

	case 5:
		return SetLocked5(locked);

	default:
		assert(0);
		return error::invalid_file;
	}
}

void RarFile::Close()
{
	if(m_open) {
		m_fileMapping.Unmap();
		m_open = false;
	}
}

//////////////////////////////////////////////////////////////////////////
// Private functions.

bool RarFile::FindSignature()
{
	const BYTE* fileBegin = m_fileMapping;
	const BYTE* fileEnd = fileBegin + m_fileMapping.GetMappingSize();
	const BYTE signature[] = { 0x52, 0x61, 0x72, 0x21, 0x1A, 0x07 };

	size_t offset = 0;

	for(;;) {
		auto it = std::search(fileBegin + offset, fileEnd,
			std::begin(signature), std::end(signature));
		if(it == fileEnd) {
			return false;
		}

		offset = it - fileBegin;

		it += _countof(signature);
		if(it >= fileEnd) {
			return false;
		}

		if(*it == 0x00) {
			it++;
			if(fileEnd - it < 0x14) {
				return false; // not enough bytes for a valid archive
			}

			m_fileRarOffset = offset;
			m_rarVersion = 4;
			return true;
		} else if(*it == 0x01) {
			it++;
			if(it < fileEnd && *it == 0x00) {
				it++;
				if(fileEnd - it < 0x08) {
					return false; // not enough bytes for a valid archive
				}

				m_fileRarOffset = offset;
				m_rarVersion = 5;
				return true;
			}
		}

		offset = it - fileBegin;
	}
}

RarFile::error RarFile::GetFlags4(DWORD& fileFlags)
{
	const BYTE* fileBegin = m_fileMapping;
	const BYTE* fileEnd = fileBegin + m_fileMapping.GetMappingSize();
	const BYTE* archive = fileBegin + m_fileRarOffset;

	assert(archive + 0x0A + sizeof(WORD) <= fileEnd);
	WORD flags = *reinterpret_cast<const WORD*>(archive + 0x0A);

	// 0x0001  - Volume attribute (archive volume)
	// 0x0002  - Archive comment present
	//           RAR 3.x uses the separate comment block
	//           and does not set this flag.
	//
	// 0x0004  - Archive lock attribute
	// 0x0008  - Solid attribute (solid archive)
	// 0x0010  - New volume naming scheme ('volname.partN.rar')
	// 0x0020  - Authenticity information present
	//           RAR 3.x does not set this flag.
	//
	// 0x0040  - Recovery record present
	// 0x0080  - Block headers are encrypted
	// 0x0100  - First volume (set only by RAR 3.0 and later)

	fileFlags = 0;

	if(flags & 0x0001) {
		fileFlags |= multivolume;
	}

	if(flags & 0x0004) {
		fileFlags |= locked;
	}

	if(flags & 0x0008) {
		fileFlags |= solid;
	}

	if(flags & 0x0040) {
		fileFlags |= recovery_record;
	}

	if(flags & 0x0080) {
		fileFlags |= encrypted_headers;
	}

	if(flags & 0x0100) {
		fileFlags |= first_volume;
	}

	return error::success;
}

RarFile::error RarFile::GetFlags5(DWORD& fileFlags)
{
	const BYTE* fileBegin = m_fileMapping;
	const BYTE* fileEnd = fileBegin + m_fileMapping.GetMappingSize();
	const BYTE* archive = fileBegin + m_fileRarOffset;
	const BYTE* archivePtr = archive;

	// Skip signature.
	archivePtr += 0x08;

	// Skip header CRC32.
	archivePtr += sizeof(DWORD);

	// Skip header size.
	ULONGLONG value;
	size_t bytesRead;
	if(!GetVint(archivePtr, fileEnd, value, bytesRead)) {
		return error::invalid_file;
	}

	archivePtr += bytesRead;

	// Verify header type.
	if(!GetVint(archivePtr, fileEnd, value, bytesRead)) {
		return error::invalid_file;
	}

	if(value != 1) {
		if(value == 4) {
			return error::encrypted_archive;
		}

		return error::invalid_file;
	}

	archivePtr += bytesRead;

	// Skip header flags and the optional extra area size.
	if(!GetVint(archivePtr, fileEnd, value, bytesRead)) {
		return error::invalid_file;
	}

	archivePtr += bytesRead;

	if(value & 0x0001) {
		if(!GetVint(archivePtr, fileEnd, value, bytesRead)) {
			return error::invalid_file;
		}

		archivePtr += bytesRead;
	}

	// Get archive flags.
	if(!GetVint(archivePtr, fileEnd, value, bytesRead)) {
		return error::invalid_file;
	}

	archivePtr += bytesRead;

	WORD flags = static_cast<WORD>(value);

	// 0x0001 - Volume. Archive is a part of multivolume set.
	// 0x0002 - Volume number field is present. This flag is present in all volumes except first.
	// 0x0004 - Solid archive.
	// 0x0008 - Recovery record is present.
	// 0x0010 - Locked archive.

	fileFlags = 0;

	if(flags & 0x0001) {
		fileFlags |= multivolume;

		if(!(flags & 0x0002)) {
			fileFlags |= first_volume;
		}
	}

	if(flags & 0x0004) {
		fileFlags |= solid;
	}

	if(flags & 0x0008) {
		fileFlags |= recovery_record;
	}

	if(flags & 0x0010) {
		fileFlags |= locked;
	}

	return error::success;
}

RarFile::error RarFile::SetLocked4(bool locked)
{
	BYTE* fileBegin = m_fileMapping;
	BYTE* fileEnd = fileBegin + m_fileMapping.GetMappingSize();
	BYTE* archive = fileBegin + m_fileRarOffset;

	assert(archive + 0x0A + sizeof(WORD) <= fileEnd);
	WORD* flags = reinterpret_cast<WORD*>(archive + 0x0A);

	bool oldLocked = (*flags & 0x0004) != 0;
	if(oldLocked == locked) {
		return error::success;
	}

	assert(archive + 0x0C + sizeof(WORD) <= fileEnd);
	WORD headerSize = *reinterpret_cast<WORD*>(archive + 0x0C);

	const BYTE* hashCalcStart = archive + 0x09;
	size_t hashCalcSize = headerSize - 2;
	if(hashCalcStart + hashCalcSize > fileEnd) {
		return error::invalid_file;
	}

	if(locked) {
		*flags |= 0x0004;
	} else {
		*flags &= ~0x0004;
	}

	DWORD hashValue = crc32(hashCalcStart, hashCalcSize);

	WORD* archiveCrc32 = reinterpret_cast<WORD*>(archive + 0x07);
	*archiveCrc32 = static_cast<WORD>(hashValue);

	return error::success;
}

RarFile::error RarFile::SetLocked5(bool locked)
{
	BYTE* fileBegin = m_fileMapping;
	BYTE* fileEnd = fileBegin + m_fileMapping.GetMappingSize();
	BYTE* archive = fileBegin + m_fileRarOffset;
	BYTE* archivePtr = archive;

	// Skip signature.
	archivePtr += 0x08;

	// Skip header CRC32.
	archivePtr += sizeof(DWORD);

	BYTE* hashCalcStart = archivePtr;

	// Get header size.
	ULONGLONG value;
	size_t bytesRead;
	if(!GetVint(archivePtr, fileEnd, value, bytesRead)) {
		return error::invalid_file;
	}

	archivePtr += bytesRead;

	size_t headerSize = static_cast<size_t>(value);
	if(headerSize != value) {
		return error::invalid_file; // header size too large
	}

	size_t hashCalcSize = headerSize + bytesRead;
	if(hashCalcStart + hashCalcSize > fileEnd) {
		return error::invalid_file;
	}

	// Verify header type.
	if(!GetVint(archivePtr, fileEnd, value, bytesRead)) {
		return error::invalid_file;
	}

	if(value != 1) {
		if(value == 4) {
			return error::encrypted_archive;
		}

		return error::invalid_file;
	}

	archivePtr += bytesRead;

	// Skip header flags and the optional extra area size.
	if(!GetVint(archivePtr, fileEnd, value, bytesRead)) {
		return error::invalid_file;
	}

	archivePtr += bytesRead;

	if(value & 0x0001) {
		if(!GetVint(archivePtr, fileEnd, value, bytesRead)) {
			return error::invalid_file;
		}

		archivePtr += bytesRead;
	}

	// Get archive flags.
	if(!GetVint(archivePtr, fileEnd, value, bytesRead)) {
		return error::invalid_file;
	}

	WORD flags = static_cast<WORD>(value);

	bool oldLocked = (flags & 0x0010) != 0;
	if(oldLocked == locked) {
		return error::success;
	}

	// Modify the 5-th bit.
	if(locked) {
		*archivePtr |= 0x10;
	} else {
		*archivePtr &= ~0x10;
	}

	DWORD hashValue = crc32(hashCalcStart, hashCalcSize);

	DWORD* archiveCrc32 = reinterpret_cast<DWORD*>(archive + 0x08);
	*archiveCrc32 = hashValue;

	return error::success;
}

bool RarFile::GetVint(const BYTE* dataBegin, const BYTE* dataEnd,
	ULONGLONG& value, size_t& bytesRead)
{
	ULONGLONG result = 0;
	size_t offset = 0;
	for(const BYTE* p = dataBegin; p < dataEnd; ++p, ++offset) {
		BYTE data = (*p) & 0x7F;
		bool last = ((*p) & 0x80) == 0;
		size_t shift_bits = offset * 7;

		ULONGLONG shifted = (ULONGLONG)data << shift_bits;
		if((shifted >> shift_bits) != data) {
			return false; // overflow
		}

		result |= shifted;

		if(last) {
			value = result;
			bytesRead = offset + 1;
			return true;
		}
	}

	return false;
}
