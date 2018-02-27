# wemos-D1-sim800l-sensor

pio run -t clean
pio run

esptool -cd nodemcu -cb 921600 -cp /dev/ttyUSB0 -cf .pioenvs/wemos/firmware-d1_mini-0.0.1.bin && pio device monitor -b 115200

curl -H "Content-type: application/x-www-form-urlencoded" -d 'firmware=http://192.168.0.106/firmware-wemos-si800l-d1_mini-0.0.1.bin' http://192.168.0.124/esp

mosquitto_pub -h 127.0.0.1 -u owntracks -p 8883 -P zhopa -t "cmnd/wemos5/Upgrade" -d -m "1"
mosquitto_pub -h 127.0.0.1 -u owntracks -p 8883 -P zhopa -t "cmnd/wemos5/OtaUrl" -d -m "http://192.168.0.106/firmware-wemos-si800l-d1_mini-0.0.1.bin"

python /home/palich/bin/decoder.py -e .pioenvs/wemos/firmware-d1_mini-0.0.1.elf ./1.txt

curl -H "Content-type: application/x-www-form-urlencoded" --data-urlencode 'cmd=+cclk?' http://192.168.0.124/modem

curl -H "Content-type: application/x-www-form-urlencoded" -d "topic=mqtt/test%26data={\"a\":\"azz\",a}" http://192.168.0.124/gprs && echo ""
