/************************************************************** 
* FILE:         PWM_drv.c
* DESCRIPTION:  A/D in PWM driver for TMS320F2808
* AUTHOR:       Andra� Kontar�ek, Mitja Nemec
* DATE:         21.12.2009
*
****************************************************************/
#include "PWM_drv.h"

// prototipi lokalnih funkcij


/**************************************************************
* Funkcija, ki popi�e registre za PWM1,2 in 3. Znotraj funkcije
* se omogo�i interrupt za pro�enje ADC, popi�e se perioda, compare
* register, tripzone register, omogo�i se izhode za PWM...
* return:void
**************************************************************/
void PWM_init(void)
{
//EPWM Module 1
    // setup timer base 
    EPwm1Regs.TBPRD = PWM_PERIOD/2;       //nastavljeno na 25us, PWM_PERIOD = 50us  
    EPwm1Regs.TBCTL.bit.PHSDIR = 0;       // count up after sync
    EPwm1Regs.TBCTL.bit.CLKDIV = 0;
    EPwm1Regs.TBCTL.bit.HSPCLKDIV = 0;
    EPwm1Regs.TBCTL.bit.SYNCOSEL = 1;     // sync out on zero
    EPwm1Regs.TBCTL.bit.PRDLD = 0;        // shadowed period reload
    EPwm1Regs.TBCTL.bit.PHSEN = 0;        // master timer does not sync  
    EPwm1Regs.TBCTR = 1;

    // debug mode behafiour
    #if PWM_DEBUG == 0
    EPwm1Regs.TBCTL.bit.FREE_SOFT = 0;  // stop immediately
    EPwm1Regs.TBCTL.bit.FREE_SOFT = 0;  // stop immediately
    #endif
    #if PWM_DEBUG == 1
    EPwm1Regs.TBCTL.bit.FREE_SOFT = 1;  // stop when finished
    EPwm1Regs.TBCTL.bit.FREE_SOFT = 1;  // stop when finished
    #endif
    #if FB_DEBUG == 2
    EPwm1Regs.TBCTL.bit.FREE_SOFT = 3;  // run free
    EPwm1Regs.TBCTL.bit.FREE_SOFT = 3;  // run free
    #endif
    

    EPwm1Regs.CMPCTL.bit.LOADAMODE = CC_CTR_ZERO; // re-load on zero
    EPwm1Regs.CMPCTL.bit.SHDWAMODE = CC_SHADOW; // shadowed compare reload

    // Compare registers
    EPwm1Regs.CMPA.half.CMPA = PWM_PERIOD/4;                 //50% duty cycle

    // Init Action Qualifier Output A Register 
    EPwm1Regs.AQSFRC.bit.RLDCSF = 0;        // load AQ on zero

    EPwm1Regs.AQCTLA.bit.CAU = AQ_SET;    // clear output on CMPA_UP
    EPwm1Regs.AQCTLA.bit.CAD = AQ_CLEAR;      // set output on CMPA_DOWN
    EPwm1Regs.AQCTLB.bit.CAU = AQ_CLEAR;      // set output on CMPA_UP
    EPwm1Regs.AQCTLB.bit.CAD = AQ_SET;    // clear output on CMPA_DOWN

    // Dead Time
    EPwm1Regs.DBCTL.bit.OUT_MODE = DB_DISABLE;
    EPwm1Regs.DBCTL.bit.IN_MODE = DB_DISABLE;
    // no dead time
    
    // Trip zone 

    // Event trigger
    // Pro�enje ADC-ja
    EPwm1Regs.ETSEL.bit.SOCASEL = 2;    //spro�i prekinitev na periodo
    EPwm1Regs.ETPS.bit.SOCAPRD = 1;     //ob vsakem prvem dogodku
    EPwm1Regs.ETCLR.bit.SOCA = 1;       //clear possible flag
    EPwm1Regs.ETSEL.bit.SOCAEN = 1;     //enable ADC Start Of conversion
    // Pro�enje prekinitve 
    EPwm1Regs.ETSEL.bit.INTSEL = 2;             //spro�i prekinitev na periodo
    EPwm1Regs.ETPS.bit.INTPRD = PWM_INT_PSCL;   //ob vsakem prvem dogodku
    EPwm1Regs.ETCLR.bit.INT = 1;                //clear possible flag
    EPwm1Regs.ETSEL.bit.INTEN = 1;              //enable interrupt

//EPWM Module 2

//EPWM Module 3
 
// output pin setup
    EALLOW;
    GpioCtrlRegs.GPAMUX1.bit.GPIO0 = 1;   // GPIO0 pin is under ePWM control
    GpioCtrlRegs.GPAMUX1.bit.GPIO1 = 1;   // GPIO1 pin is under ePWM control
    EDIS;                                 // Disable EALLOW

}   //end of PWM_PWM_init

