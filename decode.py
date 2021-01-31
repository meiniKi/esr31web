
import serial

def esr31_parse(istring):
    print(istring)
    try:
        if len(istring) < 2:
            return 1
        istring = istring.decode().strip()
        print(istring)
        if istring[0] != "*" or istring[-1] != "*":
            return 1
        dfbytes = [int(x, 16) for x in istring[2:-2].split(" ")]
        if dfbytes[0] != 0x70 or dfbytes[1] != 0x8f:
            return 1

        fields = dict()
        fields["T1"] = float((((dfbytes[3] - 0x20) << 8) | dfbytes[2])) / 10
        fields["T2"] = float((((dfbytes[5] - 0x20) << 8) | dfbytes[4])) / 10
        fields["T3"] = float((((dfbytes[7] - 0x20) << 8) | dfbytes[6])) / 10
        print(fields)
        return 0
    except Exception as e:
        print(e)
        return 1


if __name__ == "__main__":
    notdone = 1
    with serial.Serial('/dev/ttyUSB0', 115200, timeout=1) as ser:
        while notdone:
            line = ser.readline()
            notdone = esr31_parse(line)
        print("Done.")
