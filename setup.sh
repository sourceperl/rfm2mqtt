#!/bin/bash

# Python module
sudo pip install paho-mqtt

# bin file
sudo cp usr/local/bin/rfm2mqtt.py /usr/local/bin/

# setup supervisor
sudo apt-get install -y supervisor
sudo cp etc/supervisor/conf.d/rfm2mqtt.conf /etc/supervisor/conf.d/
sudo supervisorctl update
