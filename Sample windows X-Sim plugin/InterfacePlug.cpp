//
//
//
//		Plugin DLL for X-SIm Converter application
//		Interface: Arduino XPID USB Interface
//		www.x-sim.de
//		Copyright (c) 2013 Martin Wiedenbauer, hobbydev
//
//

#include "stdafx.h"
#include "InterfacePlug.h"
#include "Settings.h"
#include "resource1.h"
#include "EnumSerial.h"
#include "registry.h"
#include "enumser.h"
#include <vector>

using namespace std;
CString regkey="Software\\X-Sim\\Force-Profiler2\\Interface-Plugins\\XPID";

#pragma data_seg ("XPIDSerialShared")

bool ReadThreadRunning	= false;
int DeviceDetected		= 0;
IOsharing iosharing		= {0};
bool xpidresearching	= false;
bool shutdownthread		= false;
bool shutdowndialog		= false;
bool WasInitDll			= false;
bool threadstatus		= false;
vector <XPIDSTRUCT> xpid_struct;
vector <XPIDTO> xpid_to;
CWinThread*	dialogthread;
CWinThread*	powerthread;
bool settingdialogopened= false;
CRegistry regMyReg;
CRITICAL_SECTION CriticalSection;
LARGE_INTEGER SP_PC_Stopp;
LARGE_INTEGER SP_PC_Start;
LARGE_INTEGER PC_Freq;
CUIntArray ports;
CStringArray friendlyNames;
int timeoutdelay=30;
int poweroffcount=timeoutdelay*10;
BOOL enabletimeout=TRUE;
#pragma data_seg()
#pragma comment(linker,"/SECTION:XPIDSerialShared,RWS")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define DLLEXPORT extern "C" __declspec (dllexport)

DLLEXPORT DllInfo GetType(void);
DLLEXPORT IOsharing GetInterfaceIO(void);
DLLEXPORT INPUTSTRUCT GetInput(bool inputs[255]);
DLLEXPORT int SetOutput(int number, unsigned int wert);
DLLEXPORT int IsConnected(void);
DLLEXPORT int SetVirtualOutput(int number, unsigned int wert);
DLLEXPORT int ExecuteVirtual(void);
DLLEXPORT void EnableThread(bool enable);
DLLEXPORT void InitDLL(void);
DLLEXPORT void OpenSettingsDialog(void);

