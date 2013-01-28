//Board = Arduino Uno
#define ARDUINO 103
#define __AVR_ATmega328P__
#define F_CPU 16000000L
#define __AVR__
#define __cplusplus
#define __attribute__(x)
#define __inline__
#define __asm__(x)
#define __extension__
#define __ATTR_PURE__
#define __ATTR_CONST__
#define __inline__
#define __asm__ 
#define __volatile__
#define __builtin_va_list
#define __builtin_va_start
#define __builtin_va_end
#define __DOXYGEN__
#define prog_void
#define PGM_VOID_P int
#define NOINLINE __attribute__((noinline))

typedef unsigned char byte;
extern "C" void __cxa_pure_virtual() {}

//already defined in arduno.h
void WriteEEPRomWord(int address, int intvalue);
int ReadEEPRomWord(int address);
void WriteEEProm();
void ReadEEProm();
void SendAnalogueFeedback(int analogue1, int analogue2);
void SendPidCount();
void SendDebugValues();
void SendFirmwareVersion();
void EEPromToSerial(int eeprom_address);
void ClearEEProm();
void ParseCommand();
void FeedbackPotWorker();
void SerialWorker();
void CalculateMotorDirection();
int updateMotor1Pid(int targetPosition, int currentPosition);
int updateMotor2Pid(int targetPosition, int currentPosition);
void CalculatePID();
void SetPWM();
void SetMotor1Inp1();
void UnsetMotor1Inp1();
void SetMotor1Inp2();
void UnsetMotor1Inp2();
void SetMotor2Inp1();
void UnsetMotor2Inp1();
void SetMotor2Inp2();
void UnsetMotor2Inp2();
void SetHBridgeControl();
//already defined in arduno.h

#include "C:\Program Files\arduino\hardware\arduino\variants\standard\pins_arduino.h" 
#include "C:\Program Files\arduino\hardware\arduino\cores\arduino\arduino.h"
#include "C:\Users\SirNoName\Documents\Arduino\XPID\XPID.ino"
