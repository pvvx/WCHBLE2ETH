#!/usr/bin/env python3

# adv2ethernet.py 17.02.2024 pvvx #

import sys
import socket
from pynput import keyboard

def on_press(key):
    if key == keyboard.Key.esc:
        return False

def main():
	if(len(sys.argv) < 2):
		print("Usage: adv2eth <IP address device or url>")
		sys.exit(1)
	if(sys.argv[1] == "-h"):
		print("Usage: adv2eth <IP address device or url>")
		sys.exit(0)
	print("Press 'ESC' to exit")
	print ('Connecting to '+sys.argv[1]+' ...')
	sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)  # Create a TCP/IP socket
	sock.connect((sys.argv[1],1000))  # connect to the server
	data = bytes(0)
	with keyboard.Listener(on_press=on_press) as listener:
		while listener.running:
			data += sock.recv(1460*2) # TCP MSS * 2 receive response
			#print('Received from server: %d' % len(data))
			while(data[0] + 10 < len(data)):
				l = data[0]
				evt = data[1:2].hex() 
				adt = data[2:3].hex() 
				rssi = data[3:4].hex()
				xmac = bytes([data[9], data[8], data[7], data[6], data[5], data[4]])
				mac = xmac.hex() 
				dump = data[10:l+10].hex()
				print(evt, adt, rssi, mac, dump)
				data = data[l + 10:]
	sock.close()  # close the connection
	sys.exit(0)

if __name__ == '__main__':
	main()
