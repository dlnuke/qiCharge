/*
 * global.h
 *
 *  Created on: Sep 11, 2022
 *      Author: newco
 */

#ifndef INC_GLOBAL_H_
#define INC_GLOBAL_H_

//defines
#define TXRX_BUFFER_SIZE 50
#define TXRX_MESSAGE_SIZE 10

//external variables
extern char hardwareRevision;
extern unsigned int sleepTimer;
extern unsigned int battLEDTimer;
extern unsigned int qiStateTimer;
extern unsigned int uartIdleTimer;
extern unsigned int snoozeTimer;
extern unsigned int enableChargeTimer;
extern bool chargingBattery;
extern unsigned int chargingCount;
extern unsigned int chargeLevel;
extern unsigned int chargeCurrentSense;
extern unsigned int rawBatteryLevel;
extern unsigned int thermistorLevel;
extern char rxBuffer[TXRX_BUFFER_SIZE];
extern char txBuffer[TXRX_BUFFER_SIZE];
//extern char txMessage[TXRX_MESSAGE_SIZE];
extern char rxMessage[TXRX_MESSAGE_SIZE];
extern int rxIndex;
extern int rxSofar;
extern int txIndex;
extern int txSofar;
extern int rmIndex;
//extern int tmIndex;
extern bool txReady;
extern char sfrSave;

//function prototypes
void UpdateIO (void);
void InitializeVariables (void);
void ProcessLED (void);
void ProcessUART (void);
void SendTxMessage (char* txMessage, int tmLength);

//-----------------------------------------------------------------------------
// TimerProc() Callback Routine
// ----------------------------------------------------------------------------
// This is the 1ms Timer.  Inline not supported by compiler, so used #define
//-----------------------------------------------------------------------------
#define TimerProc() {\
    if(battLEDTimer) battLEDTimer--;\
    if(qiStateTimer) qiStateTimer--;\
    if(sleepTimer) sleepTimer--;\
    if(uartIdleTimer) uartIdleTimer--;\
    if(snoozeTimer) snoozeTimer--;\
    if(enableChargeTimer) enableChargeTimer--;\
}

#endif /* INC_GLOBAL_H_ */