HANDLE openPort(CString portName, unsigned int baudRate)
{
	HANDLE port;
	DCB commState;
	BOOL success;
	COMMTIMEOUTS timeouts;
	/* Open the serial port. */
	port = CreateFile(portName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (port == INVALID_HANDLE_VALUE)
	{
		switch(GetLastError())
		{
		case ERROR_ACCESS_DENIED:	
			break;
		case ERROR_FILE_NOT_FOUND:
			break;
		default:
			break;
		}
		return INVALID_HANDLE_VALUE;
	}
	/* Set the timeouts. */
	success = GetCommTimeouts(port, &timeouts);
	if (!success)
	{
		CloseHandle(port);
		return INVALID_HANDLE_VALUE;
	}
	timeouts.ReadIntervalTimeout = 10;
	timeouts.ReadTotalTimeoutConstant = 10;
	timeouts.ReadTotalTimeoutMultiplier = 0;
	timeouts.WriteTotalTimeoutConstant = 10;
	timeouts.WriteTotalTimeoutMultiplier = 0;
	success = SetCommTimeouts(port, &timeouts);
	if (!success)
	{
		CloseHandle(port);
		return INVALID_HANDLE_VALUE;
	}
	/* Set the baud rate. */
	success = GetCommState(port, &commState);
	if (!success)
	{
		CloseHandle(port);
		return INVALID_HANDLE_VALUE;
	}
	commState.BaudRate = baudRate;
	commState.fBinary = 1;
	commState.fDtrControl = DTR_CONTROL_DISABLE;
	commState.fRtsControl = RTS_CONTROL_DISABLE;
	commState.ByteSize = 8;
	commState.Parity = NOPARITY;
	commState.StopBits = ONESTOPBIT;
	success = SetCommState(port, &commState);
	if (!success)
	{
		CloseHandle(port);
		return INVALID_HANDLE_VALUE;
	}

	/* Flush out any bytes received from the device earlier. */
	success = FlushFileBuffers(port);
	if (!success)
	{
		CloseHandle(port);
		return INVALID_HANDLE_VALUE;
	}
	return port;
}

// Sets the XPID target variable (0-1023).
BOOL xpidSetTarget(HANDLE port, int portnumber ,int target)
{
	unsigned char command[4];
	int high,low;
	high=target/256;
	low=target-(high*256);
	DWORD bytesTransferred;
	BOOL success;
	// Compose the command.
	command[0] = 'X';
	command[1] = portnumber+1;
	command[2] = high;
	command[3] = low;
	// Send the command to the device.
	success = WriteFile(port, command, 4, &bytesTransferred, NULL);
	if (!success){return FALSE;}
	if (bytesTransferred != 4){return FALSE;}
	return TRUE;
}

// all XPID power off
BOOL xpidSetPowerOff()
{
	int numberofcomports=xpid_struct.size();
	for(int z=0; z < numberofcomports; z++)
	{
		if(xpid_struct[z].xpidpresent==true)
		{
			unsigned char command[4];
			DWORD bytesTransferred;
			BOOL success;
			// Compose the command for motor 1
			command[0] = 'X';
			command[1] = 207;
			command[2] = 0;
			command[3] = 0;
			// Send the command to the device.
			success = WriteFile(xpid_struct[z].comporthandle, command, 4, &bytesTransferred, NULL);
			if (!success || bytesTransferred != 4)
			{
				xpid_struct[z].xpidpresent=false; 
				CloseHandle(xpid_struct[z].comporthandle); 
				xpid_struct[z].comporthandle=NULL;
				return FALSE;
			}
			// Compose the command for motor 2
			command[0] = 'X';
			command[1] = 208;
			command[2] = 0;
			command[3] = 0;
			// Send the command to the device.
			success = WriteFile(xpid_struct[z].comporthandle, command, 4, &bytesTransferred, NULL);
			if (!success || bytesTransferred != 4)
			{
				xpid_struct[z].xpidpresent=false; 
				CloseHandle(xpid_struct[z].comporthandle); 
				xpid_struct[z].comporthandle=NULL;
				return FALSE;
			}
		}
	}
	return TRUE;
}

// all XPID power on
BOOL xpidSetPowerOn()
{
	int numberofcomports=xpid_struct.size();
	for(int z=0; z < numberofcomports; z++)
	{
		if(xpid_struct[z].xpidpresent==true)
		{
			unsigned char command[4];
			DWORD bytesTransferred;
			BOOL success;
			// Compose the command for motor 1
			command[0] = 'X';
			command[1] = 209;
			command[2] = 0;
			command[3] = 0;
			// Send the command to the device.
			success = WriteFile(xpid_struct[z].comporthandle, command, 4, &bytesTransferred, NULL);
			if (!success || bytesTransferred != 4)
			{
				xpid_struct[z].xpidpresent=false; 
				CloseHandle(xpid_struct[z].comporthandle); 
				xpid_struct[z].comporthandle=NULL;
				return FALSE;
			}
			// Compose the command for motor 2
			command[0] = 'X';
			command[1] = 210;
			command[2] = 0;
			command[3] = 0;
			// Send the command to the device.
			success = WriteFile(xpid_struct[z].comporthandle, command, 4, &bytesTransferred, NULL);
			if (!success || bytesTransferred != 4)
			{
				xpid_struct[z].xpidpresent=false; 
				CloseHandle(xpid_struct[z].comporthandle); 
				xpid_struct[z].comporthandle=NULL;
				return FALSE;
			}
		}
	}
	return TRUE;
}

CString GetFirmwareVersion(HANDLE comhandle)
{
	CString firmware="";
	BOOL success;
	COMMTIMEOUTS timeouts;
	/* Set the timeouts to 100ms */
	success = GetCommTimeouts(comhandle, &timeouts);
	if (!success)
	{
		CloseHandle(comhandle);
		return "";
	}
	timeouts.ReadIntervalTimeout = 100;
	timeouts.ReadTotalTimeoutConstant = 100;
	timeouts.ReadTotalTimeoutMultiplier = 0;
	timeouts.WriteTotalTimeoutConstant = 10;
	timeouts.WriteTotalTimeoutMultiplier = 0;
	success = SetCommTimeouts(comhandle, &timeouts);
	if (!success)
	{
		CloseHandle(comhandle);
		return "";
	}
	// Ask the firmware on comport if it is a Arduino with XPID firmware
	char response[9];
	char command[4]={'X',202,0,0};
	DWORD bytesTransferred;
	// Send the command to the device.
	success = WriteFile(comhandle, &command, 4, &bytesTransferred, NULL);
	if (!success || bytesTransferred != 4){return "";}
	// Read the response from the device.
	success = ReadFile(comhandle, response, 9, &bytesTransferred, NULL);
	if (!success || bytesTransferred != 9){return "";}
	for(int z=0; z < 9; z++)
	{
		firmware=firmware+response[z];
	}
	/* Set the timeouts back */
	success = GetCommTimeouts(comhandle, &timeouts);
	if (!success)
	{
		CloseHandle(comhandle);
		return "";
	}
	timeouts.ReadIntervalTimeout = 10;
	timeouts.ReadTotalTimeoutConstant = 10;
	timeouts.ReadTotalTimeoutMultiplier = 0;
	timeouts.WriteTotalTimeoutConstant = 10;
	timeouts.WriteTotalTimeoutMultiplier = 0;
	success = SetCommTimeouts(comhandle, &timeouts);
	if (!success)
	{
		CloseHandle(comhandle);
		return "";
	}
	return firmware;
}

int ReadXPIDEEPRomValue(HANDLE comhandle, int mem_address)
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

int ReadXPIDEEPRomWord(HANDLE comhandle, int mem_address)
{
	if(comhandle == NULL || comhandle == INVALID_HANDLE_VALUE){return -1;}
	int low,high, returnvalue;
	high=ReadXPIDEEPRomValue(comhandle, mem_address);
	low=ReadXPIDEEPRomValue(comhandle, mem_address+1);
	returnvalue=(high*256)+low;
	return returnvalue;
}

void ReadXPIDSettings(HANDLE comhandle, XPIDSTRUCT *settings)
{
	if(comhandle == NULL || comhandle == INVALID_HANDLE_VALUE){return;}
	settings->xpidsetting1.eeprom_minimum=ReadXPIDEEPRomWord(comhandle,1);
	settings->xpidsetting1.eeprom_maximum=ReadXPIDEEPRomWord(comhandle,3);
	settings->xpidsetting1.eeprom_deadzone=ReadXPIDEEPRomValue(comhandle,5);
	settings->xpidsetting2.eeprom_minimum=ReadXPIDEEPRomWord(comhandle,6);
	settings->xpidsetting2.eeprom_maximum=ReadXPIDEEPRomWord(comhandle,8);
	settings->xpidsetting2.eeprom_deadzone=ReadXPIDEEPRomValue(comhandle,10);
	return;
}

void ScanExistingRegistryEntry(XPIDSTRUCT *scanstruct)
{
	CString xpidkey=regkey+"\\"+scanstruct->xpidname;
	if(regMyReg.KeyExists(xpidkey, HKEY_LOCAL_MACHINE))
	{
		//SCN5 was here before, load last settings
		regMyReg.Open( xpidkey, HKEY_LOCAL_MACHINE );
		scanstruct->xpidsetting1.renameport=regMyReg["renameXPID1"];
		scanstruct->xpidsetting1.renamename=regMyReg["renamename1"];
		scanstruct->xpidsetting2.renameport=regMyReg["renameXPID2"];
		scanstruct->xpidsetting2.renamename=regMyReg["renamename2"];
		regMyReg.Close();
	}
	else
	{
		regMyReg.Open( xpidkey, HKEY_LOCAL_MACHINE );
		//Standard settings
		scanstruct->xpidsetting1.renameport=false;
		scanstruct->xpidsetting1.renamename="";
		scanstruct->xpidsetting2.renameport=false;
		scanstruct->xpidsetting2.renamename="";
		regMyReg.Open( xpidkey, HKEY_LOCAL_MACHINE );
		regMyReg["renameXPID1"]=scanstruct->xpidsetting1.renameport;
		regMyReg["renamename1"]=scanstruct->xpidsetting1.renamename;
		regMyReg["renameXPID2"]=scanstruct->xpidsetting2.renameport;
		regMyReg["renamename2"]=scanstruct->xpidsetting2.renamename;
		CString regvalue;
		regMyReg.Close();
	}
}

UINT DialogThreadFunktion(LPVOID param)
{
	CSettings dialog;
	dialog.DoModal();
	settingdialogopened=false;
	return 0;
}

int ScalePosition(int comportnumber, int portnumber, unsigned int integervalue)
{
	//to 0 to 1023 of XPID analogue pot resolution
	integervalue=integervalue/ADRESOLUTIONDEVIDER;
	//Get the fitting EEProm values
	int minimum, maximum;
	if(portnumber==0)
	{
		minimum=xpid_struct[comportnumber].xpidsetting1.eeprom_minimum;
		maximum=xpid_struct[comportnumber].xpidsetting1.eeprom_maximum;
	}
	else
	{
		minimum=xpid_struct[comportnumber].xpidsetting2.eeprom_minimum;
		maximum=xpid_struct[comportnumber].xpidsetting2.eeprom_maximum;
	}
	//Scale to minimum and maximum value
	double scaler=1023.000/(double(maximum)-double(minimum));
	double resultval=double(integervalue)/scaler;
	resultval+=double(minimum);
	int resultinteger=int(resultval);
	if(resultinteger > maximum){resultinteger=maximum;}
	if(resultinteger < minimum){resultinteger=minimum;}
	return resultinteger;	
}

BOOL MoveActuator(int comportnumber, int portnumber, unsigned int integervalue)
{
	if(xpid_struct[comportnumber].xpidpresent==false){return FALSE;}
	int newpos=ScalePosition(comportnumber, portnumber, integervalue);
	EnterCriticalSection(&CriticalSection);
	BOOL success=xpidSetTarget(xpid_struct[comportnumber].comporthandle, portnumber, newpos);
	if(success==FALSE)
	{
		xpid_struct[comportnumber].xpidpresent=false;
		CloseHandle(xpid_struct[comportnumber].comporthandle);
		xpid_struct[comportnumber].comporthandle=INVALID_HANDLE_VALUE;
	}
	LeaveCriticalSection(&CriticalSection);
	return success;
}

DllInfo GetType(void)
{
	//X-Sim dll verification
	DllInfo dllinfo;
	char* type = ( char* ) malloc( sizeof( char ) *11 );
	type="Interface ";
	for(int z=0; z < 10; z++){dllinfo.type[z]=type[z];}
	dllinfo.type[10] = 0;
	char* name = ( char* ) malloc( sizeof( char ) *11 );
	name="USB XPID  ";
	for(int z=0; z < 10; z++){dllinfo.name[z]=name[z];}
	dllinfo.name[10]= 0;
	return dllinfo;
}

IOsharing GetInterfaceIO(void)
{
	return iosharing;
}

INPUTSTRUCT GetInput(bool inputs[255])
{
	INPUTSTRUCT sendinput={0};
	return sendinput;
}

int SetOutput(int number, unsigned int inputwert)
{
	poweroffcount=timeoutdelay*10;
	if(number >= xpid_to.size()){return DeviceDetected;}
	if(xpid_struct[xpid_to[number].comport].xpidpresent==true)
	{
		BOOL success=MoveActuator(xpid_to[number].comport, xpid_to[number].portnumber, inputwert);
		if(success==FALSE){xpid_struct[xpid_to[number].comport].xpidpresent=false;}
		xpid_to[number].virtualvalue=inputwert;
		xpid_to[number].oldvirtualvalue=xpid_to[number].virtualvalue;
	}
	return DeviceDetected;
}

BOOL Checkdoublenames(CString name, int ignorecomport)
{
	BOOL found=FALSE;
	int numberofcomports=xpid_struct.size();
	int count=0;
	for(int z=0; z < numberofcomports; z++)
	{
		if(xpid_struct[z].xpidpresent==true)
		{
			if(xpid_struct[z].xpidsetting1.renamename == name && ignorecomport != z)
			{
				found=TRUE;
			}
		}
	}
	return found;
}

UINT PowerThread(LPVOID param)
{
	threadstatus	= true;
	while(shutdownthread==false)
	{
		poweroffcount--;
		if(poweroffcount <= 0 && enabletimeout==TRUE)
		{
			if(poweroffcount==0){xpidSetPowerOff();}
			poweroffcount=-1;
		}
		Sleep(100);
	}
	threadstatus	= false;
	return 0;
}

void SetIOsharing()
{
	xpid_to.clear();
	iosharing.numberofinputs=0;
	int numberofcomports=xpid_struct.size();
	for(int z=0; z < numberofcomports; z=z+2)
	{
		for(int y=0; y < 2; y++)
		{
			CString motorstring;
			motorstring.Format(" position %d",y+1);
			if(xpid_struct[z].xpidpresent==true)
			{
				iosharing.nameofoutputs[(z*2)+y] = (char *)malloc(sizeof(char)*31);
				iosharing.sizeofoutputsname[(z*2)+y] = 31;
				CString thename, thenameacc, thenamespeed;
				if(y==0)
				{
					if(xpid_struct[z].xpidsetting1.renameport==false || xpid_struct[z].xpidsetting1.renamename == "" || Checkdoublenames(xpid_struct[z].xpidsetting1.renamename,z)==TRUE)
					{
						thename=xpid_struct[z].xpidname+motorstring;
					}
					else
					{
						thename=xpid_struct[z].xpidsetting1.renamename;
					}
					thename=thename+"                                                        ";
					for(int s=0; s < 30; s++)
					{
						iosharing.nameofoutputs[(z*2)+y][s]=thename[s];
					}
				}
				else
				{
					if(xpid_struct[z].xpidsetting2.renameport==false || xpid_struct[z].xpidsetting2.renamename == "" || Checkdoublenames(xpid_struct[z].xpidsetting2.renamename,z)==TRUE)
					{
						thename=xpid_struct[z].xpidname+motorstring;
					}
					else
					{
						thename=xpid_struct[z].xpidsetting2.renamename;
					}
					thename=thename+"                                                        ";
					for(int s=0; s < 30; s++)
					{
						iosharing.nameofoutputs[(z*2)+y][s]=thename[s];
					}
				}
				iosharing.nameofoutputs[(z*2)+y][30]=0;
				XPIDTO newto={0};
				newto.comport=z;
				newto.portnumber=y;
				newto.virtualvalue=INT_MAX;
				newto.oldvirtualvalue=INT_MAX;
				xpid_to.push_back(newto);
			}
		}
	}
	iosharing.numberofoutputs=numberofcomports*2;
}

void ClearIOsharing()
{
	xpid_to.clear();
	iosharing.numberofinputs=0;
	iosharing.numberofoutputs=0;
}

int IsConnected(void)
{
	return DeviceDetected;
}

int SetVirtualOutput(int number, unsigned int inputwert)
{
	if(number >= xpid_to.size()){return DeviceDetected;}
	xpid_to[number].virtualvalue=inputwert;
	return DeviceDetected;
}

int ExecuteVirtual(void)
{
	poweroffcount=timeoutdelay*10;
	//Move all actuators
	int numberofoutputs=xpid_to.size();
	int count=0;
	for(int z=0; z < numberofoutputs; z++)
	{
		if(xpid_struct[xpid_to[z].comport].xpidpresent==true)
		{
			//All set, then set new position if needed
			if(xpid_to[z].virtualvalue != xpid_to[z].oldvirtualvalue)
			{
				BOOL success=MoveActuator(xpid_to[z].comport, xpid_to[z].portnumber, xpid_to[z].virtualvalue);
				xpid_to[z].oldvirtualvalue=xpid_to[z].virtualvalue;
				if(success==FALSE){DeviceDetected=0;}
			}
		}
	}
	return DeviceDetected;
}

void ResearchDevices(void)
{
	QueryPerformanceFrequency(&PC_Freq);
	xpidresearching	= true;
	DeviceDetected=0;
	xpid_struct.clear();
	//Scan all xpid interfaces on every available comport
	CArray<SSerInfo,SSerInfo&> asi;
	asi.RemoveAll();
	if (CEnumerateSerial::UsingSetupAPI2(ports, friendlyNames))
	{
		//Convert to asi as best as possible
		for (int i=0; i < ports.GetSize(); i++)
		{
			// Add an entry to the array
			CString pathstr;
			SSerInfo si;
			pathstr.Format("COM%d",ports[i]);
			si.strPortName = pathstr;
			pathstr.Format("\\\\.\\COM%d",ports[i]);
			si.strDevPath = pathstr;
			si.strFriendlyName = friendlyNames[i];
			si.strPortDesc = "";
			si.bUsbDevice = false;
			asi.Add(si);
		}
	}
	else
	{
		// Populate the list of serial ports.
		EnumSerialPorts(asi,FALSE/*include all*/);
	}

	if(asi.GetSize() > 0)
	{
		for (int ii=0; ii < asi.GetSize(); ii++) 
		{
			XPIDSTRUCT newxpidstruct={0};
			//Get short comport suffix like "com1:"
			newxpidstruct.comname = asi[ii].strPortName;
			newxpidstruct.devicepath = asi[ii].strDevPath;
			newxpidstruct.friendlyname = asi[ii].strFriendlyName;
			//Look if a SCN5 is present	on this comport
			//Open comport and set the speed
			HANDLE comhandle=openPort(newxpidstruct.devicepath,CBR_115200);
			if(comhandle)
			{
				CString firmware=GetFirmwareVersion(comhandle);
				//if(firmware==""){Sleep(100); firmware=GetFirmwareVersion(comhandle);}
				if(firmware != "" && firmware[0]=='X' && firmware[1]=='-' && firmware[2]=='P' && firmware[3]=='I' && firmware[4]=='D' && firmware[5]==' ')
				{
					//Found a xpid on comport, now check if it is working
					newxpidstruct.comporthandle=comhandle;
					newxpidstruct.xpidname=firmware+" "+newxpidstruct.comname;
					newxpidstruct.friendlyname=asi[ii].strFriendlyName;
					newxpidstruct.devicepath=asi[ii].strDevPath;
					newxpidstruct.comname=asi[ii].strPortName;
					newxpidstruct.xpidsetting1.renamename="";
					newxpidstruct.xpidsetting1.renameport=false;
					newxpidstruct.xpidpresent=true;
					//Read out the eeprom
					ReadXPIDSettings(comhandle,&newxpidstruct);
					//Check if this XPID has to be renamed
					ScanExistingRegistryEntry(&newxpidstruct);
					xpid_struct.push_back(newxpidstruct);
					DeviceDetected++;
				}
				else
				{
					CloseHandle(comhandle);
				}
			}
		}
	}
	if (DeviceDetected > 0)
	{
		SetIOsharing();
	}
	else
	{
		ClearIOsharing();
	}
	xpidresearching	=false;
}

void ZeroOutput(void)
{
	poweroffcount=timeoutdelay*10;
	BOOL success=TRUE;
	bool startthread=false;
	byte command[16]={0};
	//Send neutral position to all xpid interfaces
	int numberofcomports=xpid_struct.size();
	for(int z=0; z < numberofcomports; z++)
	{
		if(xpid_struct[z].xpidpresent==true && xpid_struct[z].comporthandle != NULL && xpid_struct[z].comporthandle != INVALID_HANDLE_VALUE)
		{
			//Motor 1
			int middlepos=((xpid_struct[z].xpidsetting1.eeprom_maximum-xpid_struct[z].xpidsetting1.eeprom_minimum)/2)+xpid_struct[z].xpidsetting1.eeprom_minimum;
			xpidSetTarget(xpid_struct[z].comporthandle, 0, middlepos);
			//Motor 2
			middlepos=((xpid_struct[z].xpidsetting2.eeprom_maximum-xpid_struct[z].xpidsetting2.eeprom_minimum)/2)+xpid_struct[z].xpidsetting2.eeprom_minimum;
			xpidSetTarget(xpid_struct[z].comporthandle, 1, middlepos);
		}
	}
}

void FreeHandles(void)
{
	xpidSetPowerOff();
	//Close all xpid comports
	for(int z=0; z < xpid_struct.size(); z++)
	{
		if(xpid_struct[z].comporthandle != INVALID_HANDLE_VALUE && xpid_struct[z].comporthandle != NULL){CloseHandle(xpid_struct[z].comporthandle);}
		xpid_struct[z].comporthandle=INVALID_HANDLE_VALUE;
	}
	xpid_struct.clear();
}

void EnableThread(bool enable)
{
	if(enable==true)
	{
		shutdownthread=false;
		if (threadstatus==false)
		{
			poweroffcount=timeoutdelay;
			powerthread=AfxBeginThread(PowerThread,NULL,THREAD_PRIORITY_NORMAL);
		}
	}
	else
	{
		shutdowndialog=true;
		shutdownthread=true;
		for(int z=0; z < 100; z++)
		{
			if(settingdialogopened==true)
			{
				Sleep(100);
			}
			else
			{
				break;
			}
		}
		if(settingdialogopened==true)
		{
			dialogthread->ExitInstance();
		}
		for(int z=0; z < 20; z++)
		{
			if(threadstatus==true)
			{
				Sleep(100);
			}
			else
			{
				break;
			}
		}
	}
}

void InitDLL(void)
{
	WasInitDll=true;
	//Find the XPID on any comport
	ResearchDevices();
	//Drive them to middle position
	ZeroOutput();
}

void OpenSettingsDialog(void)
{
	if(settingdialogopened==false)
	{
		settingdialogopened=true;
		dialogthread=AfxBeginThread(DialogThreadFunktion,NULL,THREAD_PRIORITY_NORMAL);
	}
}

BEGIN_MESSAGE_MAP(CInterfacePluginV2App, CWinApp)
END_MESSAGE_MAP()

CInterfacePluginV2App::CInterfacePluginV2App()
{
	
}

CInterfacePluginV2App theApp;

BOOL CInterfacePluginV2App::InitInstance()
{
	CWinApp::InitInstance();
	shutdowndialog=false;
	//Get global timeout settings out of registry
	if(regMyReg.KeyExists(regkey,HKEY_LOCAL_MACHINE))
	{
		regMyReg.Open( regkey, HKEY_LOCAL_MACHINE );
		if(regMyReg["enableTimeout"].Exists())
		{
			enabletimeout=regMyReg["enabletimeout"];
			timeoutdelay=regMyReg["timeoutdelay"];
		}
		else
		{
			enabletimeout=TRUE;
			timeoutdelay=30;
			regMyReg["enabletimeout"]=enabletimeout;
			regMyReg["timeoutdelay"]=timeoutdelay;
		}
		regMyReg.Close();
	}
	else
	{
		regMyReg.Open( regkey, HKEY_LOCAL_MACHINE );
		enabletimeout=TRUE;
		timeoutdelay=30;
		regMyReg["enabletimeout"]=enabletimeout;
		regMyReg["timeoutdelay"]=timeoutdelay;
		regMyReg.Close();
	}
	InitializeCriticalSectionAndSpinCount(&CriticalSection,0x80000400);
	return TRUE;
}	
int CInterfacePluginV2App::ExitInstance()
{
	if(WasInitDll==true){FreeHandles();}
	DeleteCriticalSection(&CriticalSection);
	CWinApp::ExitInstance();
	return TRUE;
}
