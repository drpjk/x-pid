/*
	X-Sim PID
	This program will control two motor H-Bridge with analogue feedback and serial target input value
	Target is a Arduino UNO R3 but should work on all Arduino with Atmel 328
	Copyright (c) 2013 Martin Wiedenbauer, particial use is only allowed with a reference link to the x-sim.de project

	Command input protocol	(always 4 bytes, beginning with 'X' character)
	'X' 1 H L				Set motor 1 position to High and Low value 0 to 1023
	'X' 2 H L				Set motor 2 position to High and Low value 0 to 1023
	'X' 3 H L				Set motor 1 P Proportional value to High and Low value
	'X' 4 H L				Set motor 2 P Proportional value to High and Low value
	'X' 5 H L				Set motor 1 I Integral value to High and Low value
	'X' 6 H L				Set motor 2 I Integral value to High and Low value
	'X' 7 H L				Set motor 1 D Derivative value to High and Low value
	'X' 8 H L				Set motor 2 D Derivative value to High and Low value

	'X' 200 0 0				Send back over serial port both analogue feedback raw values
	'X' 201 0 0				Send back over serial port the current pid count
	'X' 202 0 0				Send back over serial port the firmware version (used for x-sim autodetection)
	'X' 203 M V				Write EEPROM on address M (only 0 to 255 of 1024 Bytes of the EEPROM) with new value V
	'X' 204 M 0				Read EEPROM on memory address M (only 0 to 255 of 1024 Bytes of the EEPROM), send back over serial the value
	'X' 205 0 0				Clear EEPROM
	'X' 206 0 0				Reread the whole EEPRom and store settings into fitting variables
	'X' 207 0 0				Disable power on motor 1
	'X' 208 0 0				Disable power on motor 2
	
	EEPROM memory map
	00		empty eeprom detection, 111 if set, all other are indicator to set default
	01-02	minimum 1
	03-04	maximum 1
	05		dead zone 1
	06-07	minimum 2
	08-09	maximum 2
	10		dead zone 2
	11-12	P component of motor 1
	13-14	I component of motor 1
	15-16	D component of motor 1
	17-18	P component of motor 2
	19-20	I component of motor 2
	21-22	D component of motor 2


	Pin out of arduino for H-Bridge

	Pin 10 - PWM1 - Speed for Motor 1. 
	Pin  9 - PWM2 - Speed for Motor 2.
	Pin  2 - INA1 - motor 1 turn 
	Pin  3 - INB1 - motor 1 turn
	Pin  4 - INB1 - motor 2 turn
	Pin  5 - INB2 - motor 2 turn

	Analog Pins

	Pin A0 - input of feedback positioning from motor 1
	Pin A1 - input of feedback positioning from motor 2

	As well 5v and GND pins tapped in to feed feedback pots too.

*/

#include <EEPROM.h>

//Some speed test switches for testers ;)
#define FASTADC  1 //Hack to speed up the arduino analogue read function, comment out with // to disable this hack
#define FASTLOOP 1 //Hack to speed up the arduino framework loop delay, set to 0 to disable this feature

// defines for setting and clearing register bits
#ifndef cbi
	#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
	#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

#define LOWBYTE(v)   ((unsigned char) (v))								//Read
#define HIGHBYTE(v)  ((unsigned char) (((unsigned int) (v)) >> 8))
#define BYTELOW(v)   (*(((unsigned char *) (&v) + 1)))					//Write
#define BYTEHIGH(v)  (*((unsigned char *) (&v)))

#define   GUARD_MOTOR_1_GAIN   100.0     
#define   GUARD_MOTOR_2_GAIN   100.0

//Firmware version info
int firmaware_version_mayor=1;
int firmware_version_minor =0;

int currentanalogue1 = 0;
int currentanalogue2 = 0;
int target1=512;
int target2=512;
int low=0;
int high=0;
unsigned long hhigh=0;
unsigned long hlow=0;
unsigned long lhigh=0;
unsigned long llow=0;
int buffer=0;
int buffercount=-1;
int commandbuffer[4]={0};
unsigned long pidcount	= 0;		// unsigned 32bit, 0 to 4,294,967,295

