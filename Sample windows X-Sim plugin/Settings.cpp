// Settings.cpp: Implementierungsdatei
//

#include "stdafx.h"
#include "registry.h"
#include "InterfacePlug.h"
#include "Settings.h"
#include <vector>
#include "StaticMemDC.h"

using namespace std;

extern vector <XPIDSTRUCT> xpid_struct;
extern vector <XPIDTO> xpid_to;
extern CString regkey;
extern CRegistry regMyReg;
extern int DeviceDetected;
extern bool shutdowndialog;
extern bool settingdialogopened;
extern bool debugmode;
extern CRITICAL_SECTION CriticalSection;
extern int timeoutdelay;
extern BOOL enabletimeout;
int min1, min2, max1, max2, dead1, dead2;
double p1, i1, d1, p2, i2, d2;
CButton m_graph;

///////////////////////
//Analyser Graph Thread
///////////////////////
 
#define analyserheight			100
#define analyserwidth			500
volatile unsigned int	lastvaluesinput[analyserwidth-2]; //-2 to remove borders
volatile unsigned int	lastvaluesoutput[analyserwidth-2]; //-2 to remove borders
CBitmap diagrambitmap;
CDC* pDCA;
bool analysergraphrunning=false;
bool globalshutdown=false;
int analogue1=0;
int analogue2=0;
int s_analogue1=0;
int s_analogue2=0;
CStaticMemDC m_diagram;
CWinThread*	analysergraphthreadhandle;
int currentcomport;
int currentoutput;

void cleardiagram()
{
	//Zero all last values
	for(int z=0; z < (analyserwidth-2); z++){lastvaluesinput[z]=49; lastvaluesoutput[z]=49;}
}

UINT AnalyserGraphThread(LPVOID param)
{
	int gridx=10;
	int gridy=4;
	int gridcountx=0;
	double gridoffsety=double(analyserheight)/double(gridy);
	double gridoffsetx=double(analyserwidth)/double(gridx);
	//int divider = (INT_MAX / 90)*2;
	CDC memDC;
	memDC.CreateCompatibleDC(pDCA);
	CPen penblack;
	penblack.CreatePen(PS_SOLID,1,RGB(0,0,0));
	CPen pengrey;
	pengrey.CreatePen(PS_SOLID,1,RGB(210,210,210));
	CPen penblue;
	penblue.CreatePen(PS_SOLID,1,RGB(0,0,255));
	CPen penred;
	penred.CreatePen(PS_SOLID,1,RGB(255,0,0));
	analysergraphrunning=true;
	while(globalshutdown==false)
	{
		if(m_graph.GetCheck()==TRUE)
		{
			for(int z=0; z < (analyserwidth-3); z++){lastvaluesinput[z]=lastvaluesinput[z+1]; lastvaluesoutput[z]=lastvaluesoutput[z+1];}
			//Analysergraph need real time input, also if dialog is not visible
			if(currentoutput==0)
			{
				lastvaluesinput[analyserwidth-3]=int(double(s_analogue1)/10.437);
			}
			else
			{
				lastvaluesinput[analyserwidth-3]=int(double(s_analogue2)/10.437);
			}
			lastvaluesoutput[analyserwidth-3]=int(double(xpid_to[currentcomport+currentoutput].virtualvalue)/43826197.000);
		}
		//Draw blank graph with offset lines
		diagrambitmap.DeleteObject();
		diagrambitmap.CreateCompatibleBitmap(pDCA, analyserwidth, analyserheight);
		CBitmap* pOldBitmap = memDC.SelectObject((CBitmap*)&diagrambitmap);
		memDC.FillSolidRect(0,0,analyserwidth,analyserheight,RGB(250,250,250));
		//Draw a surrounding box
		memDC.SelectObject (&penblack);
		memDC.MoveTo(0,0);
		memDC.LineTo(analyserwidth-1,0);
		memDC.LineTo(analyserwidth-1,analyserheight-1);
		memDC.LineTo(0,analyserheight-1);
		memDC.LineTo(0,0);
		//Draw a moving grid
		memDC.SelectObject (&pengrey);
		for(int g1=1; g1 < gridy; g1++)
		{
			int lineoffset=int(double(gridoffsety*double(g1)));
			memDC.MoveTo(1,lineoffset);
			memDC.LineTo((analyserwidth-1),lineoffset);
		}
		for(int g2=1; g2 < gridx+1; g2++)
		{
			int lineoffset=int(double(gridoffsetx*double(g2)));
			if(lineoffset-gridcountx < analyserwidth-2)
			{
				memDC.MoveTo(lineoffset-gridcountx,1);
				memDC.LineTo(lineoffset-gridcountx,analyserheight-1);
			}
		}
		if(currentcomport != -1 && m_graph.GetCheck()==TRUE)
		{
			//Draw current analogue value
			memDC.SelectObject (&penred);
			bool first=true;
			int offset=lastvaluesinput[0];
			memDC.MoveTo(1,(analyserheight-2)-offset);
			for(int z=0; z < (analyserwidth-2); z++)
			{
				memDC.LineTo(z+2,(analyserheight-2)-lastvaluesinput[z]);
			}
			//Draw current analogue value
			memDC.SelectObject (&penblue);
			first=true;
			offset=lastvaluesoutput[0];
			memDC.MoveTo(1,(analyserheight-2)-offset);
			for(int z=0; z < (analyserwidth-2); z++)
			{
				memDC.LineTo(z+2,(analyserheight-2)-lastvaluesoutput[z]);
			}
		}
		memDC.SelectObject((CBitmap*)pOldBitmap);
		m_diagram.SetBitmap(diagrambitmap);
		gridcountx++;
		if(gridcountx >= int(gridoffsetx)){gridcountx=0;}
		Sleep(200);
	}
	//ReleaseDC(pDC);
	analysergraphrunning=false;
	return 0;
}

