/*
 * uart.c
 *
 *  Created on: Oct 29, 2022
 *      Author: newco
 */

#include "hal.h"
#include "global.h"

void ProcessRxMessage (void);

void ProcessUART (void)
{
  //Receive
  if(rxSofar != rxIndex) //not caught up
  {
      uartIdleTimer = 5000;
      rxMessage[rmIndex] = rxBuffer[rxSofar++];
      if(rxMessage[rmIndex] == 0x0A) //end of message
        ProcessRxMessage();
      else
        rmIndex++;
      if(rxSofar >= TXRX_BUFFER_SIZE)
        rxSofar = 0;
      if(rmIndex >= TXRX_MESSAGE_SIZE)
        rmIndex = 0;
  }
  //Transmit
  if(txSofar != txIndex)
  {
      if(txReady)
      {
          txReady = false;
          SBUF0 = txBuffer[txSofar++]; //flag will be set again once byte transmit has completed
          if(txSofar >= TXRX_BUFFER_SIZE)
            txSofar = 0;
      }
  }
}

void ProcessRxMessage (void)
{
  //todo: handle message, for now loop back
  if(rxMessage[0] == 'b' && rxMessage[1] == 'l') //restart in bootloader mode
  {
      // Write R0 and issue a software reset
      *((uint8_t SI_SEG_DATA *)0x00) = 0xA5;
      RSTSRC = RSTSRC_SWRSF__SET | RSTSRC_PORSF__SET;
  }
  SendTxMessage(rxMessage, rmIndex+1);
  rmIndex = 0;
}

void SendTxMessage (char* txMessage, int tmLength)
{
  int i;
  //int tmLength = sizeof(txMessage);
  for(i=0; i<tmLength; i++)
  {
      txBuffer[txIndex++] = txMessage[i];
      if(txIndex >= TXRX_BUFFER_SIZE)
        txIndex = 0;
  }
}
