#include <16F877A.h>
#device adc=10
#fuses NOWDT,HS, PUT, NOPROTECT, NODEBUG, BROWNOUT, NOLVP, NOCPD, NOWRT
#use delay(clock=12000000)

#define RTC_RST   PIN_C0
#define RTC_SCLK  PIN_C2
#define RTC_IO    PIN_C1

//*************************************************************
//Minhas definições:
#byte porta = 0x05
#byte portb = 0x06
#byte portc = 0x07
#byte portd = 0x08
#byte porte = 0x09

#byte adcon0 = 0x1F
#byte adcon1 = 0x9F

#define  port_disp   portb //Saída para display no portB.

//Botões:
#bit  bt_entra  = portc.4
#bit  bt_cancel = portc.5
#bit  bt_inc    = portd.2
#bit  bt_dec    = portd.3



