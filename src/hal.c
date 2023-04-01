/*
 * hal.c
 *
 *  Created on: Sep 11, 2022
 *      Author: newco
 */

#include "hal.h"
#include "global.h"
#include "qi.h"

// Variables
int msTimer;
bool t4Overflow;

// Local Function Prototypes
void HalInitClocks (void);
void HalGetHardwareRev (void);
void HalConfigureIO (IoPorts port, unsigned char bitnum, PinModes mode, bool setHigh);
void HalInitADC (void);
void HalInitGPIO (void);
void HalInitTimers (void);
void HalInitUart (void);

//-----------------------------------------------------------------------------
// SiLabs_Startup() Routine
// ----------------------------------------------------------------------------
// This function is called immediately after reset, before the initialization
// code is run in SILABS_STARTUP.A51 (which runs before main() ). This is a
// useful place to disable the watchdog timer, which is enable by default
// and may trigger before main() in some instances.
//-----------------------------------------------------------------------------
void SiLabs_Startup (void)
{
  // Disable the watchdog here
  DisableWatchdog();
}

//-----------------------------------------------------------------------------
// DisableWatchdog() Routine
// ----------------------------------------------------------------------------
void DisableWatchdog (void)
{
  //Writing 0xDE followed within 4 system clocks by 0xAD disables the WDT
  WDTCN = 0xDE; //Write 0xDE to Watchdog Timer Control Register
  WDTCN = 0xAD; //Write 0xAD to Watchdog Timer Control Register
}

//-----------------------------------------------------------------------------
// SI_INTERRUPT() Callback Routine
// ----------------------------------------------------------------------------
SI_INTERRUPT(TIMER0_ISR,TIMER0_IRQn)
{
  TCON_TF0 = 0; //Clear the interrupt flag for Timer 0 Overflow
  TH0 = 0xD1;   //0x4480 is 65536-12000, overflow after 0.250ms
  TL0 = 0x20;
  CoilTimerProcTx(); //proc for qi transmit, call every 0.250ms
  if(msTimer)
    msTimer--;
  else
  {
    msTimer = 3; //3,2,1,0 -- 4 x 0.250ms = 1ms
    TimerProc(); //1ms timer event
  }
}

//-----------------------------------------------------------------------------
// SI_INTERRUPT() Callback Routine
// ----------------------------------------------------------------------------
SI_INTERRUPT(UART0_ISR,UART0_IRQn)
{
  if(SCON0_TI) //transmit complete
  {
    SCON0_TI = 0; //clear it
    txReady = true;
  }
  if(SCON0_RI) { //receive flag is set
      rxBuffer[rxIndex] = SBUF0; //read received data byte
      rxIndex++;
      if(rxIndex >= TXRX_BUFFER_SIZE)
        rxIndex = 0;
  }
}

SI_INTERRUPT (TIMER4_ISR, TIMER4_IRQn)
{
  //todo: something if we ever want to enable and use this interrupt off the LFOSC
}

//-----------------------------------------------------------------------------
// HalInit() Routine
// ----------------------------------------------------------------------------
void HalInit (void)
{
  //Initialize Clocks
  HalInitClocks();
  //Initialize ADC
  HalInitADC();
  //Read HW Rev
  HalGetHardwareRev();
  //Initialize GPIOs and Peripherals
  HalInitGPIO();
  //Initialize Timers and Interrupts
  HalInitTimers();
  //Initialize UART
  HalInitUart();
}

//-----------------------------------------------------------------------------
// HalInitClocks() Routine
// ----------------------------------------------------------------------------
void HalInitClocks (void)
{
  //default system clock is 24.5MHz/8 = 3.0625MHz
    CLKSEL = CLKSEL_CLKSL__HFOSC0 | CLKSEL_CLKDIV__SYSCLK_DIV_1; //24.5MHz
    while ((CLKSEL & CLKSEL_DIVRDY__BMASK) == CLKSEL_DIVRDY__NOT_READY);
    //Note: >=24MHz required prior to switching to 48MHz clock
    CLKSEL = CLKSEL_CLKSL__HFOSC1 | CLKSEL_CLKDIV__SYSCLK_DIV_1; //48MHz system clock
    while ((CLKSEL & CLKSEL_DIVRDY__BMASK) == CLKSEL_DIVRDY__NOT_READY);
}

