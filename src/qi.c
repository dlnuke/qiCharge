/*
 * qi.c
 *
 *  Created on: Sep 12, 2022
 *      Author: newco
 */

#include "hal.h"
#include "global.h"
#include "qi.h"

typedef enum {
  idle_qistate = 0,
  ping_qistate,
  id_qistate,
  config_qistate,
  power_control_qistate,
  power_received_qistate,
  power_end_qistate,
  NUM_QI_STATES
} qi_states;

int qiState;
bool bitState;
unsigned char packetTransitions[MAX_PACKET_TRANSITIONS + EXTRA_PADDING]; //declare as external variable in a header file since used in the inline function
int packetIndex; //declare as external variable in a header file since used in the inline function
int packetLength; //declare as external variable in a header file since used in the inline function
bool sendingQiPacket; //declare as external variable in a header file since used in the inline function
bool busyQi;
int chargerRemovedDebounce;
int qiControlPacketsSent;
int qiEndPowerDebounce;

void InitializeQi(void)
{
  bitState = LOW;
  SetLoadState(bitState);
  qiState = idle_qistate;
  sendingQiPacket = false;
  busyQi = false;
  packetIndex = 0;
  packetLength = 0;
  chargerRemovedDebounce = 0;
}

void LoadNextByte(unsigned char dataByte) //loads next byte in constructing the packet
{
  int i;
  unsigned char parity = 0;
  //start bit (0)
  bitState = !bitState;
  packetTransitions[packetLength++] = bitState;
  packetTransitions[packetLength++] = bitState;
  //data
  for (i = 0; i < 8; i++) {
  bitState = !bitState;
  packetTransitions[packetLength++] = bitState;
  if (dataByte & (1 << i)) {
    parity++;
    bitState = !bitState;
  }
  packetTransitions[packetLength++] = bitState;
  }
  //parity
  bitState = !bitState;
  packetTransitions[packetLength++] = bitState;
  if ((parity&1) == 0) //if even number of 1 bits, then the parity bit = 1 so set the transition
    bitState = !bitState;
  packetTransitions[packetLength++] = bitState;
  //stop bit (1)
  bitState = !bitState;
  packetTransitions[packetLength++] = bitState;
  bitState = !bitState;
  packetTransitions[packetLength++] = bitState;
}

void SendPacket(unsigned char* dataByte, int length) {
  int i;
  unsigned char checksum = 0;
  if(sendingQiPacket)
    return;
  packetLength = 0;
  packetIndex = 0;
  //first do the preamble bits
  for (i = 0; i < PREAMBLE_BITS; i++) {
  packetTransitions[packetLength++] = HIGH;
  packetTransitions[packetLength++] = LOW;
  }
  //next, the header and message from the data
  bitState = LOW;
  for (i = 0; i < length; i++) {
  LoadNextByte(dataByte[i]);
    checksum ^= dataByte[i];
  }
  //and finally the checksum
  LoadNextByte(checksum);
  packetTransitions[packetLength++] = LOW; //end packet on a low, turn off the comm loading caps...
  for (i = 0; i < EXTRA_PADDING; i++)
    packetTransitions[packetLength++] = LOW;
  //now, set the flag for the 250uS interrupt to shift out all the bits
  sendingQiPacket = true;
}

unsigned int adcvb[2];

