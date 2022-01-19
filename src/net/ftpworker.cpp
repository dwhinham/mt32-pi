//
// ftpworker.cpp
//
// mt32-pi - A baremetal MIDI synthesizer for Raspberry Pi
// Copyright (C) 2020-2022 Dale Whinham <daleyo@gmail.com>
//
// This file is part of mt32-pi.
//
// mt32-pi is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// mt32-pi is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// mt32-pi. If not, see <http://www.gnu.org/licenses/>.
//

//#define FTPDAEMON_DEBUG

#include <circle/logger.h>
#include <circle/net/in.h>
#include <circle/net/netsubsystem.h>
#include <circle/sched/scheduler.h>
#include <circle/timer.h>
#include <fatfs/ff.h>

#include <cstdio>

#include "net/ftpworker.h"
#include "utility.h"

constexpr u16 PassivePortBase = 9000;
constexpr size_t TextBufferSize = 512;
constexpr unsigned int SocketTimeout = 20;
constexpr unsigned int NumRetries = 3;

#ifndef MT32_PI_VERSION
#define MT32_PI_VERSION "(version unknown)"
#endif

const char MOTDBanner[] = "Welcome to the mt32-pi " MT32_PI_VERSION " embedded FTP server!";

enum class TDirectoryListEntryType
{
	File,
	Directory,
};

struct TDirectoryListEntry
{
	char Name[FF_LFN_BUF + 1];
	TDirectoryListEntryType Type;
	u32 nSize;
	u16 nLastModifedDate;
	u16 nLastModifedTime;
};

using TCommandHandler = bool (CFTPWorker::*)(const char* pArgs);

struct TFTPCommand
{
	const char* pCmdStr;
	TCommandHandler pHandler;
};

const TFTPCommand CFTPWorker::Commands[] =
{
	{ "SYST",	&CFTPWorker::System			},
	{ "USER",	&CFTPWorker::Username			},
	{ "PASS",	&CFTPWorker::Password			},
	{ "TYPE",	&CFTPWorker::Type			},
	{ "PASV",	&CFTPWorker::Passive			},
	{ "PORT",	&CFTPWorker::Port			},
	{ "RETR",	&CFTPWorker::Retrieve			},
	{ "STOR",	&CFTPWorker::Store			},
	{ "DELE",	&CFTPWorker::Delete			},
	{ "RMD",	&CFTPWorker::Delete			},
	{ "MKD",	&CFTPWorker::MakeDirectory		},
	{ "CWD",	&CFTPWorker::ChangeWorkingDirectory	},
	{ "CDUP",	&CFTPWorker::ChangeToParentDirectory	},
	{ "PWD",	&CFTPWorker::PrintWorkingDirectory	},
	{ "LIST",	&CFTPWorker::List			},
	{ "NLST",	&CFTPWorker::ListFileNames		},
	{ "RNFR",	&CFTPWorker::RenameFrom			},
	{ "RNTO",	&CFTPWorker::RenameTo			},
	{ "BYE",	&CFTPWorker::Bye			},
	{ "QUIT",	&CFTPWorker::Bye			},
	{ "NOOP",	&CFTPWorker::NoOp			},
};

u8 CFTPWorker::s_nInstanceCount = 0;

// Volume names from ffconf.h
// TODO: Share with soundfontmanager.cpp
const char* const VolumeNames[] = { FF_VOLUME_STRS };

bool ValidateVolumeName(const char* pVolumeName)
{
	for (const auto pName : VolumeNames)
	{
		if (strcasecmp(pName, pVolumeName) == 0)
			return true;
	}

	return false;
}

// Comparator for sorting directory listings
inline bool DirectoryCaseInsensitiveAscending(const TDirectoryListEntry& EntryA, const TDirectoryListEntry& EntryB)
{
	// Directories first in ascending order
	if (EntryA.Type != EntryB.Type)
		return EntryA.Type == TDirectoryListEntryType::Directory;

	return strncasecmp(EntryA.Name, EntryB.Name, sizeof(TDirectoryListEntry::Name)) < 0;
}


CFTPWorker::CFTPWorker(CSocket* pControlSocket, const char* pExpectedUser, const char* pExpectedPassword)
	: CTask(TASK_STACK_SIZE),
	  m_LogName(),
	  m_pExpectedUser(pExpectedUser),
	  m_pExpectedPassword(pExpectedPassword),
	  m_pControlSocket(pControlSocket),
	  m_pDataSocket(nullptr),
	  m_nDataSocketPort(0),
	  m_DataSocketIPAddress(),
	  m_CommandBuffer{'\0'},
	  m_DataBuffer{0},
	  m_User(),
	  m_Password(),
	  m_DataType(TDataType::ASCII),
	  m_TransferMode(TTransferMode::Active),
	  m_CurrentPath(),
	  m_RenameFrom()
{
	++s_nInstanceCount;
	m_LogName.Format("ftpd[%d]", s_nInstanceCount);
}

