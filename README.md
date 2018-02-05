# wemos-D1-sim800l-sensor

pio run -t clean
pio run

esptool -cd nodemcu -cb 921600 -cp /dev/ttyUSB0 -cf .pioenvs/wemos/firmware-d1_mini-0.0.1.bin && pio device monitor -b 115200

curl -H "Content-type: application/x-www-form-urlencoded" -d 'firmware=http://192.168.0.106/firmware-d1_mini-0.0.1.bin' http://192.168.0.136/esp

python /home/palich/bin/decoder.py -e .pioenvs/wemos/firmware-d1_mini-0.0.1.elf ./1.txt