//-----------------------------------------------------------------------------
// HalInitADC() Routine
// ----------------------------------------------------------------------------
void HalInitADC(void)
{
  ADC0CN0 = 0x40;   //enable burst mode for 12 bit reads
  ADC0CF = 0x18;    //SAR clk = system clock divided by 4 (3+1) = 12MHz
  //ADC0CF = 0x7A;    //SAR clk = system clock divided by 16 (15+1) = 3MHz, w/ Tracking
  ADC0AC = 0xC3;    //12-bits, right justified, 4x accumulate
  ADC0PWR = 0x40;   //Mode 1
  ADC0MX = 0x1F;    //default channel = none
  ADC0CN0_ADEN = 1; //enable the ADC
}

//-----------------------------------------------------------------------------
// HalGetHardwareRev() Routine
// ----------------------------------------------------------------------------
// Requirement: Must Initialize ADC prior to using this function
//-----------------------------------------------------------------------------
void HalGetHardwareRev (void)
{
  unsigned int adcReading;
  //Save relevant crossbar and GPIO settings
  unsigned char saveP0MDIN = P0MDIN;
  unsigned char saveP0MDOUT = P0MDOUT;
  unsigned char saveP0 = P0;
  unsigned char saveP0SKIP = P0SKIP;
  unsigned char saveXBR0 = XBR0;
  unsigned char saveXBR2 = XBR2;
  //Disable UART
  XBR0 &= ~XBR0_URT0E__BMASK;
  //Set Tx as Analog Input
  HalConfigureIO(Port0, 4, ANALOG_MODE, false); //set pin to analog input
  HalConfigureIO(Port1, 2, OUTPUT_MODE, 0); //drive green LED to obtain reading
  //Read Tx with ADC
  XBR2 |= XBR2_XBARE__ENABLED; //enable the crossbar
  ADC0MX = ADC0MX_ADC0MX__ADC0P4; //maps to P0.4/Tx pin
  ADC0CN0_ADINT = 0; //clear the flag
  ADC0CN0_ADBUSY = 1; //start conversion
  while(!ADC0CN0_ADINT);//wait until conversion complete
  adcReading = (ADC0H<<8)|ADC0L;
  ADC0CN0_ADINT = 0; //clear the flag
  hardwareRevision = 0;
  if(adcReading<15500 && adcReading>14500) //in the proper range for a revision
    hardwareRevision = 'A';
  //Restore relevant crossbar and GPIO settings, leave green LED configured
  ADC0MX = 0x1F; //default channel = none
  XBR2 = saveXBR2;
  XBR0 = saveXBR0;
  P0MDIN = saveP0MDIN;
  P0MDOUT = saveP0MDOUT;
  P0 = saveP0;
  P0SKIP = saveP0SKIP;
}

//-----------------------------------------------------------------------------
// HalInitGPIO() Routine
// ----------------------------------------------------------------------------
void HalInitGPIO (void)
{
  HalConfigureIO(Port0, 0, INPUT_MODE, false);    //Charging Complete Input
  HalConfigureIO(Port0, 1, OUTPUT_MODE, 0);       //Charge Modulate Output, 1=driving
  HalConfigureIO(Port0, 2, ANALOG_MODE, false);   //Charge Bank Voltage Monitor
  HalConfigureIO(Port0, 3, ANALOG_MODE, false);   //Charging Current Monitor
  //HalConfigureIO(Port0, 4, INPUT_MODE, false);    //Tx to be configured by Xbar
  //HalConfigureIO(Port0, 5, INPUT_MODE, false);    //Rx to be configured by Xbar
  HalConfigureIO(Port0, 6, ANALOG_MODE, false);   //Battery Voltage Level Monitor
  HalConfigureIO(Port0, 7, INPUT_MODE, false);    //Voltage Divider EN, Open Drain
  HalConfigureIO(Port1, 0, OUTPUT_MODE, 0);       //Thermistor Drive Output, 1=driving
  HalConfigureIO(Port1, 1, ANALOG_MODE, false);   //Thermistor Voltage Monitor
  HalConfigureIO(Port1, 2, OUTPUT_MODE, 1);       //Charge Enable Output, 0=enabled
  USB0CF &= ~USB0CF_VBUSEN__BMASK;  //make sure P3.1 is set to GPIO
  HalConfigureIO(Port3, 1, OUTPUT_MODE, 0);       //Green LED, 0=driving

  pinBattMonEnable = 0; //open drain low (on)

  XBR2 |= XBR2_XBARE__ENABLED; //enable the crossbar
}