/**************************************************************
* Funkcija, ki na podlagi vklopnega razmerja in izbranega vektorja
* vklopi dolo�ene tranzistorje
* return: void
* arg1: vklopno razmerje od 0.0 do 1.0 (format IQ)
**************************************************************/
void PWM_update(float duty)
{
   unsigned int compare;
   unsigned int perioda;
   long delta;
   float duty_per;

   duty = -duty;
    // za��ita za duty cycle 
    //(za��ita za sektor je narejena v default switch case stavku)
    if (duty < -1.0) duty = -1.0;
    if (duty > 1.0) duty = 1.0;

    //izra�unam vrednost compare registra(iz periode in preklopnega razmerja)
    perioda = EPwm1Regs.TBPRD;

    duty_per = perioda * duty / 2;

    delta = (long)duty_per;

    compare = perioda/2 - delta;

    // vpisem vrednost v register
    EPwm1Regs.CMPA.half.CMPA = compare;
    

}  //end of AP_PWM_update

/**************************************************************
* Funkcija, ki nastavi periodo, za doseganje zeljene periode
* in je natancna na cikel natancno
* return: void
* arg1: zelena perioda
**************************************************************/
void PWM_period(float perioda)
{
    // spremenljivke
    float   temp_tbper;
    static float ostanek = 0;
    long celi_del;

    // naracunam TBPER (CPU_FREQ * perioda)
    temp_tbper = perioda * CPU_FREQ/2;
    
    // izlocim celi del in ostanek
    celi_del = (long)temp_tbper;
    ostanek = temp_tbper - celi_del;
    // povecam celi del, ce je ostanek veji od 1
    if (ostanek > 1.0)
    {
        ostanek = ostanek - 1.0;
        celi_del = celi_del + 1;
    }
    
    // nastavim TBPER
    EPwm1Regs.TBPRD = celi_del;
}   //end of FB_period

/**************************************************************
* Funkcija, ki nastavi periodo, za doseganje zeljene frekvence
* in je natancna na cikel natancno
* return: void
* arg1: zelena frekvenca
**************************************************************/
void PWM_frequency(float frekvenca)
{
    // spremenljivke
    float   temp_tbper;
    static float ostanek = 0;
    long celi_del;

    // naracunam TBPER (CPU_FREQ / SAMPLING_FREQ) - 1
    temp_tbper = (CPU_FREQ/2) / frekvenca;

    // izlocim celi del in ostanek
    celi_del = (long)temp_tbper;
    ostanek = temp_tbper - celi_del;
    // povecam celi del, ce je ostanek veji od 1
    if (ostanek > 1.0)
    {
        ostanek = ostanek - 1.0;
        celi_del = celi_del + 1;
    }
    
    // nastavim TBPER
    EPwm1Regs.TBPRD = celi_del - 1;
}   //end of FB_frequency
  
/**************************************************************
* Funkcija, ki starta PWM1. Znotraj funkcije nastavimo
* na�in �tetja �asovnikov (up-down-count mode)
* return: void
**************************************************************/
void PWM_start(void)
{
    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 0;
    EPwm1Regs.TBCTL.bit.CTRMODE = TB_COUNT_UPDOWN;  //up-down-count mode
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 1;
    EDIS;
    
}   //end of AP_PWM_start



