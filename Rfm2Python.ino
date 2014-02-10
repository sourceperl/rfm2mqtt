// Arduino sketch for set a gateway between RFM12B network and MQTT broquer (via Python script)

/* 
reveive data from a RFM12B module and send it over USB serial like this :
- radio   : "R 02 BE EF" for node 02 send BE EF
- message : "M init radio chip"
- error   : "E CRC"
->  this strings are trivial to parse with Python
*/

// code in public domain

// lib from https://github.com/LowPowerLab/RFM12B
#include <RFM12B.h>

#define NODEID           1  //network ID (central node = 1)
#define NETWORKID       99  //the network ID (default)
#define SERIAL_BAUD 115200

// Need an instance of the Radio Module
RFM12B radio;

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
  // fill in the UART file descriptor with pointer to writer
  fdev_setup_stream (&uartout, uart_putchar, NULL, _FDEV_SETUP_WRITE);
  // standard output device STDOUT is uart
  stdout = &uartout;
  // init radio
  printf(PSTR("M init radio chip\r\n"));
  radio.Initialize(NODEID, RF12_868MHZ, NETWORKID);
}

void loop()
{
  if (radio.ReceiveComplete())
  {
    if (radio.CRCPass())
    {
      printf_P(PSTR("R %02X"), radio.GetSender());
      for (byte i = 0; i < *radio.DataLen; i++) {
        printf_P(PSTR(" %02X"), radio.Data[i]);
      }
      printf_P(PSTR("\r\n"));
      if (radio.ACKRequested())
      {
        radio.SendACK();
        printf(PSTR("M send ACK (request by node)\r\n"));
      }
    }
    else
      printf_P(PSTR("E CRC\r\n"));    
  }
}