BOOL ClearEEPRom(HANDLE comhandle)
{
	if(comhandle == NULL || comhandle == INVALID_HANDLE_VALUE){return -1;}
	unsigned int resval=0;
	BOOL success;
	COMMTIMEOUTS timeouts;
	//Ask the firmware on comport for the PID counter value
	char command[4]={'X',205,0,0};
	DWORD bytesTransferred;
	// Send the command to the device.
	success = WriteFile(comhandle, &command, 4, &bytesTransferred, NULL);
	if (!success || bytesTransferred != 4){return FALSE;}
	Sleep(100);
	return TRUE;
}

int ReadEEPRomValue(HANDLE comhandle, int mem_address)
{
	if(comhandle == NULL || comhandle == INVALID_HANDLE_VALUE){return -1;}
	unsigned int resval=0;
	BOOL success;
	COMMTIMEOUTS timeouts;
	//Ask the firmware on comport for the PID counter value
	byte response[3];
	char command[4]={'X',204,0,0};
	command[2]=mem_address;
	DWORD bytesTransferred;
	// Send the command to the device.
	success = WriteFile(comhandle, &command, 4, &bytesTransferred, NULL);
	if (!success || bytesTransferred != 4){return -1;}
	// Read the response from the device.
	success = ReadFile(comhandle, response, 6, &bytesTransferred, NULL);
	Sleep(10);
	if (!success || bytesTransferred != 3){return -1;}
	if(response[0]=='X' && response[1]==204)
	{
		return int(response[2]);
	}
	return -1;
}

int ReadEEPRomWord(HANDLE comhandle, int mem_address)
{
	if(comhandle == NULL || comhandle == INVALID_HANDLE_VALUE){return -1;}
	int low,high, returnvalue;
	high=ReadEEPRomValue(comhandle, mem_address);
	low=ReadEEPRomValue(comhandle, mem_address+1);
	returnvalue=(high*256)+low;
	return returnvalue;
}

BOOL WriteEEPRomValue(HANDLE comhandle, int mem_address, int byte_value)
{
	if(comhandle == NULL || comhandle == INVALID_HANDLE_VALUE){return FALSE;}
	unsigned int resval=0;
	BOOL success;
	COMMTIMEOUTS timeouts;
	//Ask the firmware on comport for the PID counter value
	char command[4]={'X',203,0,0};
	command[2]=mem_address;
	command[3]=byte_value;
	DWORD bytesTransferred;
	// Send the command to the device.
	success = WriteFile(comhandle, &command, 4, &bytesTransferred, NULL);
	Sleep(20);
	if (!success || bytesTransferred != 4){return FALSE;}
	return TRUE;
}

BOOL WriteEEPRomWord(HANDLE comhandle, int mem_address, int byte_value)
{
	if(comhandle == NULL || comhandle == INVALID_HANDLE_VALUE){return FALSE;}
	int low,high;
	high=byte_value/256;
	low=byte_value-(256*high);
	WriteEEPRomValue(comhandle,mem_address,high);
	WriteEEPRomValue(comhandle,mem_address+1,low);
	return TRUE;
}

void WriteXPIDSettings(HANDLE comhandle)
{
	if(comhandle == NULL || comhandle == INVALID_HANDLE_VALUE){return;}
	WriteEEPRomWord(comhandle,1,min1);
	WriteEEPRomWord(comhandle,3,max1);
	WriteEEPRomValue(comhandle,5,dead1);
	WriteEEPRomWord(comhandle,6,min2);
	WriteEEPRomWord(comhandle,8,max2);
	WriteEEPRomValue(comhandle,10,dead2);
	WriteEEPRomWord(comhandle,11,int(p1*10.000));
	WriteEEPRomWord(comhandle,13,int(i1*10.000));
	WriteEEPRomWord(comhandle,15,int(d1*10.000));
	WriteEEPRomWord(comhandle,17,int(p2*10.000));
	WriteEEPRomWord(comhandle,19,int(i2*10.000));
	WriteEEPRomWord(comhandle,21,int(d2*10.000));
	return;
}

BOOL RereadXPIDSettings(HANDLE comhandle)
{
	if(comhandle == NULL || comhandle == INVALID_HANDLE_VALUE){return FALSE;}
	unsigned int resval=0;
	BOOL success;
	COMMTIMEOUTS timeouts;
	//Ask the firmware on comport for the PID counter value
	char command[4]={'X',206,0,0};
	DWORD bytesTransferred;
	// Send the command to the device.
	success = WriteFile(comhandle, &command, 4, &bytesTransferred, NULL);
	Sleep(20);
	if (!success || bytesTransferred != 4){return FALSE;}
	return TRUE;
}

void GetXPIDSettings(HANDLE comhandle)
{
	if(comhandle == NULL || comhandle == INVALID_HANDLE_VALUE){return;}
	min1=ReadEEPRomWord(comhandle,1);
	max1=ReadEEPRomWord(comhandle,3);
	dead1=ReadEEPRomValue(comhandle,5);
	min2=ReadEEPRomWord(comhandle,6);
	max2=ReadEEPRomWord(comhandle,8);
	dead2=ReadEEPRomValue(comhandle,10);
	p1=double(ReadEEPRomWord(comhandle,11))/10.000;
	i1=double(ReadEEPRomWord(comhandle,13))/10.000;
	d1=double(ReadEEPRomWord(comhandle,15))/10.000;
	p2=double(ReadEEPRomWord(comhandle,17))/10.000;
	i2=double(ReadEEPRomWord(comhandle,19))/10.000;
	d2=double(ReadEEPRomWord(comhandle,21))/10.000;
	return;
}