// fixed DATA for direct port manipulation, exchange here each value if your h-Bridge is connected to another port pin
// This pinning overview is to avoid the slow pin switching of the arduino libraries
// 
//                  +-\/-+
//            PC6  1|    |28  PC5 (AI 5)
//      (D 0) PD0  2|    |27  PC4 (AI 4)
//      (D 1) PD1  3|    |26  PC3 (AI 3)
//      (D 2) PD2  4|    |25  PC2 (AI 2)
// PWM+ (D 3) PD3  5|    |24  PC1 (AI 1)
//      (D 4) PD4  6|    |23  PC0 (AI 0)
//            VCC  7|    |22  GND
//            GND  8|    |21  AREF
//            PB6  9|    |20  AVCC
//            PB7 10|    |19  PB5 (D 13)
// PWM+ (D 5) PD5 11|    |18  PB4 (D 12)
// PWM+ (D 6) PD6 12|    |17  PB3 (D 11) PWM
//      (D 7) PD7 13|    |16  PB2 (D 10) PWM
//      (D 8) PB0 14|    |15  PB1 (D 9) PWM
//                  +----+
// 
int portdstatus				=PORTD; 	// read the current port D bit mask
int ControlPinM1Inp1		=2;			// motor 1 INP1 output, this is the arduino pin description
int ControlPinM1Inp2		=3;			// motor 1 INP2 output, this is the arduino pin description
int ControlPinM2Inp1		=4;			// motor 2 INP1 output, this is the arduino pin description
int ControlPinM2Inp2		=5;			// motor 2 INP2 output, this is the arduino pin description
int PWMPinM1				=10;		// motor 1 PWM output
int PWMPinM2				=9;			// motor 2 PWM output

// Pot feedback inputs
int FeedbackPin1			= A0;		// select the input pin for the potentiometer 1, PC0
int FeedbackPin2			= A1;		// select the input pin for the potentiometer 2, PC1
int FeedbackMax1			= 1021;		// Maximum position of pot 1 to scale, do not use 1023 because it cannot control outside the pot range
int FeedbackMin1			= 2;		// Minimum position of pot 1 to scale, do not use 0 because it cannot control outside the pot range
int FeedbackMax2			= 1021;		// Maximum position of pot 2 to scale, do not use 1023 because it cannot control outside the pot range
int FeedbackMin2			= 2;		// Minimum position of pot 2 to scale, do not use 0 because it cannot control outside the pot range
int FeedbackPotDeadZone1	= 0;		// +/- of this value will not move the motor		
int FeedbackPotDeadZone2	= 0;		// +/- of this value will not move the motor

//PID variables
int motordirection1		= 0;			// motor 1 move direction 0=brake, 1=forward, 2=reverse
int motordirection2		= 0;			// motor 2 move direction 0=brake, 1=forward, 2=reverse
int oldmotordirection1	= 0;
int oldmotordirection2	= 0;
double K_motor_1		= 1;
double proportional1	= 4.200;		//initial value
double integral1		= 0.400;
double derivative1		= 0.400;
double K_motor_2		= 1;
double proportional2	= 4.200;
double integral2		= 0.400;
double derivative2		= 0.400;
int OutputM1					= 0;
int OutputM2					= 0;
double integrated_motor_1_error = 0;
double integrated_motor_2_error = 0;
float last_motor_1_error		= 0;
float last_motor_2_error		= 0; 

byte debugbyte =0;				//This values are for debug purpose and can be send via
int debuginteger =0;			//the SendDebug serial 211 command to the X-Sim plugin
double debugdouble =0;			

void setup()
{
	Serial.begin(115200);
	delay(1000);
	Serial.begin(115200);
	portdstatus=PORTD;
	pinMode(ControlPinM1Inp1, OUTPUT);
	pinMode(ControlPinM1Inp2, OUTPUT);
	pinMode(ControlPinM2Inp1, OUTPUT);
	pinMode(ControlPinM2Inp2, OUTPUT);
	pinMode(PWMPinM1,		  OUTPUT);
	pinMode(PWMPinM2,		  OUTPUT);
	analogWrite(PWMPinM1,	  0);
	analogWrite(PWMPinM2,	  0);
	UnsetMotor1Inp1();
	UnsetMotor1Inp2();
	UnsetMotor2Inp1();
	UnsetMotor2Inp2();
	TCCR1B = TCCR1B & 0b11111100; //This is a hack for changing the PWM frequency to a higher value, if removed it is 490Hz
#if FASTADC
	// set analogue prescale to 16
	sbi(ADCSRA,ADPS2) ;
	cbi(ADCSRA,ADPS1) ;
	cbi(ADCSRA,ADPS0) ;
#endif
}