CFTPWorker::~CFTPWorker()
{
	if (m_pControlSocket)
		delete m_pControlSocket;

	if (m_pDataSocket)
		delete m_pDataSocket;

	--s_nInstanceCount;

	CLogger::Get()->Write(m_LogName, LogNotice, "Instance count is now %d", s_nInstanceCount);
}

void CFTPWorker::Run()
{
	assert(m_pControlSocket != nullptr);

	const size_t nWorkerNumber = s_nInstanceCount;
	CLogger* const pLogger       = CLogger::Get();
	CScheduler* const pScheduler = CScheduler::Get();

	pLogger->Write(m_LogName, LogNotice, "Worker task %d spawned", nWorkerNumber);

	if (!SendStatus(TFTPStatus::ReadyForNewUser, MOTDBanner))
		return;

	CTimer* const pTimer = CTimer::Get();
	unsigned int nTimeout = pTimer->GetTicks();

	while (m_pControlSocket)
	{
		// Block while waiting to receive
#ifdef FTPDAEMON_DEBUG
		pLogger->Write(m_LogName, LogDebug, "Waiting for command");
#endif
		const int nReceiveBytes = m_pControlSocket->Receive(m_CommandBuffer, sizeof(m_CommandBuffer), MSG_DONTWAIT);

		if (nReceiveBytes == 0)
		{
			if (pTimer->GetTicks() - nTimeout >= SocketTimeout * HZ)
			{
				CLogger::Get()->Write(m_LogName, LogError, "Socket timed out");
				break;
			}

			pScheduler->Yield();
			continue;
		}

		if (nReceiveBytes < 0)
		{
			pLogger->Write(m_LogName, LogNotice, "Connection closed");
			break;
		}

		// FIXME
		m_CommandBuffer[nReceiveBytes - 2] = '\0';

#ifdef FTPDAEMON_DEBUG
		const u8* pIPAddress = m_pControlSocket->GetForeignIP();
		pLogger->Write(m_LogName, LogDebug, "<-- Received %d bytes from %d.%d.%d.%d: '%s'", nReceiveBytes, pIPAddress[0], pIPAddress[1], pIPAddress[2], pIPAddress[3], m_CommandBuffer);
#endif

		char* pSavePtr;
		char* pToken = strtok_r(m_CommandBuffer, " \r\n", &pSavePtr);

		if (!pToken)
		{
			pLogger->Write(m_LogName, LogError, "String tokenization error (received: '%s')", m_CommandBuffer);
			continue;
		}

		TCommandHandler pHandler = nullptr;
		for (size_t i = 0; i < Utility::ArraySize(Commands); ++i)
		{
			if (strcasecmp(pToken, Commands[i].pCmdStr) == 0)
			{
				pHandler = Commands[i].pHandler;
				break;
			}
		}

		if (pHandler)
			(this->*pHandler)(pSavePtr);
		else
			SendStatus(TFTPStatus::CommandNotImplemented, "Command not implemented.");

		nTimeout = pTimer->GetTicks();
	}

	pLogger->Write(m_LogName, LogNotice, "Worker task %d shutting down", nWorkerNumber);

	delete m_pControlSocket;
	m_pControlSocket = nullptr;
}

CSocket* CFTPWorker::OpenDataConnection()
{
	CSocket* pDataSocket = nullptr;
	u8 nRetries = NumRetries;

	while (pDataSocket == nullptr && nRetries > 0)
	{
		// Active: Create new socket and connect to client
		if (m_TransferMode == TTransferMode::Active)
		{
			CNetSubSystem* const pNet = CNetSubSystem::Get();
			pDataSocket = new CSocket(pNet, IPPROTO_TCP);

			if (pDataSocket == nullptr)
			{
				SendStatus(TFTPStatus::DataConnectionFailed, "Could not open socket.");
				return nullptr;
			}

			if (pDataSocket->Connect(m_DataSocketIPAddress, m_nDataSocketPort) < 0)
			{
				SendStatus(TFTPStatus::DataConnectionFailed, "Could not connect to data port.");
				delete pDataSocket;
				pDataSocket = nullptr;
			}
		}

		// Passive: Use previously-created socket and accept connection from client
		else if (m_TransferMode == TTransferMode::Passive && m_pDataSocket != nullptr)
		{
			CIPAddress ClientIPAddress;
			u16 nClientPort;
			pDataSocket = m_pDataSocket->Accept(&ClientIPAddress, &nClientPort);
		}

		--nRetries;
	}

	if (pDataSocket == nullptr)
	{
		CLogger::Get()->Write(m_LogName, LogError, "Unable to open data socket after %d attempts", NumRetries);
		SendStatus(TFTPStatus::DataConnectionFailed, "Couldn't open data connection.");
	}

	return pDataSocket;
}

