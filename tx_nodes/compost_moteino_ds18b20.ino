// lib from https://github.com/PaulStoffregen/OneWire
#include <OneWire.h>
// lib from https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <DallasTemperature.h>
// lib from https://github.com/rocketscream/Low-Power
#include "LowPower.h"
// lib from https://github.com/LowPowerLab/RFM12B
#include <RFM12B.h>

// OneWire
#define ONE_WIRE_BUS 8
#define TEMPERATURE_PRECISION 12 // precision : Max 12 bit, min 9 bit (9bit requres 95ms, 10bit 187ms, 11bit 375ms and 12bit resolution takes 750ms)

// You will need to initialize the radio by telling it what ID it has and what network it's on
// The NodeID takes values from 1-127, 0 is reserved for sending broadcast messages (send to all nodes)
// The Network ID takes values from 0-255
// By default the SPI-SS line used is D10 on Atmega328. You can change it by calling .SetCS(pin) where pin can be {8,9,10}
#define NODEID        4  //network ID used for this unit
#define NETWORKID    99  //the network ID we are on
#define GATEWAYID     1  //the node ID we're sending to
#define ACK_TIME     50  // # of ms to wait for an ack
#define SERIAL_BAUD  115200

// Frame type
#define HELLO_FRAME 0x01
#define EVENT_FRAME 0x02
#define BOOL_FRAME  0x03
#define UINT_FRAME  0x04
#define INT_FRAME   0x05
#define FLOAT_FRAME 0x06

typedef struct {
  byte  type;
  byte  var_id;
  float value;
} Frame;
Frame frame;

typedef struct {
  byte type;
  char node_name[8];
} HelloFrame;
HelloFrame hello_frame;

// some vars
OneWire one_wire(ONE_WIRE_BUS);
DallasTemperature sensors(&one_wire);
byte ds_address [8]; // 8 bytes per address
unsigned int tick = 0;
unsigned int uint_tick = 0;
char input = 0;

// Need an instance of the Radio Module
RFM12B radio;
byte sendSize=0;

void setup()
{
  // setup IO
  pinMode(4, OUTPUT);
  // turn DS18B20 on
  digitalWrite(4, HIGH);
  // DS18B20 init
  sensors.begin();
  sensors.setWaitForConversion(false);
  one_wire.search(ds_address);
  // turn DS18B20 off
  digitalWrite(4, LOW);
  // serial init
  Serial.begin(SERIAL_BAUD);
  // radio init
  radio.Initialize(NODEID, RF12_868MHZ, NETWORKID);
  radio.Sleep();
  Serial.println(F("start\n\n"));
}

void loop()
{
  // at startup and every 450 ticks (~ 1 hour): hello message
  if (tick % 450 == 0) {
    hello_frame.type = HELLO_FRAME;
    strncpy(hello_frame.node_name, "TCOMPOST", 8);
    radio.Wakeup();
    radio.Send(GATEWAYID, &hello_frame, sizeof(hello_frame), 0);
    radio.Sleep();
  }
  // every 8 ticks (~ 64 seconds) : send data
  if (tick % 8 == 0) {
    // turn DS18B20 on
    digitalWrite(4, HIGH);
    // DS18B20 init
    sensors.setResolution(ds_address, TEMPERATURE_PRECISION);
    // start conversion
    sensors.requestTemperatures();
    // sleep 1s during conversion (we use ASYNC mode)
    LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);
    float temp = sensors.getTempC(ds_address);
    // turn DS18B20 off
    digitalWrite(4, LOW);
    // init frame
    frame.type   = FLOAT_FRAME;
    frame.var_id = 1;
    frame.value  = temp;
    // send frame
    radio.Wakeup();
    radio.Send(GATEWAYID, &frame, sizeof(frame), 0);
    radio.Sleep();
  }
  // Enter power down state for 8 s with ADC and BOD module disabled
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  // Update tick counter
  tick++;
}
