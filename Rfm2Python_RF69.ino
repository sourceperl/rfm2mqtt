// Arduino sketch for set a gateway between RFM69(H)W network and MQTT broquer (via Python script)

/* 
reveive data from a RFM69(H)W module and send it over USB serial like this :
- radio   : "R 02 BE EF" for node 02 send BE EF
- message : "M init radio chip"
- error   : "E CRC"
->  this strings are trivial to parse with Python
*/

// code in public domain

// lib from https://github.com/LowPowerLab/RFM69
#include <RFM69.h>
#include <SPI.h>

#define NODEID      1   //network ID (central node = 1)
#define NETWORKID   100  //the network ID (default)
#define FREQUENCY   RF69_868MHZ
#define IS_RFM69HW      //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define SERIAL_BAUD 115200


// Need an instance of the Radio Module
RFM69 radio;

// link stdout (printf) to Serial object
// create a FILE structure to reference our UART
static FILE uartout = {0};

// create a output function
// This works because Serial.write, although of
// type virtual, already exists.
static int uart_putchar (char c, FILE *stream)
{
  Serial.write(c);
  return 0;
}

void setup()
{
  Serial.begin(SERIAL_BAUD);
  delay(10);
  // fill in the UART file descriptor with pointer to writer
  fdev_setup_stream (&uartout, uart_putchar, NULL, _FDEV_SETUP_WRITE);
  // standard output device STDOUT is uart
  stdout = &uartout;
  // init radio
  printf(PSTR("M init radio chip\r\n"));
  radio.initialize(FREQUENCY, NODEID, NETWORKID);
#ifdef IS_RFM69HW
  radio.setHighPower();
#endif
  radio.promiscuous(false);
}

void loop()
{  
  if (radio.receiveDone())
  {
    printf_P(PSTR("R %02X"), radio.SENDERID);
    for (byte i = 0; i < radio.DATALEN; i++) {
      printf_P(PSTR(" %02X"), radio.DATA[i]);
    }
    printf_P(PSTR("\r\n"));
    if (radio.ACK_REQUESTED)
    {
      radio.sendACK();
      printf(PSTR("M send ACK (request by node)\r\n"));
    }
  }
}
