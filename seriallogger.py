import serial, six

defaultPort = '/dev/ttyUSB0'

class SerialReader:
    """
    Simple sensor communication class.
    """

    def __init__(self, port, open_connection=True):
        """
        :param string port: path to tty
        :param bool open_connection: should port be opened immediately
        """
        self.port = port
        """TTY name"""
        self.link = None
        """Connection with sensor"""
        if open_connection:
            self.connect()

    def connect(self):
        """
        Open tty connection to sensor
        """
        if self.link is not None:
            self.disconnect()
        self.link = serial.Serial(self.port, 115200, bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE,
                                  stopbits=serial.STOPBITS_ONE, dsrdtr=True, timeout=5, interCharTimeout=0.1)

    def disconnect(self):
        """
        Terminate sensor connection
        """
        if self.link:
            self.link.close()

    def _write_request(self, sequence):
        """
        Write request string to serial
        """
        for byte in sequence:
            self.link.write(six.int2byte(byte))

    def get_status(self):
        """
        Read data from sensor
        :return {ppa, t}|None:
        """
        d = {}
        line = self.link.readline()
        try:
            k, v = line.split(":")
            d[k] = int(v)
        except:
            print(line)
            pass
        return d


if __name__ == "__main__":
    import time
    import sys
    import argparse

    parser = argparse.ArgumentParser(description='Read data from MH-Z14 CO2 sensor.')
    parser.add_argument('tty', default=defaultPort, help='tty port to connect', type=str, nargs='?')
    parser.add_argument('--interval', default=0.1, help='interval between sensor reads, in seconds',
                        type=float, nargs='?')
    parser.add_argument('--mqtt-hostname', default="127.0.0.1", help='mqtt hostname', type=str, nargs='?')
    parser.add_argument('--mqtt-port', default=1883, help='mqtt port', type=int, nargs='?')
    parser.add_argument('--mqtt-username', help='mqtt username', type=str, nargs='?')
    parser.add_argument('--mqtt-password', help='mqtt password', type=str, nargs='?')
    parser.add_argument('--mqtt-topic_prefix', default='tele/mhz14', help='mqtt topic', type=str, nargs='?')
    parser.add_argument('--mqtt-client_id', default='co2reader', help='mqtt client id', type=str, nargs='?')
    parser.add_argument('--mqtt-keepalive', default=60, help='mqtt keepalive time', type=int, nargs='?')
    parser.add_argument('--single', action='store_true', help='single measure mode')
    parser.add_argument('--mqtt', action='store_true', help='mqtt publish mode')
    parser.add_argument('--silent', action='store_true', help='no terminal output')

    args = parser.parse_args()
    port = args.tty

    conn = SerialReader(port, open_connection=True)

    if args.mqtt:
        try:
            import paho.mqtt.client as mqttc
        except:
            print("No paho mqtt library found. Execute `pip install paho-mqtt` to fix this.")
            sys.exit(1)
        mqtt_client = mqttc.Client(client_id = args.mqtt_client_id, clean_session=True)
        if args.mqtt_username:
            mqtt_client.username_pw_set(args.mqtt_username, args.mqtt_password)
        mqtt_client.connect(args.mqtt_hostname, args.mqtt_port, args.mqtt_keepalive)
        mqtt_client.loop_start()

    while True:
        d = conn.get_status()
        for k, v in d.iteritems():
            if not args.silent:
                print("{}\t{}\t{}".format(time.strftime("%Y-%m-%d %H:%M:%S"), k, v))
            if args.mqtt:
                mqtt_topic="{}/{}".format(args.mqtt_topic_prefix, k)
                print(mqtt_topic)
                mqtt_client.publish(mqtt_topic, v)
        else:
            time.sleep(args.interval)
            continue
        sys.stdout.flush()
        if args.single:
            break
    conn.disconnect()
