/*
 * qi.h
 *
 *  Created on: Sep 12, 2022
 *      Author: newco
 */

#ifndef INC_QI_H_
#define INC_QI_H_

//Qi Charging
//#defines
#define PREAMBLE_BITS 16
#define HEADER_LENGTH 1
#define MAX_MESSAGE_LENGTH 8
#define CHECKSUM_LENGTH 1
#define ENDBIT_LENGTH 1
#define EXTRA_PADDING 5
#define MAX_PACKET_TRANSITIONS (PREAMBLE_BITS*2 + (HEADER_LENGTH+MAX_MESSAGE_LENGTH+CHECKSUM_LENGTH)*11*2 + ENDBIT_LENGTH)
#define SetLoadState(driveLoad) ( driveLoad ? (pinChargeModulate=1):(pinChargeModulate=0) )

#define HIGH 1
#define LOW 0
#define CHARGE_LEVEL_TARGET 18000 //note, 18000mV equates to around +18.0V through voltage divider on the charge bank
#define CHARGER_DETECT_THRESHOLD 2500 //note, mV
#define CHARGE_PING_THRESHOLD 4500 //note, mV
#define CHARGE_BATT_THRESHOLD 4600 //note, the max rising threshold for SC810ULTRT chip is 4.6V
#define BATTERY_CHARGED_THRESHOLD 4150

//external variables
extern unsigned char packetTransitions[MAX_PACKET_TRANSITIONS + EXTRA_PADDING];
extern int packetIndex;
extern int packetLength;
extern bool sendingQiPacket;
extern bool busyQi;
extern int qiState;

//function prototypes
void InitializeQi(void);
void ProcessQi(void);

//-----------------------------------------------------------------------------
// CoilTimerProcTx() Callback Routine
// ----------------------------------------------------------------------------
// This is the 0.250ms Timer.  Inline not supported by compiler, so used #define
//-----------------------------------------------------------------------------
#define CoilTimerProcTx() {\
  if(sendingQiPacket)\
  {\
    if(packetIndex < packetLength)\
      SetLoadState(packetTransitions[packetIndex++]);\
    else\
      sendingQiPacket = false;\
  }\
}

#endif /* INC_QI_H_ */
