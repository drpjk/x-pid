#pragma once

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN		
#endif

#ifndef WINVER				
#define WINVER 0x0501		
#endif

#ifndef _WIN32_WINNT		
#define _WIN32_WINNT 0x0501	
#endif						

#ifndef _WIN32_WINDOWS		
#define _WIN32_WINDOWS 0x0410
#endif

#ifndef _WIN32_IE			
#define _WIN32_IE 0x0600	
#endif

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS

#include <afxwin.h>         // MFC-Kern- und Standardkomponenten
#include <afxext.h>         // MFC-Erweiterungen

#ifndef _AFX_NO_OLE_SUPPORT
#include <afxole.h>         // MFC OLE-Klassen
#include <afxodlgs.h>       // MFC OLE-Dialogfeldklassen
#include <afxdisp.h>        // MFC-Automatisierungsklassen
#endif // _AFX_NO_OLE_SUPPORT

#ifndef _AFX_NO_DB_SUPPORT
#include <afxdb.h>			// MFC-ODBC-Datenbankklassen
#endif // _AFX_NO_DB_SUPPORT

#ifndef _AFX_NO_DAO_SUPPORT
#include <afxdao.h>			// MFC-DAO-Datenbankklassen
#endif // _AFX_NO_DAO_SUPPORT

#ifndef _AFX_NO_OLE_SUPPORT
#include <afxdtctl.h>
#endif
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			
#endif // _AFX_NO_AFXCMN_SUPPORT

#define ADRESOLUTIONDEVIDER 4194304.000 //For downscaling the 32bit X-Sim output to the 0...1023 of the AD converter of the MCU

struct DllInfo
{
	char			type[11];
	char			name[11];
};

struct IOsharing
{
	int				numberofoutputs;
	int				numberofinputs;
	char*			nameofoutputs[255];
	int				sizeofoutputsname[255];
	char*			nameofinputs[255];
	int				sizeofinputsname[255];
};

struct INPUTSTRUCT
{
	unsigned int	input[255];
	int				DeviceDetected;
};

struct XPIDSETTING
{
	bool			renameport;
	CString			renamename;
	int				eeprom_minimum;
	int				eeprom_maximum;
	int				eeprom_deadzone;
};

struct XPIDTO
{
	int comport;
	int portnumber;
	unsigned int virtualvalue;
	unsigned int oldvirtualvalue;
};

struct XPIDSTRUCT
{
	HANDLE					comporthandle;
	CString					comname;
	CString					devicepath;
	CString					friendlyname;
	bool					xpidpresent;
	XPIDSETTING				xpidsetting1;
	XPIDSETTING				xpidsetting2;
	CString					xpidname;
};
