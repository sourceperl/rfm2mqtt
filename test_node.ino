// lib from https://github.com/rocketscream/Low-Power
#include "LowPower.h"
// lib from https://github.com/LowPowerLab/RFM12B
#include <RFM12B.h>

// You will need to initialize the radio by telling it what ID it has and what network it's on
// The NodeID takes values from 1-127, 0 is reserved for sending broadcast messages (send to all nodes)
// The Network ID takes values from 0-255
// By default the SPI-SS line used is D10 on Atmega328. You can change it by calling .SetCS(pin) where pin can be {8,9,10}
#define NODEID        2  //network ID used for this unit
#define NETWORKID    99  //the network ID we are on
#define GATEWAYID     1  //the node ID we're sending to
#define ACK_TIME     50  // # of ms to wait for an ack
#define SERIAL_BAUD  115200

// Frame type
#define HELLO_FRAME 0x01
#define EVENT_FRAME 0x02
#define BOOL_FRAME  0x03
#define UINT_FRAME  0x04

typedef struct {		
  byte   type;
  byte   var_id;
  int    value; 
} Frame;
Frame frame;

typedef struct {		
  byte   type;
  char   node_name[8];
} HelloFrame;
HelloFrame hello_frame;

unsigned int tick = 0;
unsigned int uint_tick = 0;
char input = 0;

// Need an instance of the Radio Module
RFM12B radio;
byte sendSize=0;

void setup()
{
  Serial.begin(SERIAL_BAUD);
  radio.Initialize(NODEID, RF12_868MHZ, NETWORKID);
  radio.Sleep();
  Serial.println(F("start\n\n"));
}

void loop()
{
  // every 40 ticks: hello message
  if (tick % 40 == 0) { 
    hello_frame.type = HELLO_FRAME;
    strncpy(hello_frame.node_name, "TESTNODE", 8);
    radio.Wakeup();
    radio.Send(GATEWAYID, &hello_frame, sizeof(hello_frame), 0);
    radio.Sleep();
  } 
  
  // every 4 ticks : send uint counter
  if (tick % 4 == 0) { 
    frame.type   = UINT_FRAME;
    frame.var_id = 1;
    frame.value  = uint_tick++;
    radio.Wakeup();
    radio.Send(GATEWAYID, &frame, sizeof(frame), 0);
    radio.Sleep();
  }  
  
  // Enter power down state for 8 s with ADC and BOD module disabled
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);  
  // Update tick counter
  tick++;
}

/*
// wait a few milliseconds for proper ACK, return true if received
static bool waitForAck() {
  long now = millis();
  while (millis() - now <= ACK_TIME)
    if (radio.ACKReceived(GATEWAYID))
      return true;
  return false;
}
*/
