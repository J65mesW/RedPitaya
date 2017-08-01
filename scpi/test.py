#!/usr/bin/env python3

import argparse

parser = argparse.ArgumentParser(description='SCPI test.')
parser.add_argument('adr',                                  help='provide IP address or URL')
parser.add_argument('-p', '--port', type=int, default=5000, help='specify SCPI port (default is 5000)')
args = parser.parse_args()


import visa

# connect to the instrument
rm = visa.ResourceManager('@py')
#rm.list_resources()
rp = rm.open_resource('TCPIP::{}::{}::SOCKET'.format(args.adr, args.port), read_termination = '\r\n')

###############################################################################
# SCPI core
###############################################################################

# read identification string
print(rp.query("*IDN?"))
# read SCPI standard version
print(rp.query("SYSTem:VERSion?"))

###############################################################################
# generator
###############################################################################

print(rp.query(":OUTPUT2:STATe?"))
print(rp.query(":OUTPUT1:STATe?"))
rp.write(":OUTPUT1:STATe ON")
print(rp.query(":OUTPUT1:STATe?"))

print(rp.query(":SOURce1:MODE?"))
print(rp.query(":SOURce2:MODE?"))
rp.write(":SOURce1:MODE BURST")
print(rp.query(":SOURce1:MODE?"))

rp.write(":SOURce1:FUNCtion:SHAPe SINusoid,30PCT")
rp.write(":SOURce2:FUNCtion:SHAPe TRIangle,0.2")
print(rp.query(":SOURce1:FUNCtion:SHAPe?"))
print(rp.query(":SOURce2:FUNCtion:SHAPe?"))

print(rp.query(":SOURce1:FREQuency:FIXed?"))
print(rp.query(":SOURce2:FREQuency:FIXed?"))
rp.write(":SOURce1:FREQuency:FIXed 1000")
rp.write(":SOURce2:FREQuency:FIXed 2kHz")
print(rp.query(":SOURce1:FREQuency:FIXed?"))
print(rp.query(":SOURce2:FREQuency:FIXed?"))

print(rp.query(":SOURce1:VOLTage:IMMediate:AMPlitude?"))
print(rp.query(":SOURce2:VOLTage:IMMediate:AMPlitude?"))
rp.write(":SOURce1:VOLTage:IMMediate:AMPlitude 0.2")
rp.write(":SOURce2:VOLTage:IMMediate:AMPlitude 600 mV")
print(rp.query(":SOURce1:VOLTage:IMMediate:AMPlitude?"))
print(rp.query(":SOURce2:VOLTage:IMMediate:AMPlitude?"))

print(rp.query(":SOURce1:VOLTage:IMMediate:OFFSet?"))
print(rp.query(":SOURce2:VOLTage:IMMediate:OFFSet?"))
rp.write(":SOURce1:VOLTage:IMMediate:OFFSet -0.5")
rp.write(":SOURce2:VOLTage:IMMediate:OFFSet +0.5")
print(rp.query(":SOURce1:VOLTage:IMMediate:OFFSet?"))
print(rp.query(":SOURce2:VOLTage:IMMediate:OFFSet?"))

print(rp.query("SOURce1:TRACe:DATA:DATA? 16"))
print(rp.query("SOURce2:TRACe:DATA:DATA? 16"))
rp.write("SOURce1:TRACe:DATA:DATA 0.1, 0.2, 0.3, 0.4")
print(rp.query("SOURce1:TRACe:DATA:DATA? 16"))
print(rp.query("SOURce2:TRACe:DATA:DATA? 16"))

err = int(rp.query(":SYSTem:ERRor:COUNt?"))
for i in range(err):
    print(rp.query(":SYSTem:ERRor:NEXT?"))

# close instrument connection
rp.close()