void WriteEEPRomWord(int address, int intvalue)
{
	int low,high;
	high=intvalue/256;
	low=intvalue-(256*high);
	EEPROM.write(address,high);
	EEPROM.write(address+1,low);
}

int ReadEEPRomWord(int address)
{
	int low,high, returnvalue;
	high=EEPROM.read(address);
	low=EEPROM.read(address+1);
	returnvalue=(high*256)+low;
	return returnvalue;
}

void WriteEEProm()
{
	EEPROM.write(0,111);
	WriteEEPRomWord(1,FeedbackMin1);
	WriteEEPRomWord(3,FeedbackMax1);
	EEPROM.write(5,FeedbackPotDeadZone1);
	WriteEEPRomWord(6,FeedbackMin2);
	WriteEEPRomWord(8,FeedbackMax2);
	EEPROM.write(10,FeedbackPotDeadZone2);
	WriteEEPRomWord(11,int(proportional1*10.000));
	WriteEEPRomWord(13,int(integral1*10.000));
	WriteEEPRomWord(15,int(derivative1*10.000));
	WriteEEPRomWord(17,int(proportional2*10.000));
	WriteEEPRomWord(19,int(integral2*10.000));
	WriteEEPRomWord(21,int(derivative2*10.000));
}

void ReadEEProm()
{
	int evalue = EEPROM.read(0);
	if(evalue != 111) //EEProm was not set before, set default values
	{
		WriteEEProm();
		return;
	}
	FeedbackMin1=ReadEEPRomWord(1);
	FeedbackMax1=ReadEEPRomWord(3);
	FeedbackPotDeadZone1=EEPROM.read(5);
	FeedbackMin2=ReadEEPRomWord(6);
	FeedbackMax2=ReadEEPRomWord(8);
	FeedbackPotDeadZone2=EEPROM.read(10);
	proportional1=double(ReadEEPRomWord(11))/10.000;
	integral1=double(ReadEEPRomWord(13))/10.000;
	derivative1=double(ReadEEPRomWord(15))/10.000;
	proportional2=double(ReadEEPRomWord(17))/10.000;
	integral2=double(ReadEEPRomWord(19))/10.000;
	derivative2=double(ReadEEPRomWord(21))/10.000;
}

void SendAnalogueFeedback(int analogue1, int analogue2)
{
	high=analogue1/256;
	low=analogue1-(high*256);
	Serial.write('X');
	Serial.write(200);
	Serial.write(high);
	Serial.write(low);
	high=analogue2/256;
	low=analogue2-(high*256);
	Serial.write(high);
	Serial.write(low);
}

void SendPidCount()
{
	unsigned long value=pidcount;
	hhigh=value/16777216;
	value=value-(hhigh*16777216);
	hlow=value/65536;
	value=value-(hlow*65536);
	lhigh=value/256;
	llow=value-(lhigh*256);
	Serial.write('X');
	Serial.write(201);
	Serial.write(int(hhigh));
	Serial.write(int(hlow));
	Serial.write(int(lhigh));
	Serial.write(int(llow));
}

void SendDebugValues()
{
	//The double is transformed into a integer * 10 !!!
	int doubletransfere=int(double(debugdouble*10.000));
	Serial.write('X');
	Serial.write(211);
	Serial.write(debugbyte);
	Serial.write(HIGHBYTE(debuginteger));
	Serial.write(LOWBYTE(debuginteger));
	Serial.write(HIGHBYTE(doubletransfere));
	Serial.write(LOWBYTE(doubletransfere));
}

void SendFirmwareVersion()
{
	Serial.write('X');
	Serial.write('-');
	Serial.write('P');
	Serial.write('I');
	Serial.write('D');
	Serial.write(' ');
	Serial.write(48+firmaware_version_mayor);
	Serial.write('.');
	Serial.write(48+firmware_version_minor);
}

void EEPromToSerial(int eeprom_address)
{
	int retvalue=EEPROM.read(eeprom_address);
	Serial.write('X');
	Serial.write(204);
	Serial.write(retvalue);
}

void ClearEEProm()
{
	for(int z=0; z < 1024; z++)
	{
		EEPROM.write(z,255);
	}
}