void ReadWholeEEPRom(HANDLE comhandle)
{
	if(comhandle == NULL || comhandle == INVALID_HANDLE_VALUE){return;}
	int res[23]={-1};
	for(int z=0; z < 23; z++)
	{
		res[z]=ReadEEPRomValue(comhandle,z);
	}
	//WriteEEPRomValue(comhandle, 0, 12);
	//ClearEEPRom(comhandle);
	//WriteEEPRomValue(comhandle,0,255);
	return;
}


unsigned int GetPidCounter(HANDLE comhandle)
{
	unsigned int resval=0;
	BOOL success;
	COMMTIMEOUTS timeouts;
	//Ask the firmware on comport for the PID counter value
	byte response[6];
	char command[4]={'X',201,0,0};
	DWORD bytesTransferred;
	// Send the command to the device.
	success = WriteFile(comhandle, &command, 4, &bytesTransferred, NULL);
	if (!success || bytesTransferred != 4){return 0;}
	// Read the response from the device.
	success = ReadFile(comhandle, response, 6, &bytesTransferred, NULL);
	if (!success || bytesTransferred != 6){return 0;}
	if(response[0]=='X' && response[1]==201)
	{
		resval=(response[2]*16777216)+(response[3]*65536)+(response[4]*256)+response[5];
	}
	return resval;
}

BOOL GetDebugValues(HANDLE comhandle, byte* debugbyte, int* debuginteger, double* debugdouble)
{
	int recdouble=0;
	BOOL success;
	//Ask the firmware on comport for the PID counter value
	byte response[7];
	char command[4]={'X',211,0,0};
	DWORD bytesTransferred;
	// Send the command to the device.
	success = WriteFile(comhandle, &command, 4, &bytesTransferred, NULL);
	if (!success || bytesTransferred != 4){return FALSE;}
	// Read the response from the device.
	success = ReadFile(comhandle, response, 7, &bytesTransferred, NULL);
	if (!success || bytesTransferred != 7){return FALSE;}
	if(response[0]=='X' && response[1]==211)
	{
		*debugbyte=response[2];
		*debuginteger=(response[3]*256)+response[4];
		*debugdouble=double((response[5]*256)+response[6])/10.000;
		return TRUE;
	}
	return FALSE;
}

void GetAnalogueValue(HANDLE comhandle, int* analogue1, int* analogue2)
{
	*analogue1=-1;
	*analogue2=-1;
	unsigned int resval=0;
	BOOL success;
	COMMTIMEOUTS timeouts;
	//Ask the firmware on comport for the PID counter value
	byte response[6];
	char command[4]={'X',200,0,0};
	DWORD bytesTransferred;
	// Send the command to the device.
	success = WriteFile(comhandle, &command, 4, &bytesTransferred, NULL);
	if (!success || bytesTransferred != 4){return;}
	// Read the response from the device.
	success = ReadFile(comhandle, response, 6, &bytesTransferred, NULL);
	if (!success || bytesTransferred != 6){return;}
	if(response[0]=='X' && response[1]==200)
	{
		*analogue1=(response[2]*256)+response[3];
		*analogue2=(response[4]*256)+response[5];
	}
}

IMPLEMENT_DYNAMIC(CSettings, CDialog)

CSettings::CSettings(CWnd* pParent /*=NULL*/)
	: CDialog(CSettings::IDD, pParent)
{
	currentoutput=-1;
	currentcomport=-1;
	oldpidcounter=0;
	olddiff=0;
}

CSettings::~CSettings()
{
}

