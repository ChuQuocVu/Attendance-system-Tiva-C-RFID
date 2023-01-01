/**
 * main.c
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/uart.h"
#include "driverlib/ssi.h"
#include "driverlib/pin_map.h"
#include "driverlib/flash.h"
#include "driverlib/eeprom.h"
#include "utils/uartstdio.h"
#include "Library/lcd.h"
#include "Library/Mfrc522.h"


// Setup *****************************************************************************************//

#define CARD_LENGTH 5

#define redLED   0x00000002
#define blueLED  0x00000004
#define greenLED 0x00000008

int chipSelectPin = 0x20;
int NRSTPD = 0x01;
uint8_t Version;
uint8_t AntennaGain;
uint8_t status;
uint32_t readTeste;

unsigned char str[MAX_LEN];
unsigned char card_id[CARD_LENGTH];
char Buff[100];

Mfrc522 Mfrc522(chipSelectPin, NRSTPD);

void UARTGetBuffer(char *pBuff);
void init_LEDs();
void UARTSendData(unsigned char* buffer, int len);
void UARTSendData8bit(unsigned char* buffer, int len);
void lcd_default();
void lcdPrintInformation(unsigned char* buffer, int len);


static void ResetBuffer(char *p)
{
    int i = 0;
    while(*(p+i) != 0x0A)
    {
        *(p + i)= 0;
        i++;
    }
}

void UARTGetBuffer(char *pBuff) //Interrupt
{
    static uint16_t count = 0;
    char check;

    while(UARTCharsAvail(UART0_BASE))
    {
        check = UARTCharGet(UART0_BASE);
        *(pBuff + count) = check;
        count++;
    }

    if(check == 0x0A)
    {
        count = 0;
        lcdPrintInformation((unsigned char*)card_id, CARD_LENGTH);
        ResetBuffer(pBuff);
    }
}

static void UART_ISR(void)
{
    UARTIntClear(UART0_BASE, UARTIntStatus(UART0_BASE, true));
    UARTGetBuffer(&Buff[0]);
}

void init_LEDs()
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);
    GPIOPinTypeGPIOOutput(GPIO_PORTB_BASE, GPIO_PIN_1 | GPIO_PIN_2);
}

void UARTSendData(unsigned char* buffer, int len)
{
    int i;

    for(i=0; i < len; i++)
    {
        UARTprintf("%02x", buffer[i]);
    }

}

void UARTSendData8bit(unsigned char* buffer, int len)
{
    int i;

    for(i=0; i < len; i++)
    {
        UARTprintf("%0x", buffer[i]);
    }

}

void InitConsole(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTStdioConfig(0, 115200, 16000000);
    UARTIntRegister(UART0_BASE, &UART_ISR);
    IntEnable(INT_UART0);
    UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT);
}

void InitSSI(){
    uint32_t junkAuxVar;

    SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI2);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB); //SDA
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF); //reset

    GPIOPinConfigure(GPIO_PB4_SSI2CLK);
    GPIOPinConfigure(GPIO_PB6_SSI2RX);
    GPIOPinConfigure(GPIO_PB7_SSI2TX);

    GPIOPinTypeSSI(GPIO_PORTB_BASE, GPIO_PIN_4 | GPIO_PIN_6 |
                   GPIO_PIN_7);

    GPIOPinTypeGPIOOutput(GPIO_PORTB_BASE, GPIO_PIN_5); //chipSelectPin
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_0); //NRSTPD

    SSIConfigSetExpClk(SSI2_BASE, SysCtlClockGet(), SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, 4000000, 8);

    SSIEnable(SSI2_BASE);

    while(SSIDataGetNonBlocking(SSI2_BASE, &junkAuxVar)){}


}

// ********************************************************** Main **************************************************************//

