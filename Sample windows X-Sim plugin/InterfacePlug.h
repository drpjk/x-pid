// MSFlightSimDll.h : Hauptheaderdatei f�r die MSFlightSimDll DLL
//

#pragma once

#include "resource1.h"

#ifndef __AFXWIN_H__
	#error 'stdafx.h' muss vor dieser Datei in PCH eingeschlossen werden.
#endif

class CInterfacePluginV2App : public CWinApp
{
public:
	CInterfacePluginV2App();

	// �berschreibungen
public:
	virtual BOOL InitInstance();
	virtual int  ExitInstance();
	DECLARE_MESSAGE_MAP()
};

