#include "displayzao.h"
#include <DS1302.C>

/*
Ricardo B. Morim
2007

Esse código foi programado e compilado utilizando o CCS PIC compiler

"Displayzao == Big display"

Sistema de relógio com display graúdo, extraído de sucata de máquina caça-níqueis, presente
do meu amigo Márcio Rossi!
Xtal de 12MHz!
Utilizo a interrupção do timer0 para multiplexar o display, a cada 1.3 milisegundos.
Os segmentos correspondentes ao valor a ser mostrado no display é acionado pelo PortB,
que aciona o driver ULN2803.
O dígito correspondente (Anodo comum) é acionado pelo portD através do 74ls238.
Os displays são de ANODO COMUM, cujo pino comum é acionado por um transistor PNP TIP125.

Para o RTC, utilizei o driver do CCS para o DS1302, porém na prática utilizo o
HT1380, que é a mesma coisa só não tem RAM.

O dígito mais à esquerda é o d0 e o mais à direita (pequeno) é o d6.

Para sensor de temperatura utilizo o TMP36. Fornece 10 mV/°C, sendo a saída
de 500mV @ 0°C.

**********************   PIC16F877A   **********************

*/

//******************  V. 1.0 *******************************
// Junho/2007

#use fast_io(A)
#use fast_io(B)
#use fast_io(C)
#use fast_io(D)
#use fast_io(E)
//*****************************************************************************

//*****************************************************************************
#define  traco 13    //para acessar o '-' na tabela.
#define  H     10
#define  C     11
#define  grau  12
#define  nulo  14    //Dígito apagado
#define  erre  15    //r, tabela do display.
#define  ahh   16    //A
#define  lhh   17    //L
#define  relogio  10
#define  alarme   00

//*****************************************************************************
//Protótipos de funções.
void atualiza_horario(void);
void atualiza_temperatura(void);
unsigned char ler_temperatura(void);
void acerta_relogio(unsigned char oque);
void byte_bcd(unsigned char n);
void verifica_alarme(void);


//*****************************************************************************
//Variáveis globais:
unsigned char d0=63, d1=63, d2=63, d3=63, d4=63, d5=63, d6=63;   //Dígitos dos displays.
unsigned char hora, minuto, segundo;
unsigned char m_temp[3];
unsigned char alarme_hr = 0;   //hora do alarme.
unsigned char alarme_min = 0;  //minuto do alarme.

//Byte de flags:
unsigned char flags = 0;
#bit  flg_tmr0    = flags.0
#bit  dp_graus    = flags.1
#bit  apaga_d2    = flags.2
#bit  piscar_d2   = flags.3
#bit  flg_alarme  = flags.4   //Ativa o alarme.
#bit  flg_alarmar = flags.5

//*****************************************************************************
//Tabela de conversão para mostrar os dígitos no display.
//PB0 = Segmento A.
//PB1 = Segmento B.
//PB6 = Segmento G.
//PB7 = DP.
const unsigned char tab_display [] =
{
   0b00111111,    // 0
   0b00000110,    // 1
   0b01011011,    // 2
   0b01001111,    // 3
   0b01100110,    // 4
   0b01101101,    // 5
   0b01111101,    // 6
   0b00000111,    // 7
   0b01111111,    // 8
   0b01101111,    // 9
   0b01110110,    // 10 ->H
   0b00111001,    // 11 ->C
   0b01100011,    // 12 ->º
   0b01000000,    // 13 ->-
   0b00000000,    // 14 -> nulo-> digito apagado.
   0b01010000,    // 15 -> r
   0b01110111,    // 16 -> A
   0b00111000     // 17 -> L
};

