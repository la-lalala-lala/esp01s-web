#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <AsyncMqttClient.h>
#include <EEPROM.h>
/**
 * ESPAsyncWebServer库
 * https://github.com/me-no-dev/ESPAsyncWebServer
 * https://blog.csdn.net/T_infinity/article/details/105447479
 * https://blog.csdn.net/qq_43454310/article/details/114824338
 */
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ESP-01模块上只有两个GPIO可用，而且其中一个GPIO0还用于启动时的引导选择，因此在使用中断时应该小心。
// https://www.yiboard.com/thread-1836-1-1.html
int INTERRUPT_PIN = 0;
// LED指示灯
int LED_PIN = 2;
// 申请的空间大小
int EEPROM_SIZE = 256;
// 地址指针
int AP_SSID_ADDRESS = 0;
int AP_PASSWORD_ADDRESS = 32;
int STA_SSID_ADDRESS = 64;
int STA_PASSWORD_ADDRESS = 96;

// 作为客户端连接WiFi路由器的默认设置
String sta_ssid = "shmily";
String sta_password = "saya.ac.cn.666";

// 作为WiFi热点时的配置
String ap_ssid = "iot_gateway_1024";//建立AP名字
String ap_password = "123456789";//建立AP的密码
IPAddress ap_ip(192,168,4,1);//AP端IP
IPAddress ap_gateway(192,168,4,1);//AP端网关
IPAddress ap_netmask(255,255,255,0);//AP端子网掩码


//是否需要连接路由器（默认不连接）
bool router_connect = true;
// 是否已经连接上路由器
bool connect_router_flag = false;
//是否需要连接mqtt（默认不连接）
bool mqtt_connect = false;
// 是否已经连接上MQTT服务器
bool connect_mqtt_flag = false;

// mqtt相关的配置
AsyncMqttClient mqttClient;
String MQTT_HOST = "1.15.81.148";
int MQTT_PORT = 1883;
// 网关编码
String CLIENT_ID = "esp32-001";
// 认证信息表的用户名
String CLIENT_USERNAME  = "iCaNt2344m4KZbZb";
// 认证信息用户表的密码
String CLIENT_PASSWORD = "123456";
String MQTT_PUB_TOPIC = "/iot/ack/"+CLIENT_ID;
String MQTT_SUB_TOPIC_TOPIC = "/iot/listener/"+CLIENT_ID;

// 定义全局变量
char jsonBuffer[256]; // 使用较小的内存缓冲区
StaticJsonDocument<256> jsonDoc; // 使用较小的 JSON 文档

extern void writeStringToEEPROM(int addr, const String &str);
extern String readStringFromEEPROM(int addr, const String &defaultValue);
extern void IRAM_ATTR resetConfig();
extern void onStationModeDisconnected(const WiFiEventStationModeDisconnected& event);
extern void onStationModeConnected(const WiFiEventStationModeConnected&);
extern void configWebSocket();
extern void ledCtrl(int pin,bool status);
extern void configApAndSta();
extern void configAp();
extern void configWebServer();

void setup(){
  Serial.begin(115200);
  pinMode(INTERRUPT_PIN, INPUT_PULLUP);
  attachInterrupt(INTERRUPT_PIN, resetConfig, FALLING);

  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  ap_ssid = readStringFromEEPROM(AP_SSID_ADDRESS,ap_ssid);
  ap_password = readStringFromEEPROM(AP_PASSWORD_ADDRESS,ap_password);
  sta_ssid = readStringFromEEPROM(STA_SSID_ADDRESS,sta_ssid);
  sta_password = readStringFromEEPROM(STA_PASSWORD_ADDRESS,sta_password);
  Serial.printf("{type:console,message:'ap_ssid:%s,ap_password:%s,sta_ssid:%s,sta_password:%s'}\n", ap_ssid.c_str(),ap_password.c_str(),sta_ssid.c_str(),sta_password.c_str());
  // 初始化led引脚，没有初始化完成前常亮
  pinMode(LED_PIN, OUTPUT);
  ledCtrl(LED_PIN,true);
  LittleFS.begin();
  if(router_connect){
    // 混合模式
    if(mqtt_connect){
      WiFi.onStationModeDisconnected(onStationModeDisconnected);
      WiFi.onStationModeConnected(onStationModeConnected);
    }
    // 网络参数配置
    configApAndSta();
  }else{
    configAp();
  }
  // 设置web服务器
  configWebServer();
  // 设置socket服务器
  configWebSocket();
  Serial.printf("{type:console,message:'http server started'}\n");
}

void loop(){
  // 关闭过多的websocket，以便节省资源
  ws.cleanupClients();
}


  // if (Serial.available() > 0) {
  //   // 读取数据并存储到缓冲区中
  //   int bytesRead = Serial.readBytesUntil('\n', jsonBuffer, sizeof(jsonBuffer));

  //   // 解析 JSON 数据
  //   DeserializationError error = deserializeJson(jsonDoc, jsonBuffer, bytesRead);
  //   if (error) {
  //     Serial.print("JSON parsing failed: ");
  //     Serial.println(error.c_str());
  //     return;
  //   }

  //   // 从 JSON 数据中获取特定字段的值
  //   int value = jsonDoc["sensorValue"];

  //   // 处理数据，这里只是简单的打印出来
  //   Serial.print("Received sensor value: ");
  //   Serial.println(value);

  //   // 创建要发送的 JSON 数据
  //   jsonDoc.clear(); // 清空 JSON 文档以便重用
  //   jsonDoc["status"] = "OK";
  //   jsonDoc["message"] = "Data received successfully";

  //   // 序列化 JSON 数据并发送到串口
  //   size_t bytesWritten = serializeJson(jsonDoc, jsonBuffer, sizeof(jsonBuffer));
  //   Serial.write(jsonBuffer, bytesWritten);
  //   Serial.println(); // 发送一个换行符以便于接收端处理
  // }