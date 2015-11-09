// little weather station : send pressure and temperature to central node (ID#1)
// Hardware is : Moteino with RFM12B radio ship + BMP180

#include <Wire.h>
// lib from https://github.com/adafruit/Adafruit_Sensor
#include <Adafruit_Sensor.h>
// lib from https://github.com/adafruit/Adafruit_BMP085_Unified.git (work for BMP085 and BMP180)
#include <Adafruit_BMP085_U.h>
// lib from https://github.com/rocketscream/Low-Power
#include "LowPower.h"
// lib from https://github.com/LowPowerLab/RFM12B
#include <RFM12B.h>

// You will need to initialize the radio by telling it what ID it has and what network it's on
// The NodeID takes values from 1-127, 0 is reserved for sending broadcast messages (send to all nodes)
// The Network ID takes values from 0-255
// By default the SPI-SS line used is D10 on Atmega328. You can change it by calling .SetCS(pin) where pin can be {8,9,10}
#define NODEID       18  //network ID used for this unit
#define NETWORKID    99  //the network ID we are on
#define GATEWAYID     1  //the node ID we're sending to
#define ACK_TIME     50  // # of ms to wait for an ack
#define SERIAL_BAUD  115200

// Frame type
#define HELLO_FRAME  0x01
#define EVENT_FRAME  0x02
#define BOOL_FRAME   0x03
#define UINT_FRAME   0x04
#define INT_FRAME    0x05
#define FLOAT_FRAME  0x06

typedef struct {
  byte   type;
  byte   var_id;
  int    value; 
} Frame_int;

typedef struct {
  byte   type;
  byte   var_id;
  float    value; 
} Frame_float;
Frame_float frame;

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
// Sensor access
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);

/**************************************************************************/
/*
    Arduino setup function (automatically called at startup)
*/
/**************************************************************************/
void setup(void) 
{
  Serial.begin(9600);
  radio.Initialize(NODEID, RF12_868MHZ, NETWORKID);
  radio.Sleep();
  Serial.println(F("start\n\n"));
  
  /* Initialise the sensor */
  if(!bmp.begin())
  {
    //problem detecting the BMP085
    // Enter power down state
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF); 
  }
}

/**************************************************************************/
/*
    Arduino loop function, called once 'setup' is complete (your own code
    should go here)
*/
/**************************************************************************/
void loop(void)
{
    // at startup and every 450 ticks (~ 1 hour): hello message
  if (tick % 450 == 0) {
    hello_frame.type = HELLO_FRAME;
    strncpy(hello_frame.node_name, "WEATHER ", 8);
    radio.Wakeup();
    radio.Send(GATEWAYID, &hello_frame, sizeof(hello_frame), 0);
    radio.Sleep();
  } 
  
  // every 8 ticks (~ 64 seconds) : send data
  if (tick % 8 == 0) {
    /* Get a new sensor event */
    sensors_event_t event;
    bmp.getEvent(&event);

    /* Display the results (barometric pressure is measure in hPa) */
    if (event.pressure) {
      // read temperature
      float temperature;
      bmp.getTemperature(&temperature);
      /* RADIO */
      // VAR ID 1 : send pressure (in hPa * 100)
      frame.type   = FLOAT_FRAME;
      frame.var_id = 1;
      frame.value  = event.pressure;
      radio.Wakeup();
      radio.Send(GATEWAYID, &frame, sizeof(frame), 0);

      // VAR ID 2 : send temperature (in °C * 100)
      frame.type   = FLOAT_FRAME;
      frame.var_id = 2;
      frame.value  = temperature;
      radio.Send(GATEWAYID, &frame, sizeof(frame), 0);
      radio.Sleep();
    }
  }
  
  // Enter power down state for 8 s with ADC and BOD module disabled
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  // Update tick counter
  tick++;
}