//*****************************************************************************
//Interrupção do timer0, a cada 1.3ms para multiplexar os displays.
//O "comum" dos dígitos é acionado através do 74LS238->ULN2803A.
//Acesso ao 74ls238 via PORTD <7:4>.
#int_RTCC
RTCC_isr()
{
   static unsigned char digito = 0;
   static unsigned char aux =70;
   static unsigned char t_bip;

   if(!aux)
   {
      if(apaga_d2)   apaga_d2 =0;
      else           apaga_d2 =1;
      aux = 70;
   }

   output_d(portd &= 0b00001111);   //Apaga todos os dígitos.

   switch(digito)
   {
      case 0:  port_disp = d0;   //Põe o valor do primeiro dígito na saída.
               output_d(portd |= 0b10010000);   //Liga o display d0.
               break;

      case 1:  port_disp = d1;
               if(dp_graus)   port_disp |= 0b10000000;   //Liga o DP.
               output_d(portd |= 0b10100000);   //Display d1.
               break;

      case 2:  port_disp = d2;
               if(apaga_d2 && piscar_d2)   output_d(portd &= 0b00001111);   //Apaga d2.
               else           output_d(portd |= 0b10110000);   //Display d2.
               break;

      case 3:  port_disp = d3;
               output_d(portd |= 0b11000000);   //Display d3.
               break;

      case 4:  port_disp = d4;
               output_d(portd |= 0b11010000);   //Display d4.
               break;

      case 5:  port_disp = d5;
               output_d(portd |= 0b11100000);   //Display d5.
               aux--;
               break;

      case 6:  port_disp = d6;
               if(flg_alarme) port_disp |= 0b10000000;   //Liga o DP de d6 (alarme ativo).
               output_d(portd |= 0b11110000);   //Display d6.
               break;
   }//switch()

   if(digito <6)  digito++;
   else        digito = 0;
   flg_tmr0 = 1;

   //A cada int. t_bip é incrementado. De 0 a 127 não bipa. de 128 a 255 bipa,
   //perfazendo assim um bip com intervalo.
   t_bip++;
   if(flg_alarmar && (t_bip & 128))      //Saída de alarme em RE1
      {
         #asm
         movlw 2
         xorwf porte,f
         #endasm
      }

}//RTCC_isr()

//*****************************************************************************
//*****************************************************************************
void main()
{
   unsigned char seg=0, min=0, hora=0;

   set_tris_a(255);           //Port A todo entrada.
   set_tris_b(0);             //PortB todo saída.
   set_tris_c(0b11111010);
   set_tris_d(0b00001111);    // <D7:D4> controlam os dígitos (o comum).
   set_tris_e(0b00000001);    //E0 não é usado; E1 e E2 saídas.

   setup_adc_ports(AN0);
   setup_adc(ADC_CLOCK_DIV_16);
   set_adc_channel(0);        //Seleciona o canal 0 do ADC (sensor de temp.).
   setup_psp(PSP_DISABLED);
  // setup_spi(SPI_MASTER|SPI_L_TO_H|SPI_CLK_DIV_16);
   setup_timer_0(RTCC_INTERNAL|RTCC_DIV_16);
   setup_timer_1(T1_DISABLED);
   setup_timer_2(T2_DISABLED,0,1);
   setup_comparator(NC_NC_NC_NC);
   setup_vref(FALSE);
   rtc_init();
   enable_interrupts(INT_RTCC);
   enable_interrupts(GLOBAL);

   delay_ms(2000);

   //Inicialização da leitura de temperatura. Devido ao buffer circular é
   //necessário chamar a rotina pelo menos 8x antes de "usar" o valor.
   for(seg = 20; seg; seg--)  ler_temperatura();

   while(1)
   {
      unsigned char conta = 80;  //Tempo que fica mostrando a Hora.

      while(conta)
      {
         atualiza_horario();
         piscar_d2 = 1;       //Pisca o traço entre horas e minutos.
         delay_ms(90);
         conta--;
         if(bt_entra)  acerta_relogio(alarme);
         else
            if(bt_inc && bt_dec) acerta_relogio(relogio);

         verifica_alarme();
      }

      piscar_d2 = 0;    //Não deixa piscar o dígito d2.
      conta = 15;  //Tempo mostrando a Temperatura.
      //dp_graus = 1;  //mostra ponto decimal da temperatura.
      while(conta)
      {
         atualiza_temperatura();
         delay_ms(200);
         conta--;
         if(bt_entra)
         {
            //dp_graus = 0;
            acerta_relogio(alarme);
            //dp_graus = 1;
         }
         else
            if(bt_inc && bt_dec)
            {
               //dp_graus = 0;
               acerta_relogio(relogio);
               //dp_graus = 1;
            }
      }
      //dp_graus = 0;  //Desliga o DP.

   }//while(1)

}//main()