int main(void)
    {
    SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ | SYSCTL_OSC_MAIN);

    IntMasterDisable();

    InitConsole();
    lcd_init();
    init_LEDs();
    InitSSI();
    Mfrc522.Init();

    GPIOPinWrite(GPIO_PORTB_BASE, chipSelectPin, 0);
    GPIOPinWrite(GPIO_PORTF_BASE, NRSTPD, NRSTPD);

    IntMasterEnable();

    Version = Mfrc522.ReadReg(VersionReg);
    AntennaGain = Mfrc522.ReadReg(PICC_REQIDL) & (0x07<<4);

    lcd_default();

    while(1){
        status = Mfrc522.Request(PICC_REQIDL, str);

        status = Mfrc522.Anticoll(str);

        memcpy(card_id, str, 5); // Save ID to card_id!

        if(status == MI_OK)
        {
            if(card_id[4] >= 16)
            {
                UARTSendData((unsigned char*)card_id, CARD_LENGTH - 1);
            }
            else UARTSendData8bit((unsigned char*)card_id, CARD_LENGTH);

            GPIOPinWrite(GPIO_PORTF_BASE, blueLED, blueLED);
            SysCtlDelay(SysCtlClockGet()/20);
            GPIOPinWrite(GPIO_PORTF_BASE, blueLED, 0);
            SysCtlDelay(SysCtlClockGet()/2); //Delay
            lcd_default();
        }

    }
}
//******************************************************************************************************************************//


void lcdPrintInformation(unsigned char* buffer, int len)
{
    int i;
    lcd_clear();
    lcd_gotoxy(1,1);
    lcd_puts("ID: ");

    if(card_id[4] >= 16)
    {
        for(i=0; i < len-1; i++)
        {
            if((buffer[i]/16) < 10)
                lcd_put_byte(1, buffer[i]/16 + 48);
            else
                lcd_put_byte(1, buffer[i]/16 + 87);
            if((buffer[i]%16) < 10)
                lcd_put_byte(1, buffer[i]%16 + 48);
            else
                lcd_put_byte(1, buffer[i]%16 + 87);
        }
    }
    else
    {
        for(i=0; i < len; i++)
            {
                if((buffer[i]/16) < 10)
                    lcd_put_byte(1, buffer[i]/16 + 48);
                else
                    lcd_put_byte(1, buffer[i]/16 + 87);
                if((buffer[i]%16) < 10)
                    lcd_put_byte(1, buffer[i]%16 + 48);
                else
                    lcd_put_byte(1, buffer[i]%16 + 87);
            }
    }

    // Display name
    lcd_gotoxy(1,0);
    lcd_puts(Buff);
    SysCtlDelay(SysCtlClockGet()/5);

    if(Buff[16] == 'A')
    {
        lcd_clear();
        lcd_gotoxy(0,0);
        lcd_puts("<<   Access   >>");

        GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_2, GPIO_PIN_2);
        SysCtlDelay(SysCtlClockGet()/20);
        GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_2, 0);
        SysCtlDelay(SysCtlClockGet()/20);

        GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_1, GPIO_PIN_1);
        SysCtlDelay(SysCtlClockGet()/2); //Delay
        SysCtlDelay(SysCtlClockGet()/20);
        GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_1, 0);
    }
    else if(Buff[16] == 'D')
    {
        lcd_clear();
        lcd_gotoxy(0,0);
        lcd_puts("<<   Denied   >>");
        // Blink Led 2 times
        for(int i = 0; i <= 1; i++)
        {
            GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_2, GPIO_PIN_2);
            SysCtlDelay(SysCtlClockGet()/20);
            GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_2, 0);
            SysCtlDelay(SysCtlClockGet()/20);
        }
        SysCtlDelay(SysCtlClockGet()/20);
    }
//    lcd_default();
}

void lcd_default()
{
    lcd_clear();
    lcd_gotoxy(0,0);
    lcd_puts("<< Scan ID Card!");
    SysCtlDelay(SysCtlClockGet()/20);
}
