void CSettings::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMPORT, m_comportname);	  
	DDX_Control(pDX, IDC_XPID_COMBO, m_xpidcombo);
	DDX_Control(pDX, IDC_DEVICEPATH, m_devicepath);	  
	DDX_Control(pDX, IDC_COMPORTNAME, m_comportdescription);
	DDX_Control(pDX, IDC_RENAMEPORT, m_renamecomport);
	DDX_Control(pDX, IDC_SETNAME_BUTTON, m_setnamebutton);
	DDX_Control(pDX, IDC_RENAME_COMBO, m_renametext);
	DDX_Control(pDX, IDC_TIMEOUT_CHECK, m_enable_timeout);
	DDX_Control(pDX, IDC_TIMEOUT_EDIT, m_timeout_delay);
	DDX_Control(pDX, IDC_PID_COUNTER, m_pid_counter);
	DDX_Control(pDX, IDC_FEEDBACK1, m_feedback1);
	DDX_Control(pDX, IDC_FEEDBACK2, m_feedback2);
	DDX_Control(pDX, IDC_DEADZONE1_SPIN, m_deadzone1_spin);
	DDX_Control(pDX, IDC_DEADZONE2_SPIN, m_deadzone2_spin);
	DDX_Control(pDX, IDC_DEADZONE1, m_deadzone1_text);
	DDX_Control(pDX, IDC_DEADZONE2, m_deadzone2_text);
	DDX_Control(pDX, IDC_MINIMUM_1, m_minimum1_text);
	DDX_Control(pDX, IDC_MAXIMUM_1, m_maximum1_text);
	DDX_Control(pDX, IDC_MAXIMUM_2, m_maximum2_text);
	DDX_Control(pDX, IDC_MINIMUM_2, m_minimum2_text);
	DDX_Control(pDX, IDC_SCALED_FEEDBACK1, m_feedback_scaled_1);
	DDX_Control(pDX, IDC_SCALED_FEEDBACK2, m_feedback_scaled_2);
	DDX_Control(pDX, IDC_P1_SPIN, m_p1_spin);
	DDX_Control(pDX, IDC_I1_SPIN, m_i1_spin);
	DDX_Control(pDX, IDC_D1_SPIN, m_d1_spin);
	DDX_Control(pDX, IDC_P2_SPIN, m_p2_spin);
	DDX_Control(pDX, IDC_I2_SPIN, m_i2_spin);
	DDX_Control(pDX, IDC_D2_SPIN, m_d2_spin);
	DDX_Control(pDX, IDC_DEBUG_BYTE, m_debugbyte);
	DDX_Control(pDX, IDC_DEBUG_DOUBLE, m_debugdouble);
	DDX_Control(pDX, IDC_DEBUG_INTEGER, m_debuginteger);
	DDX_Control(pDX, IDC_P1_TEXT, m_p1_text);
	DDX_Control(pDX, IDC_D1_TEXT, m_d1_text);
	DDX_Control(pDX, IDC_I1_TEXT, m_i1_text);
	DDX_Control(pDX, IDC_P2_TEXT, m_p2_text);
	DDX_Control(pDX, IDC_D2_TEXT, m_d2_text);
	DDX_Control(pDX, IDC_I2_TEXT, m_i2_text);
	DDX_Control(pDX, IDC_ANALYZER, m_diagram);
	DDX_Control(pDX, IDC_GRAPH_CHECK, m_graph);
}


BEGIN_MESSAGE_MAP(CSettings, CDialog)
	ON_WM_CTLCOLOR()
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_RENAMEPORT, &CSettings::OnBnClickedRenameport)
	ON_WM_CLOSE()
	ON_CBN_SELENDOK(IDC_SCN5COMBO, &CSettings::OnCbnSelendokXpidCombo)
	ON_BN_CLICKED(IDC_TIMEOUT_CHECK, &CSettings::OnBnClickedTimeoutCheck)
	ON_EN_CHANGE(IDC_TIMEOUT_EDIT, &CSettings::OnEnChangeTimeoutEdit)
	ON_BN_CLICKED(IDC_SETNAME_BUTTON, &CSettings::OnBnClickedSetnameButton)
	ON_BN_CLICKED(IDC_CLOSE_BUTTON, &CSettings::OnBnClickedCloseButton)
	ON_BN_CLICKED(IDC_SET_BUTTON_MINIMUM_1, &CSettings::OnBnClickedSetButtonMinimum1)
	ON_BN_CLICKED(IDC_SET_BUTTON_MAXIMUM_1, &CSettings::OnBnClickedSetButtonMaximum1)
	ON_BN_CLICKED(IDC_SET_BUTTON_MINIMUM_2, &CSettings::OnBnClickedSetButtonMinimum2)
	ON_BN_CLICKED(IDC_SET_BUTTON_MAXIMUM_2, &CSettings::OnBnClickedSetButtonMaximum2)
	ON_NOTIFY(UDN_DELTAPOS, IDC_DEADZONE1_SPIN, &CSettings::OnDeltaposDeadzone1Spin)
	ON_NOTIFY(UDN_DELTAPOS, IDC_DEADZONE2_SPIN, &CSettings::OnDeltaposDeadzone2Spin)
	ON_NOTIFY(UDN_DELTAPOS, IDC_P1_SPIN, &CSettings::OnDeltaposP1Spin)
	ON_NOTIFY(UDN_DELTAPOS, IDC_I1_SPIN, &CSettings::OnDeltaposI1Spin)
	ON_NOTIFY(UDN_DELTAPOS, IDC_D1_SPIN, &CSettings::OnDeltaposD1Spin)
	ON_NOTIFY(UDN_DELTAPOS, IDC_P2_SPIN, &CSettings::OnDeltaposP2Spin)
	ON_NOTIFY(UDN_DELTAPOS, IDC_I2_SPIN, &CSettings::OnDeltaposI2Spin)
	ON_NOTIFY(UDN_DELTAPOS, IDC_D2_SPIN, &CSettings::OnDeltaposD2Spin)
	ON_BN_CLICKED(IDC_RESET_1, &CSettings::OnBnClickedReset1)
	ON_BN_CLICKED(IDC_RESET_2, &CSettings::OnBnClickedReset2)
	ON_BN_CLICKED(IDC_ENABLE_DEBUG, &CSettings::OnBnClickedEnableDebug)
	ON_BN_CLICKED(IDC_GRAPH_CHECK, &CSettings::OnBnClickedGraphCheck)
END_MESSAGE_MAP()


// CSettings-Meldungshandler

