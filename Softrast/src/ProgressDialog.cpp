#include "ProgressDialog.h"
#include "../windows/resource.h"
#include <Windows.h>
#include <CommCtrl.h>

#include <algorithm>
#include <string>
#include <assert.h>

extern HWND MainWindow;

//#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace ProgressDialog
{
	static float CurrentProgress = 0.0f;
	static bool Initialized = false;
	static HWND Dialog;
	HWND ProgressBar;
	static HWND Text;
	static std::string TextString;

	static LRESULT CALLBACK DlgProc ( HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam )
	{
		return FALSE;
	}

	void SetDialogParameters ( bool visible, float progress, const char* text )
	{
		if ( text && text != TextString )
			TextString = text;
		else
			text = nullptr;
		float actualProgress = std::max ( 0.0f, std::min ( 1.0f, progress ) );

		if ( Dialog == NULL )
		{
			Dialog      = CreateDialogA ( GetModuleHandle ( NULL ), MAKEINTRESOURCE(IDD_PROGRESS_DIALOG), MainWindow, DlgProc );
			ProgressBar = GetDlgItem ( Dialog, IDC_PROGRESS_DIALOG_PROGRESS );
			Text        = GetDlgItem ( Dialog, IDC_PROGRESS_DIALOG_TEXT );
			assert ( ProgressBar && Text );

			SendMessage ( ProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 65535) );
		}

		if ( visible )
			ShowWindow ( Dialog, SW_SHOW );
		else
			ShowWindow ( Dialog, SW_HIDE );

		WPARAM progressInt = (WPARAM)(actualProgress * 65535);
		SendMessage ( ProgressBar, PBM_SETPOS, progressInt, 0 ); 
		if ( text )
			SetWindowTextA ( Text, text );

		MSG Msg;
		while ( PeekMessage ( &Msg, NULL, 0, 0, PM_REMOVE ) )
		{
			if(!IsDialogMessage(ProgressBar, &Msg))
			{
				TranslateMessage(&Msg);
				DispatchMessage(&Msg);
			}
		}
	}
};