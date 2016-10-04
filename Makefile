all:
	rm -f usb-network-device usb-network-host
	g++ -I /usr/local/include -lusb-1.0 host/main.cpp -o usb-network-host
	g++ -I /usr/local/include  -lusb-1.0 device/main.cpp -o usb-network-device

host:
	rm -f usb-network-host
	g++ -lusb-1.0 host/main.cpp -o usb-network-host

device:
	rm -f usb-network-device
	g++ -lusb-1.0 device/main.cpp -o usb-network-device

clean:
	rm -f usb-network-device usb-network-host