BOOL CSettings::OnInitDialog()
{
	delayedupdate=false;
	m_DialogBrush.CreateSolidBrush(RGB(255,255,255));
	CDialog::OnInitDialog();
	m_deadzone1_spin.SetRange32(0,5);
	m_deadzone2_spin.SetRange32(0,5);
	m_p1_spin.SetRange32(0,100);
	m_p2_spin.SetRange32(0,100);
	m_i1_spin.SetRange32(0,100);
	m_i2_spin.SetRange32(0,100);
	m_d1_spin.SetRange32(0,100);
	m_d2_spin.SetRange32(0,100);
	//Fill combo
	int count=xpid_struct.size();
	for(int z=0; z < count; z++)
	{
		for(int y=0; y < 2; y++)
		{
			CString motorstring;
			motorstring.Format(" position %d",2-y);
			if(xpid_struct[z].xpidpresent==true)
			{
				m_xpidcombo.AddString(xpid_struct[z].xpidname+motorstring);
			}
		}
	}
	//Set the timeout delay
	m_enable_timeout.SetCheck(enabletimeout);
	CString outstring;
	outstring.Format("%d",timeoutdelay);
	m_timeout_delay.SetWindowText(outstring);
	//Fill rename combo
	m_renametext.AddString("Left actuator");
	m_renametext.AddString("Right actuator");
	m_renametext.AddString("Rotation actuator");
	m_renametext.AddString("XPID actuator 1");
	m_renametext.AddString("XPID actuator 2");
	m_renametext.AddString("XPID actuator 3");
	m_renametext.AddString("XPID actuator 4");
	m_renametext.AddString("XPID actuator 5");
	m_renametext.AddString("XPID actuator 6");
	//Receive timeout
	if(count > 0){m_xpidcombo.SetCurSel(1);}
	OnCbnSelendokXpidCombo();
	SetTimer(1,1000,NULL);
	cleardiagram();
	//Start Graph
	pDCA = GetDC();
	analysergraphthreadhandle=AfxBeginThread(AnalyserGraphThread,this,THREAD_PRIORITY_NORMAL);
	return FALSE;
}

HBRUSH CSettings::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	if(nCtlColor & CTLCOLOR_STATIC)
	{
		// Set the static text to black on white.
		pDC->SetTextColor(RGB(0, 0, 0));
		pDC->SetBkColor(RGB(255,255,255));
		// Drop through to return the background brush.
	}
	if(nCtlColor & CTLCOLOR_DLG){return m_DialogBrush;}
	return CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CSettings::OnTimer(UINT_PTR nIDEvent)
{
	if(shutdowndialog==true)
	{
		OnBnClickedCloseButton();
	}
	if(nIDEvent==1)
	{
		unsigned int pidcounter=0;
		//Show PID counter and the counts per second
		if(currentcomport != -1)
		{
			//Read PID Counter
			pidcounter=GetPidCounter(xpid_struct[currentcomport].comporthandle);
			if(pidcounter != 0)
			{
				if (oldpidcounter != 0)
				{
					int diff=pidcounter-oldpidcounter;
					if(diff >= 0 && diff < (2 * olddiff))
					{
						CString outputstring;
						outputstring.Format("%d",diff);
						m_pid_counter.SetWindowText(outputstring);
					}
					olddiff=diff;
				}
				else
				{
					m_pid_counter.SetWindowText("0");
				}
			}
			//Read analogue values
			GetAnalogueValue(xpid_struct[currentcomport].comporthandle,&analogue1,&analogue2);
			if(analogue1 != -1 && analogue2 != -1)
			{
				CString outstring;
				outstring.Format("%d",analogue1);
				m_feedback1.SetWindowText(outstring);
				outstring.Format("%d",analogue2);
				m_feedback2.SetWindowText(outstring);
				s_analogue1=analogue1;
				s_analogue2=analogue2;
			}
			//Update scaled feedback 1
			double scaler=max1-min1;
			scaler=1024.000/scaler;
			int nmax=analogue1;
			if(nmax > max1){nmax=max1;}
			if(nmax < min1){nmax=min1;}
			double noffset=nmax-min1;
			noffset=noffset*scaler;
			noffset=(noffset/10.24)+0.500;
			CString outstring;
			outstring.Format("%d",int(noffset));
			outstring=outstring+" \%";
			m_feedback_scaled_1.SetWindowText(outstring);
			//Update scaled feedback 2
			scaler=max2-min2;
			scaler=1024.000/scaler;
			nmax=analogue2;
			if(nmax > max2){nmax=max2;}
			if(nmax < min2){nmax=min2;}
			noffset=nmax-min2;
			noffset=noffset*scaler;
			noffset=(noffset/10.24)+0.500;
			outstring;
			outstring.Format("%d",int(noffset));
			outstring=outstring+" \%";
			m_feedback_scaled_2.SetWindowText(outstring);
			oldpidcounter=pidcounter;
		}
	}
	if(nIDEvent==2 && currentcomport != -1)
	{
		KillTimer(2);
		CString outstring;
		bool change=false;
		int delta1=m_deadzone1_spin.GetPos32();
		if(delta1 != dead1)
		{
			dead1=delta1;
			change=true;
		}
		delta1=m_deadzone2_spin.GetPos32();
		if(delta1 != dead2)
		{
			dead2=delta1;
			change=true;
		}
		delta1=m_p1_spin.GetPos32();
		if(delta1 != p1)
		{
			p1=delta1/10.000;
			outstring.Format("%.1f",p1);
			m_p1_text.SetWindowText(outstring);
			change=true;
		}
		delta1=m_p2_spin.GetPos32();
		if(delta1 != p2)
		{
			p2=delta1/10.000;
			outstring.Format("%.1f",p2);
			m_p2_text.SetWindowText(outstring);
			change=true;
		}
		delta1=m_i1_spin.GetPos32();
		if(delta1 != i1)
		{
			i1=delta1/10.000;
			outstring.Format("%.1f",i1);
			m_i1_text.SetWindowText(outstring);
			change=true;
		}
		delta1=m_i2_spin.GetPos32();
		if(delta1 != i2)
		{
			i2=delta1/10.000;
			outstring.Format("%.1f",i2);
			m_i2_text.SetWindowText(outstring);
			change=true;
		}
		delta1=m_d1_spin.GetPos32();
		if(delta1 != d1)
		{
			d1=delta1/10.000;
			outstring.Format("%.1f",d1);
			m_d1_text.SetWindowText(outstring);
			change=true;
		}
		delta1=m_d2_spin.GetPos32();
		if(delta1 != d2)
		{
			d2=delta1/10.000;
			outstring.Format("%.1f",d2);
			m_d2_text.SetWindowText(outstring);
			change=true;
		}
		if(change==true && xpid_struct[currentcomport].comporthandle != INVALID_HANDLE_VALUE && xpid_struct[currentcomport].comporthandle != NULL)
		{
			WriteXPIDSettings(xpid_struct[currentcomport].comporthandle);
			RereadXPIDSettings(xpid_struct[currentcomport].comporthandle);
			xpid_struct[currentcomport].xpidsetting1.eeprom_deadzone=dead1;
			xpid_struct[currentcomport].xpidsetting2.eeprom_deadzone=dead2;
		}
	}
	if(nIDEvent==3)
	{
		byte debugbyte=0;
		int debuginteger=0;
		double debugdouble=0;
		//Show PID counter and the counts per second
		if(currentcomport != -1)
		{
			//Read PID Counter
			if(GetDebugValues(xpid_struct[currentcomport].comporthandle, &debugbyte, &debuginteger, &debugdouble)==TRUE)
			{
				CString outputstring;
				outputstring.Format("Byte: %d",int(debugbyte));
				m_debugbyte.SetWindowText(outputstring);
				outputstring.Format("Integer: %d",debuginteger);
				m_debuginteger.SetWindowText(outputstring);
				outputstring.Format("Double: %.1f",debugdouble);
				m_debugdouble.SetWindowText(outputstring);
			}
		}
	}
	CDialog::OnTimer(nIDEvent);
}

