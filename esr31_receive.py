
import serial
from influxdb import InfluxDBClient

HOST = 'localhost'
PORT = 8086
DB_NAME = 'hotwater'
client = None

def esr31_parse(istring, into_dict):
    print(istring)
    try:
        if len(istring) < 2:
            return True
        istring = istring.decode().strip()
        print(istring)
        if istring[0] != "*" or istring[-1] != "*":
            return True
        dfbytes = [int(x, 16) for x in istring[2:-2].split(" ")]
        if dfbytes[0] != 0x70 or dfbytes[1] != 0x8f:
            return True

        checksum = (sum(dfbytes) - dfbytes[-1]) & 0xFF
        if checksum != dfbytes[-1]:
            return True

        fields["SOLART1"] = (float((((dfbytes[3] - 0x20) << 8) | dfbytes[2])) / 10, "temp")
        fields["SOLART2"] = (float((((dfbytes[5] - 0x20) << 8) | dfbytes[4])) / 10, "temp")
        fields["SOLART3"] = (float((((dfbytes[7] - 0x20) << 8) | dfbytes[6])) / 10, "temp")
        fields["SOLARO1"] = (dfbytes[20] & 0x01, "state")
        # TODO: add other fields
        return False
    except Exception as e:
        print(e)
        return True


if __name__ == "__main__":
    notdone = True
    fields = dict()
    json_body = []
    retries_left = 12
    with serial.Serial('/dev/ttyUSB0', 115200, timeout=1) as ser:
        while notdone and retries_left > 1:
            line = ser.readline()
            notdone = esr31_parse(line, fields)
            retries_left -= 1
        print("Done.")
        print(fields)

    for key, value in fields.items():
        json_body.append({"measurement": key, "fields": {value[1]: value[0]}})

    try:
        #print(json_body)
        client = InfluxDBClient(host=HOST, port=PORT, database=DB_NAME, timeout=3)
        client.write_points(json_body)

    except Exception as e: 
        print("Error: " + str(e))

    try:
        #pass
        client.close()
    except Exception as e: 
        print("Error: " + str(e))