bool CFTPWorker::SendStatus(TFTPStatus StatusCode, const char* pMessage)
{
	assert(m_pControlSocket != nullptr);
	CLogger* pLogger = CLogger::Get();

	const int nLength = snprintf(m_CommandBuffer, sizeof(m_CommandBuffer), "%d %s\r\n", StatusCode, pMessage);
	if (m_pControlSocket->Send(m_CommandBuffer, nLength, 0) < 0)
	{
		pLogger->Write(m_LogName, LogError, "Failed to send status");
		return false;
	}
#ifdef FTPDAEMON_DEBUG
	else
	{
		m_CommandBuffer[nLength - 2] = '\0';
		pLogger->Write(m_LogName, LogDebug, "--> Sent: '%s'", m_CommandBuffer);
	}
#endif

	return true;
}

bool CFTPWorker::CheckLoggedIn()
{
#ifdef FTPDAEMON_DEBUG
	CLogger* pLogger = CLogger::Get();
	pLogger->Write(m_LogName, LogDebug, "Username compare: expected '%s', actual '%s'", static_cast<const char*>(m_pExpectedUser), static_cast<const char*>(m_User));
	pLogger->Write(m_LogName, LogDebug, "Password compare: expected '%s', actual '%s'", static_cast<const char*>(m_pExpectedPassword),  static_cast<const char*>(m_Password));
#endif

	if (m_User.Compare(m_pExpectedUser) == 0 && m_Password.Compare(m_pExpectedPassword) == 0)
		return true;

	SendStatus(TFTPStatus::NotLoggedIn, "Not logged in.");
	return false;
}

CString CFTPWorker::RealPath(const char* pInBuffer) const
{
	assert(pInBuffer != nullptr);

	CString Path;
	const bool bAbsolute = pInBuffer[0] == '/';

	if (bAbsolute)
	{
		char Buffer[TextBufferSize];
		FTPPathToFatFsPath(pInBuffer, Buffer, sizeof(Buffer));
		Path = Buffer;
	}
	else
		Path.Format("%s/%s", static_cast<const char*>(m_CurrentPath), pInBuffer);

	return Path;
}

const TDirectoryListEntry* CFTPWorker::BuildDirectoryList(size_t& nOutEntries) const
{
	DIR Dir;
	FILINFO FileInfo;
	FRESULT Result;

	TDirectoryListEntry* pEntries = nullptr;
	nOutEntries = 0;

	// Volume list
	if (m_CurrentPath.GetLength() == 0)
	{
		constexpr size_t nVolumes = Utility::ArraySize(VolumeNames);
		bool VolumesAvailable[nVolumes] = { false };

		for (size_t i = 0; i < nVolumes; ++i)
		{
			char VolumeName[6];
			strncpy(VolumeName, VolumeNames[i], sizeof(VolumeName));
			strcat(VolumeName, ":");

			// Returns FR_
			if ((Result = f_opendir(&Dir, VolumeName)) == FR_OK)
			{
				f_closedir(&Dir);
				VolumesAvailable[i] = true;
				++nOutEntries;
			}
		}

		pEntries = new TDirectoryListEntry[nOutEntries];

		size_t nCurrentEntry = 0;
		for (size_t i = 0; i < nVolumes && nCurrentEntry < nOutEntries; ++i)
		{
			if (VolumesAvailable[i])
			{
				TDirectoryListEntry& Entry = pEntries[nCurrentEntry++];
				strncpy(Entry.Name, VolumeNames[i], sizeof(Entry.Name));
				Entry.Type = TDirectoryListEntryType::Directory;
				Entry.nSize = 0;
				Entry.nLastModifedDate = 0;
				Entry.nLastModifedTime = 0;
			}
		}

		return pEntries;
	}

	// Directory list
	Result = f_findfirst(&Dir, &FileInfo, m_CurrentPath, "*");
	if (Result == FR_OK && *FileInfo.fname)
	{
		// Count how many entries we need
		do
		{
			++nOutEntries;
			Result = f_findnext(&Dir, &FileInfo);
		} while (Result == FR_OK && *FileInfo.fname);

		f_closedir(&Dir);

		if (nOutEntries && (pEntries = new TDirectoryListEntry[nOutEntries]))
		{
			size_t nCurrentEntry = 0;
			Result = f_findfirst(&Dir, &FileInfo, m_CurrentPath, "*");
			while (Result == FR_OK && *FileInfo.fname)
			{
				TDirectoryListEntry& Entry = pEntries[nCurrentEntry++];
				strncpy(Entry.Name, FileInfo.fname, sizeof(Entry.Name));

				if (FileInfo.fattrib & AM_DIR)
				{
					Entry.Type = TDirectoryListEntryType::Directory;
					Entry.nSize = 0;
				}
				else
				{
					Entry.Type = TDirectoryListEntryType::File;
					Entry.nSize = FileInfo.fsize;
				}

				Entry.nLastModifedDate = FileInfo.fdate;
				Entry.nLastModifedTime = FileInfo.ftime;

				Result = f_findnext(&Dir, &FileInfo);
			}

			f_closedir(&Dir);

			Utility::QSort(pEntries, DirectoryCaseInsensitiveAscending, 0, nOutEntries - 1);
		}
	}

	return pEntries;
}