void CSettings::OnBnClickedRenameport()
{
	if(currentcomport == -1){return;}
	if(currentoutput  == -1){return;}
	if(currentoutput==0)
	{
		if(xpid_struct[currentcomport].xpidsetting1.renameport==false)
		{
			xpid_struct[currentcomport].xpidsetting1.renameport=true;
		}
		else
		{
			xpid_struct[currentcomport].xpidsetting1.renameport=false;
			MessageBox("You have to start a HW rescan in the converter program to take effect!", "Notice",0);
		}
	}
	if(currentoutput==1)
	{
		if(xpid_struct[currentcomport].xpidsetting2.renameport==false)
		{
			xpid_struct[currentcomport].xpidsetting2.renameport=true;
		}
		else
		{
			xpid_struct[currentcomport].xpidsetting2.renameport=false;
			MessageBox("You have to start a HW rescan in the converter program to take effect!", "Notice",0);
		}
	}
	SaveSingleXpidRegistry(currentcomport);
}

void CSettings::OnBnClickedCloseButton()
{
	globalshutdown=true;
	for(int z=0; z < 100; z++)
	{
		if(analysergraphrunning==false)
		{
			break;
		}
		Sleep(50);
	}
	KillTimer(1);
	KillTimer(2);
	KillTimer(3);
	//Check empty rename boxes
	if(currentcomport != -1)
	{
		int count=xpid_struct.size();
		for(int z=0; z < count; z++)
		{
			if(xpid_struct[z].xpidsetting1.renameport==true && xpid_struct[z].xpidsetting1.renamename=="")
			{
				xpid_struct[z].xpidsetting1.renameport=false;
				SaveSingleXpidRegistry(z);
			}
			if(xpid_struct[z].xpidsetting2.renameport==true && xpid_struct[z].xpidsetting2.renamename=="")
			{
				xpid_struct[z].xpidsetting2.renameport=false;
				SaveSingleXpidRegistry(z);
			}
		}
	}
	settingdialogopened=false;
	OnOK();
}

void CSettings::OnClose()
{
	globalshutdown=true;
	for(int z=0; z < 100; z++)
	{
		if(analysergraphrunning==false)
		{
			break;
		}
		Sleep(50);
	}
	KillTimer(1);
	KillTimer(2);
	KillTimer(3);
	//Check empty rename boxes
	if(currentcomport != -1)
	{
		int count=xpid_struct.size();
		for(int z=0; z < count; z++)
		{
				if(xpid_struct[z].xpidsetting1.renameport==true && xpid_struct[z].xpidsetting1.renamename=="")
				{
					xpid_struct[z].xpidsetting1.renameport=false;
					SaveSingleXpidRegistry(z);
				}
				if(xpid_struct[z].xpidsetting2.renameport==true && xpid_struct[z].xpidsetting2.renamename=="")
				{
					xpid_struct[z].xpidsetting2.renameport=false;
					SaveSingleXpidRegistry(z);
				}
		}
	}
	settingdialogopened=false;
	CDialog::OnClose();
}

void CSettings::SaveSingleXpidRegistry(int comportnumer)
{
	CString scn5key=regkey+"\\"+xpid_struct[comportnumer].xpidname;
	regMyReg.Open( scn5key, HKEY_LOCAL_MACHINE );
	regMyReg["renameXPID1"]=xpid_struct[comportnumer].xpidsetting1.renameport;
	regMyReg["renamename1"]=xpid_struct[comportnumer].xpidsetting1.renamename;
	regMyReg["renameXPID2"]=xpid_struct[comportnumer].xpidsetting2.renameport;
	regMyReg["renamename2"]=xpid_struct[comportnumer].xpidsetting2.renamename;
	regMyReg.Close();
}

