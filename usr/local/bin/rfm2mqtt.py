#!/usr/bin/env python
# -*- coding: utf-8 -*-

# script based on this wonderful repo :
# https://github.com/kylegordon/mqtt-rfm12b.git

# share, it's happiness !

#TODO
# use struct pack/unpack to decode frame (more efficiency and devel friendly)

import os
import logging
import logging.handlers
import signal
import socket
import string
import struct
import time
import serial
import sys
import paho.mqtt.client as paho
from datetime import datetime
import pytz
import json

# Constant
LOG_LEVEL = logging.INFO
SERIAL_DEVICE = "/dev/ttyUSB0"
SERIAL_BAUD = 115200
MQTT_HOST = "127.0.0.1"
MQTT_PORT = 1883
MQTT_ROOT_TOPIC = "rfm12/"

# Frame type
(
RESERVED_FRAME,
HELLO_FRAME,
EVENT_FRAME,
BOOL_FRAME,
UINT_FRAME,
INT_FRAME,
FLOAT_FRAME,
) = range(7)

# Global functions
def iso_format(dt):
    """ return an ISO string (at zulu time) from a datetime object

    :param dt: a datetime object (naive or not)
    :type dt: datetime
    :return: datetime as ISO string
    :rtype: str
    """
    # naive is UTC other is convert to UTC
    if dt.tzinfo is None:
        dt = pytz.utc.localize(dt)
    else:
        dt = dt.astimezone(pytz.utc)
    # convert dt to isostring at zero meridian time
    isostring = datetime.strftime(dt, '%Y-%m-%dT%H:%M:%S.{0:03d}Z')
    return isostring.format(int(round(dt.microsecond/1000.0)))

# Exception class
class FrameDecodeError(Exception):
  def __init__(self, msg):
    self.msg = msg
  def __str__(self):
    return repr(self.msg)

# MQTT callbacks
def on_publish(mosq, obj, mid):
  """
  What to do when a message is published
  """
  logging.debug("MID " + str(mid) + " published.")


def on_subscribe(mosq, obj, mid, qos_list):
  """
  What to do in the event of subscribing to a topic"
  """
  logging.debug("Subscribe with mid " + str(mid) + " received.")


def on_unsubscribe(mosq, obj, mid):
  """
  What to do in the event of unsubscribing from a topic
  """
  logging.debug("Unsubscribe with mid " + str(mid) + " received.")


def on_connect(mosq, obj, result_code):
  """
  Handle connections (or failures) to the broker.
  This is called after the client has received a CONNACK message
  from the broker in response to calling connect().
  The parameter rc is an integer giving the return code:

  0: Success
  1: Refused – unacceptable protocol version
  2: Refused – identifier rejected
  3: Refused – server unavailable
  4: Refused – bad user name or password (MQTT v3.1 broker only)
  5: Refused – not authorised (MQTT v3.1 broker only)
  """
  logging.debug("on_connect RC: " + str(result_code))
  if result_code == 0:
    logging.info("Connected to %s:%s", MQTT_HOST, MQTT_PORT)
    process_connection()
  elif result_code == 1:
    logging.info("Connection refused - unacceptable protocol version")
    cleanup()
  elif result_code == 2:
    logging.info("Connection refused - identifier rejected")
    cleanup()
  elif result_code == 3:
    logging.info("Connection refused - server unavailable")
    logging.info("Retrying in 30 seconds")
    time.sleep(30)
  elif result_code == 4:
    logging.info("Connection refused - bad user name or password")
    cleanup()
  elif result_code == 5:
    logging.info("Connection refused - not authorised")
    cleanup()
  else:
    logging.warning("Something went wrong. RC:" + str(result_code))
    cleanup()


def on_disconnect(mosq, obj, result_code):
  """
  Handle disconnections from the broker
  """
  if result_code == 0:
    logging.info("Clean disconnection")
  else:
    logging.info("Unexpected disconnection! Reconnecting in 5 seconds")
    logging.debug("Result code: %s", result_code)
    time.sleep(5)


