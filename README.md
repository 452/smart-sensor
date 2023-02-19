# smart-sensor

```shell
# Build project
$ pio run

# Upload firmware
$ pio run --target upload

# Build specific environment
$ pio run -e nodemcuv2

# Upload firmware for the specific environment
$ pio run -e nodemcuv2 --target upload

# Clean build files
$ pio run --target clean
```

esp8266 libraries:
```
#include <MQTT.h> // MQTT by Joel Gaehwiler https://github.com/256dpi/arduino-mqtt
#include <TimeLib.h> // https://github.com/PaulStoffregen/Time
#include <ArduinoJson.h> // arduinojson
Adafruit BME280 Library
```