void ParseCommand()
{
	if(commandbuffer[0]==1)			//Set motor 1 position to High and Low value 0 to 1023
	{
		target1=(commandbuffer[1]*256)+commandbuffer[2];
	}
	if(commandbuffer[0]==2)			//Set motor 2 position to High and Low value 0 to 1023
	{
		target2=(commandbuffer[1]*256)+commandbuffer[2];
	}

	if(commandbuffer[0]==200)		//Send both analogue feedback raw values
	{
		SendAnalogueFeedback(currentanalogue1, currentanalogue2);
	}
	if(commandbuffer[0]==201)		//Send PID count
	{
		SendPidCount();
	}
	if(commandbuffer[0]==202)		//Send Firmware Version
	{
		SendFirmwareVersion();
	}
	if(commandbuffer[0]==203)		//Write EEPROM
	{
		EEPROM.write(commandbuffer[1],uint8_t(commandbuffer[2]));
	}
	if(commandbuffer[0]==204)		//Read EEPROM
	{
		EEPromToSerial(commandbuffer[1]);
	}
	if(commandbuffer[0]==205)		//Clear EEPROM
	{
		ClearEEProm();
	}
	if(commandbuffer[0]==206)		//Reread the whole EEPRom and store settings into fitting variables
	{
		ReadEEProm();
	}
	if(commandbuffer[0]==207)		//Disable power on motor 1
	{
		analogWrite(PWMPinM1,	  0);
		UnsetMotor1Inp1();
		UnsetMotor1Inp2();
	}
	if(commandbuffer[0]==208)		//Disable power on motor 2
	{
		analogWrite(PWMPinM2,	  0);
		UnsetMotor2Inp1();
		UnsetMotor2Inp2();
	}
	if(commandbuffer[0]==209)		//Enable power on motor 1
	{
		analogWrite(PWMPinM1,	  128);
		UnsetMotor1Inp1();
		UnsetMotor1Inp2();
	}
	if(commandbuffer[0]==210)		//Enable power on motor 2
	{
		analogWrite(PWMPinM2,	  128);
		UnsetMotor2Inp1();
		UnsetMotor2Inp2();
	}
	if(commandbuffer[0]==211)		//Send all debug values
	{
		SendDebugValues();
	}
}

void FeedbackPotWorker()
{
	currentanalogue1 = analogRead(FeedbackPin1);
	currentanalogue2 = analogRead(FeedbackPin2);
	//Notice: Minimum and maximum scaling calculation is done in the PC plugin with faster float support
}

void SerialWorker()
{
	while(Serial.available()) 
	{
		if(buffercount==-1)
		{
			buffer = Serial.read();
			if(buffer != 'X'){buffercount=-1;}else{buffercount=0;}
		}
		else
		{
			buffer = Serial.read();
			commandbuffer[buffercount]=buffer;
			buffercount++;
			if(buffercount > 2){ParseCommand(); buffercount=-1;}
		}
	}
}

void CalculateMotorDirection()
{
	if(target1 > (currentanalogue1 + FeedbackPotDeadZone1) || target1 < (currentanalogue1 - FeedbackPotDeadZone1))
	{
		if (OutputM1 >= 0)  
		{                                    
			motordirection1=1;				// drive motor 1 forward
		}  
		else 
		{                                              
			motordirection1=2;				// drive motor 1 backward
			OutputM1 = abs(OutputM1);
		}
	}
	else
	{
		motordirection1=0;
	}

	if(target2 > (currentanalogue2 + FeedbackPotDeadZone2) || target2 < (currentanalogue2 - FeedbackPotDeadZone2))
	{
		if (OutputM2 >= 0)  
		{                                    
			motordirection2=1;				// drive motor 2 forward
		}  
		else 
		{                                              
			motordirection2=2;				// drive motor 2 backward
			OutputM2 = abs(OutputM2);
		}
	}
	else
	{
		motordirection2=0;
	}

	OutputM1 = constrain(OutputM1, -255, 255);
	OutputM2 = constrain(OutputM2, -255, 255);
}