//-----------------------------------------------------------------------------
// HalConfigureIO() Routine
// ----------------------------------------------------------------------------
void HalInitTimers (void)
{
  //Initialize Timer0 to be a 1ms 16-bit timer, manual reload
  msTimer = 0; //initialize the 1ms timer variable
  IE_ET0 = 1; //enable Timer0 interrupts
  //IP_PT0 = 1; //raised priority for Timer0
  CKCON0 &= ~(0x07); //clear Timer0 settings
  CKCON0 |= 0x04; //use system clock, no prescaler
  TMOD &= ~(0x0f); //clear Timer0 settings
  TMOD |= 0x01; //16 bit mode
  TH0 = 0xD1;   //0x4480 is 65536-12000, overflow after 0.250ms
  TL0 = 0x20;
  TCON_TR0 = 1; //enable Timer0
  //Timer1 is used by the UART
  //Initialize Timer 4 for snooze routine
  sfrSave = SFRPAGE;
  SFRPAGE = 0x10;
  CKCON1 = 0x00; //use clk selected in TMR4CN0
  TMR4CN0 = TMR4CN0_T4XCLK__LFOSC_DIV_8;
  TMR4RLH = 0x00;
  TMR4RLL = 0x00;
  //EIE2 = 0x10; //enable Timer4 interrupts
  LFO0CN = LFO0CN_OSCLEN__BMASK | LFO0CN_OSCLD__DIVIDE_BY_1; //note, found this must be div by 1 to wake the snooze
  while (!(LFO0CN & LFO0CN_OSCLRDY__BMASK));
  //TMR4CN0_TR4 = 1; //enable/run Timer 4  //enable this if you want to wake snooze from timer 4
  SFRPAGE = sfrSave;
  IE_EA = 1; //enable global interrupts
}

//-----------------------------------------------------------------------------
// HalConfigureIO() Routine
// ----------------------------------------------------------------------------
void HalConfigureIO (IoPorts port, unsigned char bitnum, PinModes mode, bool setHigh)
{
  unsigned char pin = (1<<bitnum);
  switch(port)
  {
    case Port0:
      switch(mode)
      {
        case ANALOG_MODE:
          P0MDIN &= ~pin;   //set to Analog
          P0 |= pin;        //high, floating
          break;
        case INPUT_MODE:
          P0MDIN |= pin;    //set to Digital
          P0 |= pin;        //high, floating
          P0MDOUT &= ~pin;  //open Drain
          break;
        case OUTPUT_MODE:
          P0MDIN |= pin;    //set to Digital
          if(setHigh)
            P0 |= pin;      //drive high
          else
            P0 &= ~pin;     //drive low
          P0MDOUT |= pin;   //Push-Pull
          break;
      }
      P0SKIP |= pin; //in all cases, skip crossbar since pin is configured
      break;
    case Port1:
      switch(mode)
      {
        case ANALOG_MODE:
          P1MDIN &= ~pin;   //set to Analog
          P1 |= pin;        //high, floating
          break;
        case INPUT_MODE:
          P1MDIN |= pin;    //set to Digital
          P1 |= pin;        //high, floating
          P1MDOUT &= ~pin;  //open Drain
          break;
        case OUTPUT_MODE:
          P1MDIN |= pin;    //set to Digital
          if(setHigh)
            P1 |= pin;      //drive high
          else
            P1 &= ~pin;     //drive low
          P1MDOUT |= pin;   //Push-Pull
          break;
      }
      P1SKIP |= pin; //in all cases, skip crossbar since pin is configured
      break;
    case Port2:
      switch(mode)
      {
        case ANALOG_MODE:
          P2MDIN &= ~pin;   //set to Analog
          P2 |= pin;        //high, floating
          break;
        case INPUT_MODE:
          P2MDIN |= pin;    //set to Digital
          P2 |= pin;        //high, floating
          P2MDOUT &= ~pin;  //open Drain
          break;
        case OUTPUT_MODE:
          P2MDIN |= pin;    //set to Digital
          if(setHigh)
            P2 |= pin;      //drive high
          else
            P2 &= ~pin;     //drive low
          P2MDOUT |= pin;   //Push-Pull
          break;
      }
      P2SKIP |= pin; //in all cases, skip crossbar since pin is configured
      break;
    case Port3:
      switch(mode)
      {
        case ANALOG_MODE:
          P3MDIN &= ~pin;   //set to Analog
          P3 |= pin;        //high, floating
          break;
        case INPUT_MODE:
          P3MDIN |= pin;    //set to Digital
          P3 |= pin;        //high, floating
          P3MDOUT &= ~pin;  //open Drain
          break;
        case OUTPUT_MODE:
          P3MDIN |= pin;    //set to Digital
          if(setHigh)
            P3 |= pin;      //drive high
          else
            P3 &= ~pin;     //drive low
          P3MDOUT |= pin;   //Push-Pull
          break;
      }
      //P3SKIP |= pin; //there is no skip register for Port3
      break;
  }
}