def on_message(mosq, obj, msg):
  """
  What to do when the client recieves a message from the broker
  """
  logging.debug("Received: " + msg.payload +
                " received on topic " + msg.topic +
                " with QoS " + str(msg.qos))
  process_message(msg)


def on_log(mosq, obj, level, string):
  """
  What to do with debug log output from the MQTT library
  """
  logging.debug(string)

# End of MQTT callbacks


def cleanup(signum, frame):
  """
  Signal handler to ensure we disconnect cleanly
  in the event of a SIGTERM or SIGINT.
  """
  logging.info("Disconnecting from broker")
  mq.disconnect()
  mq.loop_stop()
  logging.info("Exiting on signal %d", signum)
  sys.exit(signum)


def connect():
  """
  Connect to the broker, define the callbacks, and subscribe
  This will also set the Last Will and Testament (LWT)
  The LWT will be published in the event of an unclean or
  unexpected disconnection.
  """
  logging.debug("Connecting to %s:%s", MQTT_HOST, MQTT_PORT)
  result = mq.connect(MQTT_HOST, str(MQTT_PORT), 60)
  if result != 0:
    logging.info("Connection failed with error code %s. Retrying", result)
    time.sleep(10)
    connect()

  # Define the callbacks
  mq.on_connect = on_connect
  mq.on_disconnect = on_disconnect
  mq.on_publish = on_publish
  mq.on_subscribe = on_subscribe
  mq.on_unsubscribe = on_unsubscribe
  mq.on_message = on_message
  mq.on_log = on_log

  mq.loop_start()


def process_connection():
  """
  What to do when a new connection is established
  """
  logging.debug("Processing connection")

def process_message(msg):
  """
  What to do with the message that's arrived
  """
  logging.debug("Received: %s", msg.topic)


def open_serial(port, speed):
  """
  Open the serial port and flush any waiting input.
  """
  global ser
  try:
    logging.info("Connecting to " + port + " at " + speed + " baud")
    ser = serial.Serial(port, speed)
    ser.flushInput()
  except:
    logging.warning("Unable to connect to " +
                    port  + " at " +
                    speed + " baud")
    raise SystemExit

