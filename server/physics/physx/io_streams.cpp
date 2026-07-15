/*
io_streams.cpp - i/o stream classes that used in PhysX for cooking
Copyright (C) 2012 Uncle Mike
Copyright (C) 2023 SNMetamorph
Copyright (C) 2026 rickomax

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "extdll.h"
#include "util.h"
#include "physic.h"

#ifdef USE_PHYSICS_ENGINE

#include "io_streams.h"
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

using namespace physx;

/*
=================
UserStream

opens a file stream: either precaches the whole file into memory for reading, or creates the file (and its directory path) for writing
=================
*/
UserStream::UserStream(const char *filePath, bool precacheToMemory)
{
	m_offset = 0;
	m_length = 0;
	m_buffer = NULL;
	m_outputFile = NULL;
	m_fileCached = precacheToMemory;

	if (m_fileCached)
	{
		int size = 0;
		m_buffer = LOAD_FILE(filePath, &size);
		m_length = size;
	}
	else
	{
		char szFilePath[MAX_PATH];
		char szFullPath[MAX_PATH];

		GET_GAME_DIR(szFilePath);
		Q_snprintf(szFullPath, sizeof(szFullPath), "%s/%s", szFilePath, filePath);
		CreatePath(szFullPath);

		m_outputFile = fopen(szFullPath, "wb");
		ASSERT(m_outputFile != NULL);
	}
}

/*
=================
~UserStream

frees the cached buffer or closes the output file, depending on mode
=================
*/
UserStream::~UserStream()
{
	if (m_fileCached)
	{
		if (m_buffer)
		{
			FREE_FILE(m_buffer);
		}
		m_offset = m_length = 0;
		m_buffer = NULL;
	}
	else
	{
		if (m_outputFile)
		{
			fclose(m_outputFile);
		}
		m_outputFile = NULL;
	}
}

/*
=================
CreatePath

creates every intermediate directory along the given file path
=================
*/
void UserStream::CreatePath(char *path)
{
	char *ofs, save;

	for (ofs = path + 1; *ofs; ofs++)
	{
		if (*ofs == '/' || *ofs == '\\')
		{
			save = *ofs;
			*ofs = 0;
#ifdef _WIN32
			_mkdir(path);
#else
			mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif
			*ofs = save;
		}
	}
}

/*
=================
read

reads bytes from the precached buffer, clamping to the remaining length and warning on overrun. Returns the number of bytes copied
=================
*/
uint32_t UserStream::read(void *outputBuf, uint32_t size)
{
	if (size == 0) {
		return 0;
	}

	if (!m_length || !m_buffer || !outputBuf)
	{
		return 0;
	}

	if (m_offset >= m_length) {
		ALERT(at_warning, "UserStream::read: precached file buffer overrun\n");
		return 0;
	}

	if (m_offset + size <= m_length)
	{
		memcpy(outputBuf, m_buffer + m_offset, size);
		m_offset += size;
		return size;
	}
	else
	{
		ALERT(at_warning, "UserStream::read: precached file buffer overrun\n");
		size_t reducedSize = m_length - m_offset;
		memcpy(outputBuf, m_buffer + m_offset, reducedSize);
		m_offset += reducedSize;
		return reducedSize;
	}
}

/*
=================
write

writes bytes to the output file. Returns the number of bytes written
=================
*/
uint32_t UserStream::write(const void *inputBuf, uint32_t size)
{
	ASSERT(m_outputFile != NULL);
	if (!m_outputFile)
	{
		return 0;
	}
	return fwrite(inputBuf, 1, size, m_outputFile);
}

/*
=================
MemoryWriteBuffer

constructs an empty in-memory write buffer
=================
*/
MemoryWriteBuffer::MemoryWriteBuffer()
{
	m_currentSize = 0;
}

/*
=================
~MemoryWriteBuffer

releases the buffer storage
=================
*/
MemoryWriteBuffer::~MemoryWriteBuffer()
{
	m_dataBuffer.clear();
}

/*
=================
write

appends bytes to the buffer, growing its capacity in chunks as needed. Returns the number of bytes written
=================
*/
uint32_t MemoryWriteBuffer::write(const void *inputBuf, uint32_t size)
{
	const size_t growthSize = 4096;
	size_t expectedSize = m_currentSize + size;
	if (expectedSize > m_dataBuffer.size()) {
		m_dataBuffer.resize(expectedSize + growthSize);
	}
	memcpy(m_dataBuffer.data() + m_currentSize, inputBuf, size);
	m_currentSize += size;
	return size;
}

/*
=================
getSize

returns the allocated buffer size
=================
*/
size_t MemoryWriteBuffer::getSize() const
{
	return m_dataBuffer.size();
}

/*
=================
getData

returns a pointer to the buffer's data
=================
*/
const uint8_t *MemoryWriteBuffer::getData() const
{
	return m_dataBuffer.data();
}

/*
=================
MemoryReadBuffer

wraps an existing data block for sequential reading
=================
*/
MemoryReadBuffer::MemoryReadBuffer(const uint8_t *data, size_t dataSize)
{
	m_dataSize = dataSize;
	m_dataOffset = 0;
	m_buffer = data;
}

/*
=================
~MemoryReadBuffer

destructor; does not own the wrapped data
=================
*/
MemoryReadBuffer::~MemoryReadBuffer()
{
}

/*
=================
read

reads bytes from the wrapped block, clamping to the remaining size and warning on overrun. Returns the number of bytes copied
=================
*/
uint32_t MemoryReadBuffer::read(void *outputBuf, uint32_t count)
{
	if (m_dataOffset >= m_dataSize) {
		ALERT(at_warning, "MemoryReadBuffer::read: input buffer is overrun\n");
		return 0;
	}

	size_t bytesToRead = count;
	if (m_dataOffset + count > m_dataSize) {
		ALERT(at_warning, "MemoryReadBuffer::read: input buffer is overrun\n");
		bytesToRead = m_dataSize - m_dataOffset;
	}

	memcpy(outputBuf, m_buffer + m_dataOffset, bytesToRead);
	m_dataOffset += bytesToRead;
	return bytesToRead;
}

#endif // USE_PHYSICS_ENGINE