//*****************************************************************************
//*****************************************************************************
//Copia os valores lidos do RTC para as variáveis que são mostradas no display.
void atualiza_horario(void)
{
   rtc_get_time(hora, minuto, segundo);   //Lê o relógio.
   flg_tmr0 = 0;
   while(!flg_tmr0);    //Aguarda sair da interrupção para atualizar valores.
   d0 = tab_display[(hora >>4)];
   d1 = tab_display[(hora & 15)];
   d2 = tab_display[traco];
   d3 = tab_display[(minuto >>4)];
   d4 = tab_display[(minuto & 15)];
   d5 = tab_display[(segundo >>4)];
   d6 = tab_display[(segundo & 15)];
}
//*****************************************************************************
void verifica_alarme(void)
{
   unsigned char temp;

   if(hora == alarme_hr && minuto == alarme_min && flg_alarme)
   {
      do
      {
         flg_alarmar = 1;
         for(temp = 50; temp && !bt_entra; temp--)
         {
            atualiza_horario();
            delay_ms(50);
         }
         flg_alarmar = 0;
         for(temp = 35; temp && !bt_entra; temp--)
         {
            atualiza_horario();
            delay_ms(50);
         }

      }while(!bt_entra && minuto == alarme_min); //Aguarda acionar o botão para parar o alarme ou passar 1 minuto.

      flg_alarmar = 0;
      flg_alarme  = 0;
      while(bt_entra);
      delay_ms(200);
      if(bt_entra)   //Testa de novo.
      {
         while(bt_entra);
         delay_ms(150);
      }
   }
}

//*****************************************************************************
//Copia o valor de temperatura para as variáveis que são mostradas no display.
void atualiza_temperatura(void)
{
   unsigned char valor;

   valor = ler_temperatura();
   byte_bcd(valor);

   flg_tmr0 = 0;
   while(!flg_tmr0); //Aguarda sair de uma interrupção.
   //d2 = tab_display[(valor%10)];           //Unidade.
   //d1 = tab_display[((valor/100) %10)];      //Centena.
   //d0 = tab_display[(((valor/1000) %10))];   //Milhar.
   d1 = tab_display[m_temp[2]];  //Unidade.
   d0 = tab_display[m_temp[1]];  //Dezena.
   if(d0 == tab_display[0])   d0 = tab_display[nulo]; //Não mostra o '0' à esqu.
   d2 = tab_display[nulo];
   d3 = tab_display[grau];
   d4 = tab_display[C];    //Apaga os últimos dígitos.
   d5 = tab_display[nulo];
   d6 = tab_display[nulo];
}

//*****************************************************************************
//*****************************************************************************
//Função para ler a temperatura através de um sensor TMP36.
//500 mV @ 0°C.,
//Com Vref = 5.0V -> 4.88 mV/bit -> 500mV ~= 101 no ADC.
//Temperatura de 0°C 99°C -> 0V a 990 mV, ADC: 0 a 202.
unsigned char ler_temperatura(void)
{
   static unsigned char sensor[16];
   static unsigned char pt_sensor = 15;
   unsigned int16 soma_sensor;
   unsigned char temp;

   sensor[pt_sensor] = (read_adc() - 101);
   if(pt_sensor)  pt_sensor--;
   else           pt_sensor = 15;

   soma_sensor =0;
   for(temp=0; temp <16; temp++) soma_sensor += sensor[temp];    //Soma as leituras.
   soma_sensor >>= 4;   //Divide pelo número de leituras (16).
   soma_sensor *= 49;   //Multiplica * 49 (~4.887 mV/bit).
   //Atenção: Soma 50 para que, se a temperatura estiver em 17.6, por ex. mostra 18 °C.
   //Se estiver em 17.4, mostra 17 °C.
   soma_sensor += 50;

   return (soma_sensor/100);
}

