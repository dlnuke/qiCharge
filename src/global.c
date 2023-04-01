/*
 * global.c
 *
 *  Created on: Sep 11, 2022
 *      Author: newco
 */

#include "hal.h"
#include "global.h"

//external variables
char hardwareRevision;
unsigned int sleepTimer;
unsigned int battLEDTimer;
unsigned int qiStateTimer;
unsigned int uartIdleTimer;
unsigned int snoozeTimer;
unsigned int enableChargeTimer;
bool chargingBattery;
unsigned int chargingCount;
unsigned int chargeLevel;
unsigned int chargeCurrentSense;
unsigned int rawBatteryLevel;
unsigned int thermistorLevel;
char rxBuffer[TXRX_BUFFER_SIZE];
char txBuffer[TXRX_BUFFER_SIZE];
//char txMessage[TXRX_MESSAGE_SIZE];
char rxMessage[TXRX_MESSAGE_SIZE];
int rxIndex;
int rxSofar;
int txIndex;
int txSofar;
int rmIndex;
//int tmIndex;
bool txReady;
char sfrSave;