bool CFTPWorker::System(const char* pArgs)
{
	// Some FTP clients (e.g. Directory Opus) will only attempt to parse LIST responses as IIS/DOS-style if we pretend to be Windows NT
	SendStatus(TFTPStatus::SystemType, "Windows_NT");
	return true;
}

bool CFTPWorker::Username(const char* pArgs)
{
	m_User = pArgs;
	char Buffer[TextBufferSize];
	snprintf(Buffer, sizeof(Buffer), "Password required for '%s'.", static_cast<const char*>(m_User));
	SendStatus(TFTPStatus::PasswordRequired, Buffer);
	return true;
}

bool CFTPWorker::Port(const char* pArgs)
{
	if (!CheckLoggedIn())
		return false;

	char Buffer[TextBufferSize];
	strncpy(Buffer, pArgs, sizeof(Buffer));

	if (m_pDataSocket != nullptr)
	{
		delete m_pDataSocket;
		m_pDataSocket = nullptr;
	}

	m_TransferMode = TTransferMode::Active;

	// TODO: PORT IP Address should match original IP address

	u8 PortBytes[6];
	char* pSavePtr;
	char* pToken = strtok_r(Buffer, " ,", &pSavePtr);
	bool bParseError = (pToken == nullptr);

	if (!bParseError)
	{
		PortBytes[0] = static_cast<u8>(atoi(pToken));

		for (u8 i = 0; i < 5; ++i)
		{
			pToken = strtok_r(nullptr, " ,", &pSavePtr);
			if (pToken == nullptr)
			{
				bParseError = true;
				break;
			}

			PortBytes[i + 1] = static_cast<u8>(atoi(pToken));
		}
	}

	if (bParseError)
	{
		SendStatus(TFTPStatus::SyntaxError, "Syntax error.");
		return false;
	}

	m_DataSocketIPAddress.Set(PortBytes);
	m_nDataSocketPort = (PortBytes[4] << 8) + PortBytes[5];

#ifdef FTPDAEMON_DEBUG
	CString IPAddressString;
	m_DataSocketIPAddress.Format(&IPAddressString);
	CLogger::Get()->Write(m_LogName, LogDebug, "PORT set to: %s:%d", static_cast<const char*>(IPAddressString), m_nDataSocketPort);
#endif

	SendStatus(TFTPStatus::Success, "Command OK.");
	return true;
}

bool CFTPWorker::Passive(const char* pArgs)
{
	if (!CheckLoggedIn())
		return false;

	if (m_pDataSocket == nullptr)
	{
		m_TransferMode = TTransferMode::Passive;
		m_nDataSocketPort = PassivePortBase + s_nInstanceCount - 1;

		CNetSubSystem* const pNet = CNetSubSystem::Get();
		m_pDataSocket = new CSocket(pNet, IPPROTO_TCP);

		if (m_pDataSocket == nullptr)
		{
			SendStatus(TFTPStatus::ServiceNotAvailable, "Failed to open port for passive mode.");
			return false;
		}

		if (m_pDataSocket->Bind(m_nDataSocketPort) < 0)
		{
			SendStatus(TFTPStatus::DataConnectionFailed, "Could not bind to data port.");
			delete m_pDataSocket;
			m_pDataSocket = nullptr;
			return false;
		}

		if (m_pDataSocket->Listen() < 0)
		{
			SendStatus(TFTPStatus::DataConnectionFailed, "Could not listen on data port.");
			delete m_pDataSocket;
			m_pDataSocket = nullptr;
			return false;
		}
	}

	u8 IPAddress[IP_ADDRESS_SIZE];
	CNetSubSystem::Get()->GetConfig()->GetIPAddress()->CopyTo(IPAddress);

	char Buffer[TextBufferSize];
	snprintf(Buffer, sizeof(Buffer), "Entering passive mode (%d,%d,%d,%d,%d,%d).",
		IPAddress[0],
		IPAddress[1],
		IPAddress[2],
		IPAddress[3],
		(m_nDataSocketPort >> 8) & 0xFF,
		m_nDataSocketPort & 0xFF
	);

	SendStatus(TFTPStatus::EnteringPassiveMode, Buffer);
	return true;
}