//*****************************************************************************
//Função para acertar o relógio e o alarme.
//Recebe: Alarme ou relógio.
void acerta_relogio(unsigned char oque)
{
   unsigned char dig_a, dig_b, dig_c, dig_d;
   unsigned char ctrl = 10;

   flg_tmr0 = 0;
   while(!flg_tmr0); //Aguarda sair de uma interrupção.
   d0 = tab_display[traco];
   d1 = tab_display[traco];
   d2 = tab_display[traco];
   d3 = tab_display[traco];
   d4 = tab_display[traco];
   if(oque == relogio)
   {
      d5 = tab_display[h];
      d6 = tab_display[erre];
      rtc_get_time(hora, minuto, segundo);   //Lê o relógio.
      dig_a = (hora >>4);
      dig_b = (hora & 15);
      dig_c = (minuto >>4);
      dig_d = (minuto & 15);
   }else
      if(oque == alarme)
      {
         d5 = tab_display[ahh];
         d6 = tab_display[lhh];
         dig_a = (alarme_hr >>4);
         dig_b = (alarme_hr & 15);
         dig_c = (alarme_min >>4);
         dig_d = (alarme_min & 15);
      }

   while(bt_entra);       //Aguarda soltar o botão.
   delay_ms(300);

   do
   {
      flg_tmr0 = 0;
      while(!flg_tmr0);    //Aguarda sair da interrupção para atualizar valores.
      d0 = tab_display[dig_a];
      d1 = tab_display[dig_b];

      if(bt_entra)
      {
         delay_ms(200);
         if(bt_entra)  ctrl = 0;
         while(bt_entra);
      }

      if(ctrl)
      {
         if(bt_inc)
         {
            if(dig_a <2)
            {
               if(dig_b <9)
               {
                  dig_b++;
               }else
               {
                  dig_b = 0;
                  dig_a++;
               }
            }else
            {
               if(dig_b <3)
               {
                  dig_b++;
               }else
               {
                  dig_a = 0;
                  dig_b = 0;
               }
            }
            while(bt_inc);
            delay_ms(150);
         }//if(!bt_inc)
         else
         if(bt_dec)
         {
            if(dig_a)
            {
               if(dig_b)
               {
                  dig_b--;
               }else
               {
                  dig_a--;
                  dig_b = 9;
               }
            }else
            {
               if(dig_b)
               {
                  dig_b--;
               }else
               {
                  dig_a = 2;
                  dig_b = 3;
               }
            }
            while(bt_dec);
            delay_ms(150);
         }//if(bt_dec)
      }//if(ctrl)
   }while(ctrl);

   ctrl = 10;
   do
   {
      flg_tmr0 = 0;
      while(!flg_tmr0);    //Aguarda sair da interrupção para atualizar valores.
      d3 = tab_display[dig_c];
      d4 = tab_display[dig_d];

      if(bt_entra)
      {
         delay_ms(200);
         if(bt_entra)  ctrl = 0;
         while(bt_entra);
      }
      if(ctrl)
      {
         if(bt_inc)
         {
            if(dig_c <5)
            {
               if(dig_d <9)
               {
                  dig_d++;
               }else
               {
                  dig_d = 0;
                  dig_c++;
               }
            }else
            {
               if(dig_d <9)
               {
                  dig_d++;
               }else
               {
                  dig_c = 0;
                  dig_d = 0;
               }
            }
            while(bt_inc);
            delay_ms(150);
         }//if(bt_inc)
         else
         if(bt_dec)
         {
            if(dig_c)
            {
               if(dig_d)
               {
                  dig_d--;
               }else
               {
                  dig_c--;
                  dig_d = 9;
               }
            }else
            {
               if(dig_d)
               {
                  dig_d--;
               }else
               {
                  dig_c = 5;
                  dig_d = 9;
               }
            }
            while(bt_dec);
            delay_ms(150);
         }//if(bt_dec)
      }//if(ctrl)

   }while(ctrl);

   flg_tmr0 = 0;
   while(!flg_tmr0); //Aguarda sair de uma interrupção.
   d0 = tab_display[nulo];
   d1 = tab_display[traco];
   d2 = tab_display[nulo];
   d3 = tab_display[traco];
   d4 = tab_display[nulo];
   d5 = tab_display[nulo];
   d6 = tab_display[nulo];

   while(!bt_entra && !bt_dec);
   if(bt_entra)
   {
      dig_a <<=4;
      dig_a |= dig_b;
      dig_c <<= 4;
      dig_c |= dig_d;
      if(oque == relogio)  rtc_set_datetime(5, 1, 5, 1, dig_a, dig_c);  //Acerta o RTC.
      else
         {
            alarme_hr   = dig_a;
            alarme_min  = dig_c;
            flg_alarme  = 1;
         }
      while(bt_entra);
      delay_ms(200);
   }else
   if(bt_dec)
   {
      while(bt_dec);
      delay_ms(200);
   }

}//acerta_relogio(void)
//*****************************************************************************
//*****************************************************************************
void byte_bcd(unsigned char n)
	{
		m_temp[2] = (n % 10);		//Unidade.
		m_temp[1] = ((n/10) % 10);	//Dezena.
		m_temp[0] = ((n/100) % 10);	//Centena.
	}


/*
      //TESTE: contador de tempo.

      if(seg <59) seg++;
      else
      {
         seg = 0;
         if(min <59) min++;
         else
         {
            min = 0;
            if(hora <23)   hora++;
            else           hora = 0;
         }
      }

      flg_tmr0 = 0;
      while(!flg_tmr0);
      byte_bcd(hora);
      d0 = tab_display[m_temp[1]];
      d1 = tab_display[m_temp[2]];
      d2 = tab_display[traco];
      byte_bcd(min);
      d3 = tab_display[m_temp[1]];
      d4 = tab_display[m_temp[2]];
      byte_bcd(seg);
      d5 = tab_display[m_temp[1]];
      d6 = tab_display[m_temp[2]];

      delay_ms(995);
*/