void CSettings::PrintSettings()
{
	CString printstring;
	printstring.Format("%d",min1);
	m_minimum1_text.SetWindowText(printstring);
	printstring.Format("%d",max1);
	m_maximum1_text.SetWindowText(printstring);
	m_deadzone1_spin.SetPos32(dead1);
	m_p1_spin.SetPos32(p1*10.000);
	m_i1_spin.SetPos32(i1*10.000);
	m_d1_spin.SetPos32(d1*10.000);
	printstring.Format("%d",min2);
	m_minimum2_text.SetWindowText(printstring);
	printstring.Format("%d",max2);
	m_maximum2_text.SetWindowText(printstring);
	m_deadzone2_spin.SetPos32(dead2);
	m_p2_spin.SetPos32(p2*10.000);
	m_i2_spin.SetPos32(i2*10.000);
	m_d2_spin.SetPos32(d2*10.000);
	//Fill out the pid settings
	CString outstring;
	outstring.Format("%.1f",p1);
	m_p1_text.SetWindowText(outstring);
	outstring.Format("%.1f",i1);
	m_i1_text.SetWindowText(outstring);
	outstring.Format("%.1f",d1);
	m_d1_text.SetWindowText(outstring);
	outstring.Format("%.1f",p2);
	m_p2_text.SetWindowText(outstring);
	outstring.Format("%.1f",i2);
	m_i2_text.SetWindowText(outstring);
	outstring.Format("%.1f",d2);
	m_d2_text.SetWindowText(outstring);
}

void CSettings::OnCbnSelendokXpidCombo()
{
	int poss=m_xpidcombo.GetCurSel();
	if(poss >= 0)
	{
		CString debugmessage;
		currentcomport=xpid_to[poss].comport;
		m_comportdescription.SetWindowTextA(xpid_struct[currentcomport].friendlyname);
		m_comportname.SetWindowTextA(xpid_struct[currentcomport].comname);
		m_devicepath.SetWindowTextA(xpid_struct[currentcomport].devicepath);
		m_renamecomport.EnableWindow(TRUE);
		m_renametext.EnableWindow(TRUE);
		m_setnamebutton.EnableWindow(TRUE);
		//Set rename fields
		if(currentcomport*2 != poss)	//first position port
		{
			m_renamecomport.SetCheck(xpid_struct[currentcomport].xpidsetting1.renameport);
			m_renametext.SetWindowTextA(xpid_struct[currentcomport].xpidsetting1.renamename);
			currentoutput=0;
		}
		else							//second position port
		{
			m_renamecomport.SetCheck(xpid_struct[currentcomport].xpidsetting2.renameport);
			m_renametext.SetWindowTextA(xpid_struct[currentcomport].xpidsetting2.renamename);
			currentoutput=1;
		}
		//ReadWholeEEPRom(xpid_struct[currentcomport].comporthandle);  //Only for debug test
		GetXPIDSettings(xpid_struct[currentcomport].comporthandle);
		PrintSettings();
		cleardiagram();
	}
	else
	{
		currentcomport=-1;
		currentoutput=-1;
		m_renamecomport.EnableWindow(FALSE);
		m_renamecomport.SetCheck(FALSE);
		m_renametext.SetWindowTextA("");
		m_renametext.EnableWindow(FALSE);
		m_setnamebutton.EnableWindow(FALSE);
		m_comportdescription.SetWindowTextA("");
		m_comportname.SetWindowTextA("");
		m_devicepath.SetWindowTextA("");
		cleardiagram();
	}
}

void CSettings::OnBnClickedTimeoutCheck()
{
	enabletimeout=m_enable_timeout.GetCheck();
	regMyReg.Open( regkey, HKEY_LOCAL_MACHINE );
	regMyReg["enabletimeout"]=enabletimeout;
	regMyReg.Close();
}

void CSettings::OnEnChangeTimeoutEdit()
{
	CString inputstring;
	m_timeout_delay.GetWindowText(inputstring);
	int delay=atoi(inputstring);
	if(delay >= 1)
	{
		timeoutdelay=delay;
		regMyReg.Open( regkey, HKEY_LOCAL_MACHINE );
		regMyReg["timeoutdelay"]=delay;
		regMyReg.Close();
	}
}

void CSettings::OnBnClickedSetnameButton()
{
	if(currentcomport == -1){return;}
	if(currentoutput  == -1){return;}
	CString newname;
	m_renametext.GetWindowTextA(newname);
	if(newname.GetLength() > 28)
	{
		DWORD result=MessageBox("The name is too long. Do you like to shorten it to the correct size?","Input failure",MB_YESNO | MB_ICONQUESTION);
		if(result==IDNO){return;}
		newname=newname.Left(28);
		m_renametext.SetWindowText(newname);
		MessageBox("If this name is ok, press set again","Message",0);
		return;
	}
	if(currentoutput==0){xpid_struct[currentcomport].xpidsetting1.renamename=newname;}
	if(currentoutput==1){xpid_struct[currentcomport].xpidsetting2.renamename=newname;}
	SaveSingleXpidRegistry(currentcomport);
	MessageBox("You have to start a HW rescan in the converter program to take effect!", "Notice",0);
}