bool CFTPWorker::Password(const char* pArgs)
{
	if (m_User.GetLength() == 0)
	{
		SendStatus(TFTPStatus::AccountRequired, "Need account for login.");
		return false;
	}

	m_Password = pArgs;

	if (!CheckLoggedIn())
		return false;

	SendStatus(TFTPStatus::UserLoggedIn, "User logged in.");
	return true;
}

bool CFTPWorker::Type(const char* pArgs)
{
	if (!CheckLoggedIn())
		return false;

	if (strcasecmp(pArgs, "A") == 0)
	{
		m_DataType = TDataType::ASCII;
		SendStatus(TFTPStatus::Success, "Type set to ASCII.");
		return true;
	}

	if (strcasecmp(pArgs, "I") == 0)
	{
		m_DataType = TDataType::Binary;
		SendStatus(TFTPStatus::Success, "Type set to binary.");
		return true;
	}

	SendStatus(TFTPStatus::SyntaxError, "Syntax error.");
	return false;
}

bool CFTPWorker::Retrieve(const char* pArgs)
{
	if (!CheckLoggedIn())
		return false;

	FIL File;
	CString Path = RealPath(pArgs);

	if (f_open(&File, Path, FA_READ) != FR_OK)
	{
		SendStatus(TFTPStatus::FileActionNotTaken, "Could not open file for reading.");
		return false;
	}

	if (!SendStatus(TFTPStatus::FileStatusOk, "Command OK."))
		return false;

	CSocket* pDataSocket = OpenDataConnection();
	if (pDataSocket == nullptr)
		return false;

	size_t nSize = f_size(&File);
	size_t nSent = 0;

	while (nSent < nSize)
	{
		UINT nBytesRead;
#ifdef FTPDAEMON_DEBUG
		CLogger::Get()->Write(m_LogName, LogDebug, "Sending data");
#endif
		if (f_read(&File, m_DataBuffer, sizeof(m_DataBuffer), &nBytesRead) != FR_OK || pDataSocket->Send(m_DataBuffer, nBytesRead, 0) < 0)
		{
			delete pDataSocket;
			f_close(&File);
			SendStatus(TFTPStatus::ActionAborted, "File action aborted, local error.");
			return false;
		}

		nSent += nBytesRead;
		assert(nSent <= nSize);
	}

	delete pDataSocket;
	f_close(&File);
	SendStatus(TFTPStatus::TransferComplete, "Transfer complete.");

	return false;
}

