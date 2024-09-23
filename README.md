## 遇到的问题整理

### 1、初始化固件

https://blog.csdn.net/qq_42250136/article/details/138417786

### 2、代码上传超时

解决办法：Vscode PlatformIO上传固件到ESP-01S失败的解决方法（https://www.xiaoguo123.com/p/platformio-esp01s-upload-err/）

添加一行：
```ini
; ESP01S下载设置
upload_resetmethod = nodemcu
```

完整的配置
```ini

[env:esp01_1m]
platform = espressif8266
board = esp01_1m
framework = arduino
monitor_speed = 115200
upload_port = COM4
monitor_port = COM4
; ESP01S下载设置
upload_resetmethod = nodemcu
```

官方给出参数详解(https://docs.platformio.org/en/latest/platforms/espressif8266.html)
[Reset Method]
You can set custom reset method using upload_resetmethod option from “platformio.ini” (Project Configuration File).

The possible values are:

* ck - RTS controls RESET or CH_PD, DTR controls GPIO0

* wifio - TXD controls GPIO0 via PNP transistor and DTR controls RESET via a capacitor

* nodemcu - GPIO0 and RESET controlled using two NPN transistors as in NodeMCU devkit.

See default reset methods per board.

[env:myenv]
upload_resetmethod = ck

### 3、独立供电问题

esp01 独立供电时，除了 GND和3.3v要接以外，EN引脚也需要接入3.3V