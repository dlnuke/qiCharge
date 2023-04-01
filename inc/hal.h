/*
 * hal.h
 *
 *  Created on: Sep 11, 2022
 *      Author: newco
 */

#ifndef INC_HAL_H_
#define INC_HAL_H_

#include "SI_EFM8UB1_Register_Enums.h"                // SFR declarations

//structs and enums
typedef enum IoPorts
{
  Port0,  //MCU Port 0
  Port1,  //MCU Port 1
  Port2,  //MCU Port 2
  Port3,  //MCU Port 3
} IoPorts;

typedef enum PinModes
{
  ANALOG_MODE,  //ADC
  INPUT_MODE,   //Digital Input
  OUTPUT_MODE,  //Digital Output
} PinModes;

//#defines
// Pin Mappings
#define pinChargingComplete P0_B0
#define pinChargeModulate   P0_B1
#define pinChargeMonitor    P0_B2
#define pinChargeCurrent    P0_B3
#define pinUartTx           P0_B4
#define pinUartRx           P0_B5
#define pinBatteryLevel     P0_B6
#define pinBattMonEnable    P0_B7
#define pinThermDrive       P1_B0
#define pinThermMonitor     P1_B1
#define pinChargeEnable     P1_B2
#define pinGreenLED         P3_B1

//external variables
extern bool test;

//function prototypes
void DisableWatchdog (void);
void HalInit (void);
void Sleep (unsigned int ms);
void Snooze (void);

#endif /* INC_HAL_H_ */
