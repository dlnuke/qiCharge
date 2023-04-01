/*
 * update.c
 *
 *  Created on: Sep 12, 2022
 *      Author: newco
 */

#include "hal.h"
#include "global.h"
#include "qi.h"

//variables
bool adcError;
int adcErrorCount;
bool adcSampleWait;
unsigned char adcIndex;
static char adcPinMap[] = {
    ADC0MX_ADC0MX__ADC0P2, //charge bank voltage level
    ADC0MX_ADC0MX__ADC0P3, //charging current
    ADC0MX_ADC0MX__ADC0P6, //battery voltage
    ADC0MX_ADC0MX__ADC0P9, //thermistor
};

//function prototypes
void ProcessADC (void);
bool BatteryIsCharging (void);

void UpdateIO (void)
{
  ProcessADC();
  chargingBattery = BatteryIsCharging();
}

void ProcessADC (void)
{
  unsigned int adcReading;
  if(ADC0CN0_ADINT) //conversion complete
  {
      if(!adcError && !adcSampleWait)
      {
          adcReading = (ADC0H<<8)|ADC0L; //16383 = 3300mV (4.96 ADC bits / mV)
          switch(adcIndex)
          {
            case 0: //charge bank voltage level
              chargeLevel = adcReading * 2;
              break;
            case 1: //charging current (mV / 11 / 0.25ohms = mA) div by 13.75 or 55/4
              chargeCurrentSense = (adcReading * 4) / 55;
              break;
            case 2: //battery voltage
              if(pinBattMonEnable)
                rawBatteryLevel = adcReading / 5;
              else
                rawBatteryLevel = (2 * adcReading) / 5;
              break;
            case 3: //thermistor
              thermistorLevel = adcReading / 5;
              break;
            default:
              break;
          }
          adcIndex++;
          if(adcIndex >= sizeof(adcPinMap))
            adcIndex = 0;
          ADC0MX = adcPinMap[adcIndex]; //switch ADC Pin
          adcSampleWait = true; //introduces a wait after switching pins
      }
      else {
        ADC0CN0_ADINT = 0; //clear flag
        ADC0CN0_ADBUSY = 1; //start conversion
        adcSampleWait = false;
        adcError = false;
      }
  }
  else
  {
    adcErrorCount++;
    if(adcErrorCount > 2) {
      ADC0CN0_ADINT = 1; //had enough time, so manually set flag
      adcIndex = 0; //set index back to 0
      ADC0MX = adcPinMap[adcIndex];
      adcError = true;
      adcErrorCount = 0;
    }
  }
}

bool BatteryIsCharging (void)
{
  bool retVal = (!pinChargingComplete || chargeCurrentSense > 60);
  if(chargeCurrentSense > 60 && pinChargingComplete) { //charging more than 60mA, but not indicated
      chargingCount++;
      if(chargingCount > 1000) {//stop charging, could shutdown qi and let qi start it again, but disable and re-enable works
          pinChargeEnable = 1; //charging of battery is disabled
          chargingCount = 0;
          enableChargeTimer = 500;
      }
  }
  else //nothing out of the ordinary, clear the debounce
    chargingCount = 0;

  if(!enableChargeTimer)
    pinChargeEnable = 0; //enabled charging of battery

  return retVal;
}