int updateMotor1Pid(int targetPosition, int currentPosition)   
{
	float error = (float)targetPosition - (float)currentPosition; 
	float pTerm_motor_R = proportional1 * error;
	integrated_motor_1_error += error;                                       
	float iTerm_motor_R = integral1 * constrain(integrated_motor_1_error, -GUARD_MOTOR_1_GAIN, GUARD_MOTOR_1_GAIN);
	float dTerm_motor_R = derivative1 * (error - last_motor_1_error);                            
	last_motor_1_error = error;

	return constrain(K_motor_1*(pTerm_motor_R + iTerm_motor_R + dTerm_motor_R), -255, 255);
}

int updateMotor2Pid(int targetPosition, int currentPosition)   
{
	float error = (float)targetPosition - (float)currentPosition; 
	float pTerm_motor_L = proportional2 * error;
	integrated_motor_2_error += error;                                       
	float iTerm_motor_L = integral2 * constrain(integrated_motor_2_error, -GUARD_MOTOR_2_GAIN, GUARD_MOTOR_2_GAIN);
	float dTerm_motor_L = derivative2 * (error - last_motor_2_error);                            
	last_motor_2_error = error;

	return constrain(K_motor_2*(pTerm_motor_L + iTerm_motor_L + dTerm_motor_L), -255, 255);
}

void CalculatePID()
{
	OutputM1=updateMotor1Pid(target1,currentanalogue1);
	OutputM2=updateMotor2Pid(target2,currentanalogue2);
}

void SetPWM()
{
	debuginteger=motordirection1;
	debugdouble=OutputM1;
	if(motordirection1 != 0)
	{
		analogWrite(PWMPinM1, int(OutputM1));
	}
	else
	{
		analogWrite(PWMPinM1, 255);
	}
	if(motordirection2 != 0)
	{
		analogWrite(PWMPinM2, int(OutputM2));
	}
	else
	{
		analogWrite(PWMPinM2, 255);
	}
}

//Direct port manipulation, change here your port code

void SetMotor1Inp1()
{
	portdstatus |= 1 << ControlPinM1Inp1;
	PORTD = portdstatus;
}

void UnsetMotor1Inp1()
{
	portdstatus &= ~(1 << ControlPinM1Inp1);
	PORTD = portdstatus;
}

void SetMotor1Inp2()
{
	portdstatus |= 1 << ControlPinM1Inp2;
	PORTD = portdstatus;
}

void UnsetMotor1Inp2()
{
	portdstatus &= ~(1 << ControlPinM1Inp2);
	PORTD = portdstatus;
}

void SetMotor2Inp1()
{
	portdstatus |= 1 << ControlPinM2Inp1;
	PORTD = portdstatus;
}

void UnsetMotor2Inp1()
{
	portdstatus &= ~(1 << ControlPinM2Inp1);
	PORTD = portdstatus;
}

void SetMotor2Inp2()
{
	portdstatus |= 1 << ControlPinM2Inp2;
	PORTD = portdstatus;
}

void UnsetMotor2Inp2()
{
	portdstatus &= ~(1 << ControlPinM2Inp2);
	PORTD = portdstatus;
}

void SetHBridgeControl() //With direct port manipulation for speedup the arduino framework!
{
	//Motor 1
	if(motordirection1 != oldmotordirection1)
	{
		if(motordirection1 != 0)
		{
			if(motordirection1 == 1)
			{
				SetMotor1Inp1();
				UnsetMotor1Inp2();
			}
			else
			{
				UnsetMotor1Inp1();
				SetMotor1Inp2();
			}
		}
		else
		{
			UnsetMotor1Inp1();
			UnsetMotor1Inp2();
		}
		oldmotordirection1=motordirection1;
	}

	//Motor 2
	if(motordirection2 != oldmotordirection2)
	{
		if(motordirection2 != 0)
		{
			if(motordirection2 == 1)
			{
				SetMotor2Inp1();
				UnsetMotor2Inp2();
			}
			else
			{
				UnsetMotor2Inp1();
				SetMotor2Inp2();
			}
		}
		else
		{
			UnsetMotor2Inp1();
			UnsetMotor2Inp2();
		}
		oldmotordirection2=motordirection2;
	}
}

void loop()
{
	//Read all stored PID and Feedback settings
	ReadEEProm();
	//Program loop
	while (1==FASTLOOP) //Important hack: Use this own real time loop code without arduino framework delays
	{
		FeedbackPotWorker();
		SerialWorker();
		CalculatePID();
		CalculateMotorDirection();
		SetPWM();
		SetHBridgeControl();
		pidcount++;
	}
}