void CSettings::OnBnClickedSetButtonMinimum1()
{
	if(currentcomport == -1){return;}
	CString printstring;
	m_feedback1.GetWindowText(printstring);
	int stringval=atoi(printstring);
	if(stringval >= max1){stringval=max1-1;}
	if(stringval < 0){stringval=0;}
	min1=stringval;
	xpid_struct[currentcomport].xpidsetting1.eeprom_minimum=min1;
	printstring.Format("%d",min1);
	m_minimum1_text.SetWindowText(printstring);
	WriteXPIDSettings(xpid_struct[currentcomport].comporthandle);
	RereadXPIDSettings(xpid_struct[currentcomport].comporthandle);
}


void CSettings::OnBnClickedSetButtonMaximum1()
{
	if(currentcomport == -1){return;}
	CString printstring;
	m_feedback1.GetWindowText(printstring);
	int stringval=atoi(printstring);
	if(stringval <= min1){stringval=min1+1;}
	if(stringval > 1023){stringval=1023;}
	max1=stringval;
	xpid_struct[currentcomport].xpidsetting1.eeprom_maximum=max1;
	printstring.Format("%d",max1);
	m_maximum1_text.SetWindowText(printstring);
	WriteXPIDSettings(xpid_struct[currentcomport].comporthandle);
	RereadXPIDSettings(xpid_struct[currentcomport].comporthandle);
}


void CSettings::OnBnClickedSetButtonMinimum2()
{
	if(currentcomport == -1){return;}
	CString printstring;
	m_feedback2.GetWindowText(printstring);
	int stringval=atoi(printstring);
	if(stringval >= max2){stringval=max2-1;}
	if(stringval < 0){stringval=0;}
	min2=stringval;
	xpid_struct[currentcomport].xpidsetting2.eeprom_minimum=min2;
	printstring.Format("%d",min2);
	m_minimum2_text.SetWindowText(printstring);
	WriteXPIDSettings(xpid_struct[currentcomport].comporthandle);
	RereadXPIDSettings(xpid_struct[currentcomport].comporthandle);
}


void CSettings::OnBnClickedSetButtonMaximum2()
{
	if(currentcomport == -1){return;}
	CString printstring;
	m_feedback2.GetWindowText(printstring);
	int stringval=atoi(printstring);
	if(stringval <= min2){stringval=min2+1;}
	if(stringval > 1023){stringval=1023;}
	max2=stringval;
	xpid_struct[currentcomport].xpidsetting2.eeprom_maximum=max2;
	printstring.Format("%d",max2);
	m_maximum2_text.SetWindowText(printstring);
	WriteXPIDSettings(xpid_struct[currentcomport].comporthandle);
	RereadXPIDSettings(xpid_struct[currentcomport].comporthandle);
}


void CSettings::OnDeltaposDeadzone1Spin(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	*pResult = 0;
	SetTimer(2,50,NULL);
}


void CSettings::OnDeltaposDeadzone2Spin(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	*pResult = 0;
	SetTimer(2,50,NULL);
}


void CSettings::OnDeltaposP1Spin(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	*pResult = 0;
	SetTimer(2,50,NULL);
}


void CSettings::OnDeltaposI1Spin(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	*pResult = 0;
	SetTimer(2,50,NULL);
}


void CSettings::OnDeltaposD1Spin(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	*pResult = 0;
	SetTimer(2,50,NULL);
}


void CSettings::OnDeltaposP2Spin(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	*pResult = 0;
	SetTimer(2,50,NULL);
}


void CSettings::OnDeltaposI2Spin(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	*pResult = 0;
	SetTimer(2,50,NULL);
}


void CSettings::OnDeltaposD2Spin(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	*pResult = 0;
	SetTimer(2,50,NULL);
}


void CSettings::OnBnClickedReset1()
{
	if(currentcomport == -1){return;}
	min1=2;
	max1=1021;
	dead1=0;
	xpid_struct[currentcomport].xpidsetting1.eeprom_minimum=min1;
	xpid_struct[currentcomport].xpidsetting1.eeprom_maximum=max1;
	m_minimum1_text.SetWindowText("2");
	m_maximum1_text.SetWindowText("1021");
	xpid_struct[currentcomport].xpidsetting1.eeprom_deadzone=0;
	m_deadzone1_spin.SetPos32(0);
	WriteXPIDSettings(xpid_struct[currentcomport].comporthandle);
	RereadXPIDSettings(xpid_struct[currentcomport].comporthandle);
}


void CSettings::OnBnClickedReset2()
{
	if(currentcomport == -1){return;}
	min2=2;
	max2=1021;
	dead2=0;
	xpid_struct[currentcomport].xpidsetting2.eeprom_minimum=min1;
	xpid_struct[currentcomport].xpidsetting2.eeprom_maximum=max1;
	m_minimum2_text.SetWindowText("2");
	m_maximum2_text.SetWindowText("1021");
	xpid_struct[currentcomport].xpidsetting2.eeprom_deadzone=0;
	m_deadzone2_spin.SetPos32(0);
	WriteXPIDSettings(xpid_struct[currentcomport].comporthandle);
	RereadXPIDSettings(xpid_struct[currentcomport].comporthandle);
}


void CSettings::OnBnClickedEnableDebug()
{
	SetTimer(3,100,0);
	m_debugbyte.ShowWindow(SW_SHOW);
	m_debuginteger.ShowWindow(SW_SHOW);
	m_debugdouble.ShowWindow(SW_SHOW);
}


void CSettings::OnBnClickedGraphCheck()
{
	BOOL check=m_graph.GetCheck();
	if(check==TRUE)
	{
		SetTimer(1,200,NULL);
	}
	else
	{
		SetTimer(1,1000,NULL);
	}
}