def main_loop():
  """
  The main loop in which we stay connected to the broker
  """
  global nodes
  while True:
    # Read for serial input, and split into values
    msg = ser.readline()
    items = msg.split()
    try:
      logging.debug("items list is %s", items)
      # it's a receive frame
      if (items[0] == "R"):
        # decode fix header
        # node id : value between 2 and 127
        node_id    = int(items[1], 16)
        if (node_id <= 1) or (node_id > 127):
          raise FrameDecodeError("node ID not in 2 to 127 interval")
        # frame type : value between 1 and 4
        frame_type = int(items[2], 16)
        if (frame_type < 1) or (frame_type > 6):
          raise FrameDecodeError("frame type not in 1 to 6 interval")
        # publish lastseen for this node
        nodes[node_id]['last_seen'] =  iso_format(datetime.utcnow())
        # decode variable header
        if (frame_type == HELLO_FRAME):
          if (len(items[3:]) != 8):
            raise FrameDecodeError("name must be 8 chars length in hello frame")
          # convert hex list to str ['4D'..., '41'] -> 'M...A'
          node_name = "".join([chr(int(hex_char,16)) for hex_char in items[3:]])
          if (not all(c in string.printable for c in node_name)):
            raise FrameDecodeError("name char must be printable in hello frame")
          # publish node name
          nodes[node_id]['name'] = node_name
          nodes[node_id]['last_hello'] = nodes[node_id]['last_seen']
        elif (frame_type == EVENT_FRAME):
          # event_id : value between 1 and 255
          event_id = int(items[3], 16)
          if (event_id == 0):
            raise FrameDecodeError("event id not in 1 to 255 interval")
          # publish event
          mq.publish(MQTT_ROOT_TOPIC + str(node_id) + "/evt",
                     str(event_id))
        elif (frame_type == BOOL_FRAME):
          # bool_id : value between 1 and 255
          bool_id = int(items[3], 16)
          if (bool_id == 0):
            raise FrameDecodeError("bool id not in 1 to 255 interval")
          # bool_val : must be 0x00 for false or 0x01 for true
          if (int(items[4], 16) == 1):
            bool_val = "1"
          else:
            bool_val = "0"
          # publish bool
          mq.publish(MQTT_ROOT_TOPIC + str(node_id) + "/bool/" + str(bool_id),
                     bool_val)
        elif (frame_type == UINT_FRAME):
          # uint_id : value between 1 and 255
          uint_id = int(items[3], 16)
          if (uint_id == 0):
            raise FrameDecodeError("uint id not in 1 to 255 interval")
          # uint_val :
          uint_val = (int(items[5], 16) << 8) + int(items[4], 16)
          # publish bool
          mq.publish(MQTT_ROOT_TOPIC + str(node_id) + "/uint/" + str(uint_id),
                     uint_val)
        elif (frame_type == INT_FRAME):
          # int_id : value between 1 and 255
          int_id = int(items[3], 16)
          if (int_id == 0):
            raise FrameDecodeError("int id not in 1 to 255 interval")
          # int_val :
          int_val = (int(items[5], 16) << 8) + int(items[4], 16)
          # two's complement
          if((int_val&(1<<15)) != 0):
            int_val = int_val - (1<<16)
          # publish bool
          mq.publish(MQTT_ROOT_TOPIC + str(node_id) + "/int/" + str(int_id),
                     int_val)
        elif (frame_type == FLOAT_FRAME):
          # float_id : value between 1 and 255
          float_id = int(items[3], 16)
          if (float_id == 0):
            raise FrameDecodeError("float id not in 1 to 255 interval")
          # float_val :
          float_val = ((int(items[7], 16) << 24) + (int(items[6], 16) << 16) +
                       (int(items[5], 16) << 8) + (int(items[4], 16)))
          # float convertion
          float_val = struct.unpack('f', struct.pack('I', float_val))[0]
          # publish bool
          mq.publish(MQTT_ROOT_TOPIC + str(node_id) + "/float/" + str(float_id),
                     float_val)
        # update info node topic
        mq.publish(MQTT_ROOT_TOPIC + str(node_id) + "/infos",
                   json.dumps(nodes[node_id]), retain=True)
      # it's a message
      elif (items[0] == "M"):
        do_nothing = 1
      # it's a error report
      elif (items[0] == "E"):
        do_nothing = 1
    except (IndexError, ValueError):
      logging.debug("except IndexError or ValueError occur, skip frame")
    except FrameDecodeError as e_msg:
      logging.info("frame decode error : " + str(e_msg))

# Global vars
nodes = {}
# init nodes dict
for id in range(1,128):
    nodes[id] = {'name': '', 'last_seen': '', 'last_hello': ''}
# mqtt client ID
client_id = "rfm2mqtt_%d" % os.getpid()
mq = paho.Client(client_id)

# set log option
logger = logging.getLogger()
logger.setLevel(LOG_LEVEL)
handler = logging.handlers.SysLogHandler(address='/dev/log')
handler.setFormatter(logging.Formatter('%(module)s: %(message)s'))
logger.addHandler(handler)

# log startup
logging.info("start")

# Use the signal module to handle signals
signal.signal(signal.SIGTERM, cleanup)
signal.signal(signal.SIGINT, cleanup)

# Connect to the broker, open the serial port, and enter the main loop
open_serial(SERIAL_DEVICE, str(SERIAL_BAUD))
connect()
# Try to start the main loop
try:
  main_loop()
except KeyboardInterrupt:
  logging.info("Interrupted by keypress")
  sys.exit(0)