void HalInitUart (void)
{
  rxIndex = 0; //buffer index for received bytes
  rxSofar = 0; //buffer index for received bytes processed
  txIndex = 0;
  txSofar = 0;
  rmIndex = 0;
  txReady = true;
  //Init Timer1 for UART Baud
  CKCON0 &= ~(0x08); //clear Timer1 settings
  //CKCON0 |= 0x08; //use system clock, no prescaler
  CKCON0 |= 0x02; //sys clk /48 = 1MHz
  TMOD &= ~(0xf0); //clear Timer1 settings
  TMOD |= 0x20; //8 bit mode
  TH1 = 0xCB;   //reload value, 9600, 1M/9600/2 = 52, 255-52=203 or 0xCB
  TL1 = 0xCB;
  TCON_TR1 = 1; //enable Timer1
  //Setup UART0
  SCON0 = 0x50; //receiver enabled
  //Enable UART0
  XBR0 |= XBR0_URT0E__ENABLED; //enable UART0 in the crossbar
  //Enable UART0 interrupts
  IE_ES0 = 1; //specifically enable UART0 interrupts
}

void Sleep (unsigned int ms)
{
  sleepTimer = ms * 4;
  while(sleepTimer);
}

void Snooze (void)
{
  SendTxMessage("Enter Snooze\n",13);
  while(txSofar != txIndex)
    ProcessUART();
  Sleep(2); //wait long enough to ensure last char is sent out at 9600
  //enable pin match for charge EOC pin P0.0
  P0MASK = 0x01; //compare bit P0.0
  P0MAT = 0x01; //compare with a low value
  pinBattMonEnable = 1; //open drain high (off)
  pinChargeEnable = 0; //make sure this is pulled the same way as the resistor
  CLKSEL = CLKSEL_CLKSL__HFOSC0 | CLKSEL_CLKDIV__SYSCLK_DIV_1; //24.5MHz
  while ((CLKSEL & CLKSEL_DIVRDY__BMASK) == CLKSEL_DIVRDY__NOT_READY);
  IE_EA = 0; //disable global interrupts
  sfrSave = SFRPAGE;
  SFRPAGE = 0x10; //to set timer 4, if using timer 4 for wakeup
  TMR4H = 0x00;
  TMR4L = 0x00;
  SFRPAGE = 0x00; //to set snooze
  PCON1 = PCON1_SNOOZE__SNOOZE | PCON1_SUSPEND__NORMAL;
  //snoozing here
  PCON1 = PCON1_SNOOZE__NORMAL | PCON1_SUSPEND__NORMAL;
  SFRPAGE = sfrSave;
  CLKSEL = CLKSEL_CLKSL__HFOSC1 | CLKSEL_CLKDIV__SYSCLK_DIV_1; //48MHz system clock
  while ((CLKSEL & CLKSEL_DIVRDY__BMASK) == CLKSEL_DIVRDY__NOT_READY);
  HalInit();
  InitializeQi();
  InitializeVariables();
  Sleep(25);
  SendTxMessage("Exit Snooze\n",12);
}

