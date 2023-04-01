//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------

#include "hal.h"
#include "global.h"
#include "qi.h"

//-----------------------------------------------------------------------------
// main() Routine
// ----------------------------------------------------------------------------
// This is the main loop function.
//-----------------------------------------------------------------------------
int main (void)
{
  HalInit();
  InitializeQi();
  InitializeVariables();

  Sleep(25);

  while (1) {                          // Spin forever
      UpdateIO();   //Process ADC and Update Variables
      ProcessQi();  //Run the Qi Charging Process
      ProcessLED(); //Handle LED
      ProcessUART(); //Handle Serial Communication
      Sleep(1); //loop sleep 1ms

      //Todo: Enter Snooze Mode when only on battery and no UART keep alive pings
      if(chargingBattery || (chargeLevel > 100) || uartIdleTimer) //not on charger and no messages
        snoozeTimer = 30000;
      if(!snoozeTimer)
        Snooze();
  }
}

void InitializeVariables (void)
{
  battLEDTimer = 0;
  snoozeTimer = 30000;
  enableChargeTimer = 0;
  uartIdleTimer = 5000;
  chargingBattery = false;
  chargingCount = 0;
  chargeLevel = 0;
  chargeCurrentSense = 0;
  rawBatteryLevel = 0;
  thermistorLevel = 0;
}

void ProcessLED (void)
{
  int battLEDOnTime;
  int battLEDOffTime;

  if(chargingBattery) {
      if(rawBatteryLevel < BATTERY_CHARGED_THRESHOLD || chargeCurrentSense > 60) {
          battLEDOnTime = 2500/chargeCurrentSense;
          battLEDOffTime = 250;
      }
      else { //solid on
          battLEDOnTime = 250;
          battLEDOffTime = 0;
      }
  }
  else {
      battLEDOnTime = 5;
      battLEDOffTime = 2500;
  }

  if(!battLEDTimer && !busyQi) { //avoid varying load when sending packet
      if(!pinGreenLED) //LED is on
        battLEDTimer = battLEDOffTime;
      else
        battLEDTimer = battLEDOnTime;
      pinGreenLED = !pinGreenLED;
  }
}