bool CFTPWorker::Store(const char* pArgs)
{
	if (!CheckLoggedIn())
		return false;

	FIL File;
	CString Path = RealPath(pArgs);

	if (f_open(&File, Path, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
	{
		SendStatus(TFTPStatus::FileActionNotTaken, "Could not open file for writing.");
		return false;
	}

	f_sync(&File);

	if (!SendStatus(TFTPStatus::FileStatusOk, "Command OK."))
		return false;

	CSocket* pDataSocket = OpenDataConnection();
	if (pDataSocket == nullptr)
		return false;

	bool bSuccess = true;

	CTimer* const pTimer = CTimer::Get();
	unsigned int nTimeout = pTimer->GetTicks();

	while (true)
	{
#ifdef FTPDAEMON_DEBUG
		CLogger::Get()->Write(m_LogName, LogDebug, "Waiting to receive");
#endif
		int nReceiveResult = pDataSocket->Receive(m_DataBuffer, sizeof(m_DataBuffer), MSG_DONTWAIT);
		FRESULT nWriteResult;
		UINT nWritten;

		if (nReceiveResult == 0)
		{
			if (pTimer->GetTicks() - nTimeout >= SocketTimeout * HZ)
			{
				CLogger::Get()->Write(m_LogName, LogError, "Socket timed out");
				bSuccess = false;
				break;
			}
			CScheduler::Get()->Yield();
			continue;
		}

		// All done
		if (nReceiveResult < 0)
		{
			CLogger::Get()->Write(m_LogName, LogNotice, "Receive done, no more data");
			break;
		}

#ifdef FTPDAEMON_DEBUG
		//CLogger::Get()->Write(m_LogName, LogDebug, "Received %d bytes", nReceiveResult);
#endif

		if ((nWriteResult = f_write(&File, m_DataBuffer, nReceiveResult, &nWritten)) != FR_OK)
		{
			CLogger::Get()->Write(m_LogName, LogError, "Write FAILED, return code %d", nWriteResult);
			bSuccess = false;
			break;
		}

		f_sync(&File);
		CScheduler::Get()->Yield();

		nTimeout = pTimer->GetTicks();
	}

	if (bSuccess)
		SendStatus(TFTPStatus::TransferComplete, "Transfer complete.");
	else
		SendStatus(TFTPStatus::ActionAborted, "File action aborted, local error.");

#ifdef FTPDAEMON_DEBUG
	CLogger::Get()->Write(m_LogName, LogDebug, "Closing socket/file");
#endif
	delete pDataSocket;
	f_close(&File);

	return true;
}

bool CFTPWorker::Delete(const char* pArgs)
{
	if (!CheckLoggedIn())
		return false;

	CString Path = RealPath(pArgs);

	if (f_unlink(Path) != FR_OK)
		SendStatus(TFTPStatus::FileActionNotTaken, "File was not deleted.");
	else
		SendStatus(TFTPStatus::FileActionOk, "File deleted.");

	return true;
}

bool CFTPWorker::MakeDirectory(const char* pArgs)
{
	if (!CheckLoggedIn())
		return false;

	CString Path = RealPath(pArgs);

	if (f_mkdir(Path) != FR_OK)
		SendStatus(TFTPStatus::FileActionNotTaken, "Directory creation failed.");
	else
	{
		char Buffer[TextBufferSize];
		FatFsPathToFTPPath(Path, Buffer, sizeof(Buffer));
		strcat(Buffer, " directory created.");
		SendStatus(TFTPStatus::PathCreated, Buffer);
	}

	return true;
}

bool CFTPWorker::ChangeWorkingDirectory(const char* pArgs)
{
	if (!CheckLoggedIn())
		return false;

	char Buffer[TextBufferSize];
	bool bSuccess = false;

	const bool bAbsolute = pArgs[0] == '/';
	if (bAbsolute)
	{
		// Root
		if (pArgs[1] == '\0')
		{
			m_CurrentPath = "";
			bSuccess = true;
		}
		else
		{
			DIR Dir;
			FTPPathToFatFsPath(pArgs, Buffer, sizeof(Buffer));

			// f_stat() will fail if we're trying to CWD to the root of a volume, so use f_opendir()
			if (f_opendir(&Dir, Buffer) == FR_OK)
			{
				f_closedir(&Dir);
				m_CurrentPath = Buffer;
				bSuccess = true;
			}
		}
	}
	else
	{
		const bool bAtRoot = m_CurrentPath.GetLength() == 0;
		if (bAtRoot)
		{
			if (ValidateVolumeName(pArgs))
			{
				m_CurrentPath.Format("%s:", pArgs);
				bSuccess = true;
			}
		}
		else
		{
			CString NewPath;
			NewPath.Format("%s/%s", static_cast<const char*>(m_CurrentPath), pArgs);

			if (f_stat(NewPath, nullptr) == FR_OK)
			{
				m_CurrentPath = NewPath;
				bSuccess = true;
			}
		}
	}

	if (bSuccess)
	{
		const bool bAtRoot = m_CurrentPath.GetLength() == 0;
		if (bAtRoot)
			strncpy(Buffer, "\"/\"", sizeof(Buffer));
		else
			FatFsPathToFTPPath(m_CurrentPath, Buffer, sizeof(Buffer));
		SendStatus(TFTPStatus::FileActionOk, Buffer);
	}
	else
		SendStatus(TFTPStatus::FileNotFound, "Directory unavailable.");

	return bSuccess;
}

bool CFTPWorker::ChangeToParentDirectory(const char* pArgs)
{
	if (!CheckLoggedIn())
		return false;

	char Buffer[TextBufferSize];
	bool bSuccess = false;
	bool bAtRoot = m_CurrentPath.GetLength() == 0;

	if (!bAtRoot)
	{
		DIR Dir;
		FatFsParentPath(m_CurrentPath, Buffer, sizeof(Buffer));

		bAtRoot = Buffer[0] == '\0';
		if (bAtRoot)
		{
			m_CurrentPath = Buffer;
			bSuccess = true;
		}
		else if (f_opendir(&Dir, Buffer) == FR_OK)
		{
			f_closedir(&Dir);
			m_CurrentPath = Buffer;
			bSuccess = true;
		}
	}

	if (bSuccess)
	{
		bAtRoot = m_CurrentPath.GetLength() == 0;
		if (bAtRoot)
			strncpy(Buffer, "\"/\"", sizeof(Buffer));
		else
			FatFsPathToFTPPath(m_CurrentPath, Buffer, sizeof(Buffer));
		SendStatus(TFTPStatus::FileActionOk, Buffer);
	}
	else
		SendStatus(TFTPStatus::FileNotFound, "Directory unavailable.");

	return false;
}

bool CFTPWorker::PrintWorkingDirectory(const char* pArgs)
{
	if (!CheckLoggedIn())
		return false;

	char Buffer[TextBufferSize];

	const bool bAtRoot = m_CurrentPath.GetLength() == 0;
	if (bAtRoot)
		strncpy(Buffer, "\"/\"", sizeof(Buffer));
	else
		FatFsPathToFTPPath(m_CurrentPath, Buffer, sizeof(Buffer));

	SendStatus(TFTPStatus::PathCreated, Buffer);

	return true;
}

bool CFTPWorker::List(const char* pArgs)
{
	if (!CheckLoggedIn())
		return false;

	if (!SendStatus(TFTPStatus::FileStatusOk, "Command OK."))
		return false;

	CSocket* pDataSocket = OpenDataConnection();
	if (pDataSocket == nullptr)
		return false;

	char Buffer[TextBufferSize];
	char Date[9];
	char Time[8];

	size_t nEntries;
	const TDirectoryListEntry* pDirEntries = BuildDirectoryList(nEntries);

	if (pDirEntries)
	{
		for (size_t i = 0; i < nEntries; ++i)
		{
			const TDirectoryListEntry& Entry = pDirEntries[i];
			int nLength;

			// Mimic the Microsoft IIS LIST format
			FormatLastModifiedDate(Entry.nLastModifedDate, Date, sizeof(Date));
			FormatLastModifiedTime(Entry.nLastModifedTime, Time, sizeof(Time));

			if (Entry.Type == TDirectoryListEntryType::Directory)
				nLength = snprintf(Buffer, sizeof(Buffer), "%-9s %-13s %-14s %s\r\n", Date, Time, "<DIR>", Entry.Name);
			else
				nLength = snprintf(Buffer, sizeof(Buffer), "%-9s %-13s %14d %s\r\n", Date, Time, Entry.nSize, Entry.Name);

			if (pDataSocket->Send(Buffer, nLength, 0) < 0)
			{
				delete[] pDirEntries;
				delete pDataSocket;
				SendStatus(TFTPStatus::DataConnectionFailed, "Transfer error.");
				return false;
			}
		}

		delete[] pDirEntries;
	}

	delete pDataSocket;
	SendStatus(TFTPStatus::TransferComplete, "Transfer complete.");
	return true;
}

bool CFTPWorker::ListFileNames(const char* pArgs)
{
	if (!CheckLoggedIn())
		return false;

	if (!SendStatus(TFTPStatus::FileStatusOk, "Command OK."))
		return false;

	CSocket* pDataSocket = OpenDataConnection();
	if (pDataSocket == nullptr)
		return false;

	char Buffer[TextBufferSize];
	size_t nEntries;
	const TDirectoryListEntry* pDirEntries = BuildDirectoryList(nEntries);

	if (pDirEntries)
	{
		for (size_t i = 0; i < nEntries; ++i)
		{
			const TDirectoryListEntry& Entry = pDirEntries[i];
			if (Entry.Type == TDirectoryListEntryType::Directory)
				continue;

			const int nLength = snprintf(Buffer, sizeof(Buffer), "%s\r\n", Entry.Name);
			if (pDataSocket->Send(Buffer, nLength, 0) < 0)
			{
				delete[] pDirEntries;
				delete pDataSocket;
				SendStatus(TFTPStatus::DataConnectionFailed, "Transfer error.");
				return false;
			}
		}

		delete[] pDirEntries;
	}

	delete pDataSocket;
	SendStatus(TFTPStatus::TransferComplete, "Transfer complete.");
	return true;
}

bool CFTPWorker::RenameFrom(const char* pArgs)
{
	if (!CheckLoggedIn())
		return false;

	m_RenameFrom = pArgs;
	SendStatus(TFTPStatus::PendingFurtherInfo, "Requested file action pending further information.");

	return false;
}

bool CFTPWorker::RenameTo(const char* pArgs)
{
	if (!CheckLoggedIn())
		return false;

	if (m_RenameFrom.GetLength() == 0)
	{
		SendStatus(TFTPStatus::BadCommandSequence, "Bad sequence of commands.");
		return false;
	}

	CString SourcePath = RealPath(m_RenameFrom);
	CString DestPath = RealPath(pArgs);

	if (f_rename(SourcePath, DestPath) != FR_OK)
		SendStatus(TFTPStatus::FileNameNotAllowed, "File name not allowed.");
	else
		SendStatus(TFTPStatus::FileActionOk, "File renamed.");

	m_RenameFrom = "";

	return false;
}

bool CFTPWorker::Bye(const char* pArgs)
{
	SendStatus(TFTPStatus::ClosingControl, "Goodbye.");
	delete m_pControlSocket;
	m_pControlSocket = nullptr;
	return true;
}

bool CFTPWorker::NoOp(const char* pArgs)
{
	SendStatus(TFTPStatus::Success, "Command OK.");
	return true;
}

void CFTPWorker::FatFsPathToFTPPath(const char* pInBuffer, char* pOutBuffer, size_t nSize)
{
	assert(pOutBuffer && nSize > 2);
	const char* pEnd = pOutBuffer + nSize;
	const char* pInChar = pInBuffer;
	char* pOutChar = pOutBuffer;

	*pOutChar++ = '"';
	*pOutChar++ = '/';

	while (*pInChar != '\0' && pOutChar < pEnd)
	{
		// Kill the volume colon
		if (*pInChar == ':')
		{
			*pOutChar++ = '/';
			++pInChar;

			// Kill any slashes after the colon
			while (*pInChar == '/') ++pInChar;
			continue;
		}

		// Kill duplicate slashes
		if (*pInChar == '/')
		{
			*pOutChar++ = *pInChar++;
			while (*pInChar == '/') ++pInChar;
			continue;
		}

		*pOutChar++ = *pInChar++;
	}

	// Kill trailing slash
	if (*(pOutChar - 1) == '/')
		--pOutChar;

	assert(pOutChar < pEnd - 2);
	*pOutChar++ = '"';
	*pOutChar++ = '\0';
}

void CFTPWorker::FTPPathToFatFsPath(const char* pInBuffer, char* pOutBuffer, size_t nSize)
{
	assert(pInBuffer && pOutBuffer);
	const char* pEnd = pOutBuffer + nSize;
	const char* pInChar = pInBuffer;
	char* pOutChar = pOutBuffer;

	// Kill leading slashes
	while (*pInChar == '/') ++pInChar;

	bool bGotVolume = false;
	while (*pInChar != '\0' && pOutChar < pEnd)
	{
		// Kill the volume colon
		if (!bGotVolume && *pInChar == '/')
		{
			bGotVolume = true;
			*pOutChar++ = ':';
			++pInChar;

			// Kill any slashes after the colon
			while (*pInChar == '/') ++pInChar;
			continue;
		}

		// Kill duplicate slashes
		if (*pInChar == '/')
		{
			*pOutChar++ = *pInChar++;
			while (*pInChar == '/') ++pInChar;
			continue;
		}

		*pOutChar++ = *pInChar++;
	}

	assert(pOutChar < pEnd - 2);

	// Kill trailing slash
	if (*(pOutChar - 1) == '/')
		--pOutChar;

	// Add volume colon
	if (!bGotVolume)
		*pOutChar++ = ':';

	*pOutChar++ = '\0';
}

void CFTPWorker::FatFsParentPath(const char* pInBuffer, char* pOutBuffer, size_t nSize)
{
	assert(pInBuffer != nullptr && pOutBuffer != nullptr);

	size_t nLength = strlen(pInBuffer);
	assert(nLength > 0 && nSize >= nLength);

	const char* pLastChar = pInBuffer + nLength - 1;
	const char* pInChar = pLastChar;

	// Kill trailing slashes
	while (*pInChar == '/' && pInChar > pInBuffer) --pInChar;

	// Kill subdirectory name
	while (*pInChar != '/' && *pInChar != ':' && pInChar > pInBuffer) --pInChar;

	// Kill trailing slashes
	while (*pInChar == '/' && pInChar > pInBuffer) --pInChar;

	// Pointer didn't move (we're already at a volume root), or we reached the start of the string (path invalid)
	if (pInChar == pLastChar || pInChar == pInBuffer)
	{
		*pOutBuffer = '\0';
		return;
	}

	// Truncate string
	nLength = pInChar - pInBuffer + 1;
	memcpy(pOutBuffer, pInBuffer, nLength);
	pOutBuffer[nLength] = '\0';
}

void CFTPWorker::FormatLastModifiedDate(u16 nDate, char* pOutBuffer, size_t nSize)
{
	// 2-digit year
	const u16 nYear = (1980 + (nDate >> 9)) % 100;
	u16 nMonth = (nDate >> 5) & 0x0F;
	u16 nDay = nDate & 0x1F;

	if (nMonth == 0)
		nMonth = 1;
	if (nDay == 0)
		nDay = 1;

	snprintf(pOutBuffer, nSize, "%02d-%02d-%02d", nMonth, nDay, nYear);
}

void CFTPWorker::FormatLastModifiedTime(u16 nDate, char* pOutBuffer, size_t nSize)
{
	u16 nHour = (nDate >> 11) & 0x1F;
	const u16 nMinute = (nDate >> 5) & 0x3F;
	const char* pSuffix = nHour < 12 ? "AM" : "PM";

	if (nHour == 0)
		nHour = 12;
	else if (nHour >= 12)
		nHour -= 12;

	snprintf(pOutBuffer, nSize, "%02d:%02d%s", nHour, nMinute, pSuffix);
}