//ProcessQi:
//call from main loop with zero delays
//requires a 1ms timer interrupt that decrements qiStateTimer by 1 each interrupt if qiStateTimer!=0 (eg. code in timer interrupt: if(qiStateTimer)qiStateTimer--;)
//requires a 250us interrupt that incorporates the CoilTimerProcTx routine above
void ProcessQi(void)
{
  unsigned char hmData[10];

  if(chargeLevel < CHARGER_DETECT_THRESHOLD && qiState > idle_qistate)
  {
    chargerRemovedDebounce++;
    if(chargerRemovedDebounce > 10) {
      InitializeQi();
      pinChargeEnable = 1; //disable charging of battery so it doesn't affect load during negotiation
    }
  }
  else
    chargerRemovedDebounce = 0;

  switch (qiState) {
    case idle_qistate: //idle
        if( (chargeLevel > CHARGE_PING_THRESHOLD) ) { //note, chargeLevel is ADC reading (converted to mV) of the charge bank through voltage divider
          qiStateTimer = 25; //target within ~65ms since power was applied
          pinGreenLED = 0; //turn led on
          pinChargeEnable = 1; //disable charging of battery so it doesn't affect load during negotiation
          qiState = ping_qistate;
          SendTxMessage("QI State=1\n",11);
        }
        break;
    case ping_qistate: //signal strength response to a ping phase (within the 19ms to 65ms ping window, target = 40ms)
      if(!qiStateTimer) {
        hmData[0] = 0x1;  //header
        hmData[1] = 255;  //ss
        SendPacket(hmData, 2);  //send ping response so that the transmitter identifies receiver.
        qiStateTimer = 10 + packetLength/4;
        qiState = id_qistate;
        SendTxMessage("QI State=2\n",11);
      }
      break;

    case id_qistate: //identification packet
      if(!qiStateTimer) {
        hmData[0] = 0x71; //header
        hmData[1] = 0x11; //version
        hmData[2] = 0x10; //mfg1
        hmData[3] = 0x20; //mfg2
        hmData[4] = 0x01; //id1
        hmData[5] = 0x02; //id2
        hmData[6] = 0x03; //id3
        hmData[7] = 0x04; //id4
        SendPacket(hmData, 8);  //send ping response so that the transmitter identifies receiver.
        qiStateTimer = 10 + packetLength/4;
        qiState = config_qistate;
        SendTxMessage("QI State=3\n",11);
      }
      break;

    case config_qistate: //configuration packet
      if(!qiStateTimer) {
        hmData[0] = 0x51; //header
        hmData[1] = 10;
        hmData[2] = 0x00;
        hmData[3] = 0x00;
        hmData[4] = 0x43;
        hmData[5] = 0x00;
        SendPacket(hmData, 6);  //send ping response so that the transmitter identifies receiver.
        qiStateTimer = 10 + packetLength/4;
        qiControlPacketsSent = 0;
        qiEndPowerDebounce = 0;
        qiState = power_control_qistate;
        SendTxMessage("QI State=4\n",11);
      }
      break;

    case power_control_qistate:
      if(!qiStateTimer) {
        char error = 0;
        int temp_error = 0;
        temp_error = (int)(CHARGE_LEVEL_TARGET - chargeLevel);
        if(temp_error > 1000)
          error = 1;
        if(temp_error < 1000)
          error = -1;
        hmData[0] = 0x3;
        hmData[1] = (unsigned char) error;
        SendPacket(hmData, 2);  //send error correction packet. 0x03 is error correction packet header. 1 BYTE payload, check WPC documents for more details.
        qiStateTimer = 100;
        qiControlPacketsSent++;
        if(qiControlPacketsSent > 7)
        {
          qiState = power_received_qistate;
          SendTxMessage("QI State=5\n",11);
        }
      }
      break;

    case power_received_qistate:
      if(!qiStateTimer){
        hmData[0] = 0x4;
        hmData[1] = 128;
        SendPacket(hmData, 2);
        qiStateTimer = 100;
        qiControlPacketsSent = 0;
        if(chargeCurrentSense > CHARGE_BATT_THRESHOLD)
          pinChargeEnable = 0; //enabled charging of battery if not enabled
        qiState = power_control_qistate;
        if(rawBatteryLevel > BATTERY_CHARGED_THRESHOLD && chargeCurrentSense < 60) {
          qiEndPowerDebounce++;
          if(qiEndPowerDebounce > 5) {
            qiStateTimer = 100;
            qiState = power_end_qistate;
            SendTxMessage("QI State=6\n",11);
          }
        }
        else
          qiEndPowerDebounce = 0;
      }
      break;

    case power_end_qistate: //hang out here until power is gone
      if(!qiStateTimer){
        hmData[0] = 0x02; //end power transfer
        hmData[1] = 0x01; //charge complete
        SendPacket(hmData, 2);
        qiStateTimer = 200;
        SendTxMessage("QI State=E\n",11);
      }
      break;

    default:
      break;
  }

  if(sendingQiPacket || (qiState>idle_qistate && qiState<power_control_qistate)){
      busyQi = true;
  }
  else
    busyQi = false;

}
