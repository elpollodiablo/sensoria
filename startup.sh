#!/bin/sh

. ./python/bin/activate
python seriallogger.py --mqtt-hostname 192.168.30.2 --mqtt-username mqttuser --mqtt-password mqttpassword --mqtt-topic_prefix tele/sensors --mqtt-client_id arduino --mqtt /dev/ttyUSB0
