//
// ftpworker.h
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

#ifndef _ftpworker_h
#define _ftpworker_h

#include <circle/net/ipaddress.h>
#include <circle/net/socket.h>
#include <circle/sched/task.h>
#include <circle/string.h>

// TODO: These may be incomplete/inaccurate
enum TFTPStatus
{
	FileStatusOk		= 150,

	Success			= 200,
	SystemType		= 215,
	ReadyForNewUser		= 220,
	ClosingControl		= 221,
	TransferComplete	= 226,
	EnteringPassiveMode	= 227,
	UserLoggedIn		= 230,
	FileActionOk		= 250,
	PathCreated		= 257,

	PasswordRequired	= 331,
	AccountRequired		= 332,
	PendingFurtherInfo	= 350,

	ServiceNotAvailable	= 421,
	DataConnectionFailed	= 425,
	FileActionNotTaken	= 450,
	ActionAborted		= 451,

	CommandUnrecognized	= 500,
	SyntaxError		= 501,
	CommandNotImplemented	= 502,
	BadCommandSequence	= 503,
	NotLoggedIn		= 530,
	FileNotFound		= 550,
	FileNameNotAllowed	= 553,
};

enum class TTransferMode
{
	Active,
	Passive,
};

enum class TDataType
{
	ASCII,
	Binary,
};

struct TFTPCommand;
struct TDirectoryListEntry;

class CFTPWorker : protected CTask
{
public:
	CFTPWorker(CSocket* pControlSocket, const char* pExpectedUser, const char* pExpectedPassword);
	virtual ~CFTPWorker() override;

	virtual void Run() override;

	static u8 GetInstanceCount() { return s_nInstanceCount; }

private:
	CSocket* OpenDataConnection();

	bool SendStatus(TFTPStatus StatusCode, const char* pMessage);

	bool CheckLoggedIn();

	// Directory navigation
	CString RealPath(const char* pInBuffer) const;
	const TDirectoryListEntry* BuildDirectoryList(size_t& nOutEntries) const;

	// FTP command handlers
	bool System(const char* pArgs);
	bool Username(const char* pArgs);
	bool Port(const char* pArgs);
	bool Passive(const char* pArgs);
	bool Password(const char* pArgs);
	bool Type(const char* pArgs);
	bool Retrieve(const char* pArgs);
	bool Store(const char* pArgs);
	bool Delete(const char* pArgs);
	bool MakeDirectory(const char* pArgs);
	bool ChangeWorkingDirectory(const char* pArgs);
	bool ChangeToParentDirectory(const char* pArgs);
	bool PrintWorkingDirectory(const char* pArgs);
	bool List(const char* pArgs);
	bool ListFileNames(const char* pArgs);
	bool RenameFrom(const char* pArgs);
	bool RenameTo(const char* pArgs);
	bool Bye(const char* pArgs);
	bool NoOp(const char* pArgs);

	CString m_LogName;

	// Authentication
	const char* m_pExpectedUser;
	const char* m_pExpectedPassword;

	// TCP sockets
	CSocket* m_pControlSocket;
	CSocket* m_pDataSocket;
	u16 m_nDataSocketPort;
	CIPAddress m_DataSocketIPAddress;

	// Command/data buffers
	char m_CommandBuffer[FRAME_BUFFER_SIZE];
	u8 m_DataBuffer[FRAME_BUFFER_SIZE];

	// Session state
	CString m_User;
	CString m_Password;
	TDataType m_DataType;
	TTransferMode m_TransferMode;
	CString m_CurrentPath;
	CString m_RenameFrom;

	static void FatFsPathToFTPPath(const char* pInBuffer, char* pOutBuffer, size_t nSize);
	static void FTPPathToFatFsPath(const char* pInBuffer, char* pOutBuffer, size_t nSize);

	static void FatFsParentPath(const char* pInBuffer, char* pOutBuffer, size_t nSize);

	static void FormatLastModifiedDate(u16 nDate, char* pOutBuffer, size_t nSize);
	static void FormatLastModifiedTime(u16 nDate, char* pOutBuffer, size_t nSize);

	static const TFTPCommand Commands[];
	static u8 s_nInstanceCount;
};

#endif
