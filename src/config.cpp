#include <ArduinoJson.h>
#include <AsyncMqttClient.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
// 操作参考 https://blog.csdn.net/qq_41853244/article/details/105682611
#include <EEPROM.h> 

extern AsyncWebServer server;
extern AsyncWebSocket ws;

extern int LED_PIN;

extern int AP_SSID_ADDRESS;
extern int AP_PASSWORD_ADDRESS;
extern int STA_SSID_ADDRESS;
extern int STA_PASSWORD_ADDRESS;

// 作为客户端连接WiFi路由器的默认设置
extern String sta_ssid;
extern String sta_password;

// 作为WiFi热点时的配置
extern String ap_ssid;
extern String ap_password;
extern IPAddress ap_ip;
extern IPAddress ap_gateway;
extern IPAddress ap_netmask;

// mqtt相关的配置
extern AsyncMqttClient mqttClient;
extern String MQTT_HOST;
extern int MQTT_PORT;
// 网关编码
extern String CLIENT_ID;
// 认证信息表的用户名
extern String CLIENT_USERNAME;
// 认证信息用户表的密码
extern String CLIENT_PASSWORD;
extern String MQTT_PUB_TOPIC;
extern String MQTT_SUB_TOPIC_TOPIC;

extern bool connect_mqtt_flag;
extern bool mqtt_connect;
extern void ledCtrl(int pin,bool status);
extern void handGetBoard(AsyncWebServerRequest * request);
extern void handLogin(AsyncWebServerRequest * request);
extern void handleNotFound(AsyncWebServerRequest * request);
extern void onSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
extern void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);

/**
 * 写入数据到eeprom
 */
void writeStringToEEPROM(int addr, const String &str){
  for(unsigned int i = 0; i < str.length(); i++){
    EEPROM.write(addr + i, str[i]);
  }
  EEPROM.write(addr + str.length(), '\0'); // Add null terminator at the end
  EEPROM.commit(); // Commit the changes to EEPROM
}

/**
 * 从eeprom读取数据
*/
String readStringFromEEPROM(int addr){
  String str;
  char c;
  while(1){
    c = EEPROM.read(addr + str.length());
    if(c == '\0') break; // Break if we encounter null terminator
    str += c;
  }
  return str;
}

/**
 * 从eeprom读取数据，没有值，则返回defaultValue
*/
String readStringFromEEPROM(int addr, const String &defaultValue){
  String str;
  char c;
  while(1){
    c = EEPROM.read(addr + str.length());
    if(c == '\0') {
      break; // Break if we encounter null terminator
    }
    str += c;
  }
  return str.isEmpty()?defaultValue:str;
}

/**
 * 清空指定位置的eeprom数据
*/
void clearStringFromEEPROM(const int addr){
  int index = addr;
  while(1){
    if(EEPROM.read(index) == '\0'){
      break;
    }else{
      EEPROM.write(index, '\0');
    }
    index = index + 1;
  }
   EEPROM.commit();
}

/**
 * 硬件中断，抹掉数据
*/
void IRAM_ATTR resetConfig() {
  // 清空现有的数据
  clearStringFromEEPROM(AP_SSID_ADDRESS);
  clearStringFromEEPROM(AP_PASSWORD_ADDRESS);
  clearStringFromEEPROM(STA_SSID_ADDRESS);
  clearStringFromEEPROM(STA_PASSWORD_ADDRESS);
}


/**
 * web服务器设置
*/
void configWebServer(){
  server.serveStatic("/", LittleFS, "/");
  // 获取主页数据
  server.on("/api/home.action",HTTP_GET,handGetBoard);
    // 响应登录接口
  server.on("/api/login.action",HTTP_GET,handLogin);
  server.onNotFound(handleNotFound);
  server.begin();
}

/**
 * 一键配网
*/
void smartConfig(){
  WiFi.mode(WIFI_STA);
  WiFi.setAutoConnect(true);
  // 设置AP网络参数
  // WiFi.softAPConfig(ap_ip,ap_gateway,ap_netmask);
  // // 设置AP账号密码
  // WiFi.softAP(ap_ssid.c_str(),ap_password.c_str());
  Serial.printf("{type:console,message:'wait for smartconfig'}\n");
  WiFi.beginSmartConfig();
  while (1){
    Serial.printf("{type:console,message:'wait for smartconfig ...'}\n");
    ledCtrl(LED_PIN, false);
    delay(500);
    ledCtrl(LED_PIN, true);
    delay(500);
    if (WiFi.smartConfigDone()){
      // 设置自动连接
      WiFi.setAutoConnect(true);
      Serial.printf("{type:console,message:'smartconfig success'}\n");
      sta_ssid = WiFi.SSID().c_str();
      sta_password = WiFi.psk().c_str();
      break;
    }
  }
  writeStringToEEPROM(STA_SSID_ADDRESS, sta_ssid);
  writeStringToEEPROM(STA_PASSWORD_ADDRESS, sta_password);
  Serial.printf("{type:console,message:'connect ssid:%s,password:%s,sta_ip:%s'}\n",sta_ssid.c_str(),sta_password.c_str(),WiFi.localIP());
}

/**
 * 采用AP和STA混合模式
*/
void configApAndSta() {
  Serial.printf("{type:console,message:'Connecting to Wi-Fi...'}\n");
  // 断开连接（防止已连接）
  WiFi.disconnect();
  // 设置成AP和STA混合模式
  WiFi.mode(WIFI_AP_STA);
  // 设置AP网络参数
  WiFi.softAPConfig(ap_ip,ap_gateway,ap_netmask);
  // 设置AP账号密码
  WiFi.softAP(ap_ssid.c_str(),ap_password.c_str());
  Serial.printf("{type:console,meesage:'ap_ssid:%s,connect_ssid:%s,password:%s'}\n",ap_ssid.c_str(),sta_ssid.c_str(),sta_password.c_str());
  // 连接到指定路由器
  WiFi.begin(sta_ssid.c_str(),sta_password.c_str());
  // 设置本地网络参数
  Serial.printf("{type:console,message:'please wait'}\n");
  // 计时函数，20s内没有连接上，直接启用一键配网
  int time = 0;
  while(WiFi.status()!=WL_CONNECTED){
    delay(1000);
    time = time + 1;
    if (time > 30){
      // 启用一键配网
      smartConfig();
      return;
    }
    Serial.printf("{type:console,message:'connect_status...'}\n");
  }
  ledCtrl(LED_PIN,false);
  // WiFi路由器的ip
  // 板子的ip，也就是热点的ip
  Serial.printf("{type:console,message:'sta_ip:%s,ap_ip:%s'}\n",WiFi.localIP().toString().c_str(),WiFi.softAPIP().toString().c_str());
}

/**
 * 采用AP模式
*/
void configAp() {
  Serial.printf("{type:console,message:'Turn On Wi-Fi...'}\n");
  // 断开连接（防止已连接）
  WiFi.disconnect();
  // 设置成A模式
  WiFi.mode(WIFI_AP);
  // 设置AP网络参数
  WiFi.softAPConfig(ap_ip,ap_gateway,ap_netmask);
  // 设置AP账号密码
  WiFi.softAP(ap_ssid.c_str(),ap_password.c_str());
  // 板子的ip，也就是热点的ip
  Serial.printf("{type:console,message:'ap_ssid:%s,ap_ip:%s'}\n",ap_ssid,WiFi.softAPIP().toString().c_str());
  ledCtrl(LED_PIN,false);
}

/**
 * 当与WiFi断开连接后的操作
*/
void onStationModeDisconnected(const WiFiEventStationModeDisconnected& event) {
  ledCtrl(LED_PIN,true);
  // 如果连接丢失，尝试重新连接
  Serial.printf("{type:console,message:'The wifi connection has been disconnected and is trying to reconnect.'}\n");
  // 重联
  configApAndSta();
}

/**
 * 将ESP01连接到MQTT服务器
*/
void connectToMqtt() {
  Serial.printf("{type:console,message:'Connecting to MQTT...'}\n");
  mqttClient.connect();
}

/**
 * 连接成功后执行的操作
*/
void onStationModeConnected(const WiFiEventStationModeConnected&){
  //在与路由器和MQTT代理成功连接后，它将打印ESP32 IP地址
  ledCtrl(LED_PIN,false);
  Serial.printf("{type:console,message:'WiFi connected.IP address: %s'}\n",WiFi.localIP().toString().c_str());
  if(mqtt_connect){
    connectToMqtt();
  }
}


/**
 * 当您将消息发布到MQTT主题时， onMqttPublish（）函数被调用。它在串行监视器中打印数据包ID
*/
void onMqttPublish(uint16_t packetId) {
  Serial.printf("{type:console,message:'Publish acknowledged.packetId: %s'}\n",packetId);
}

/**
 * websocket配置
*/
void configWebSocket() {
  ws.onEvent(onSocketEvent);
  server.addHandler(&ws);
}



/**
 * 与MQTT服务器连接成功后，执行的方法
*/
void onMqttConnect(bool sessionPresent) {
  connect_mqtt_flag = true;
  // 连接成功时订阅主题
  // QoS 0：“最多一次”，消息发布完全依赖底层 TCP/IP 网络。分发的消息可能丢失或重复。例如，这个等级可用于环境传感器数据，单次的数据丢失没关系，因为不久后还会有第二次发送。
  // QoS 1：“至少一次”，确保消息可以到达，但消息可能会重复。
  // QoS 2：“只有一次”，确保消息只到达一次。例如，这个等级可用在一个计费系统中，这里如果消息重复或丢失会导致不正确的收费。
  mqttClient.subscribe(MQTT_SUB_TOPIC_TOPIC.c_str(),2);
  Serial.printf("{type:console,message:'Connected to MQTT.Session present:%s'}\n",sessionPresent);
}


/**
 * ESP01断开与MQTT连接，它将调用 onMqttDisconnect 在串行监视器中打印该消息的功能。
*/
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.printf("{type:console,message:'Disconnected from MQTT.'}\n");
  connect_mqtt_flag = false;
  if (WiFi.isConnected()) {
    connectToMqtt();
  }
}

void configMqtt(){
    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    mqttClient.onPublish(onMqttPublish);
    mqttClient.onMessage(onMqttMessage);
    // 设置连接信息，包括MQTT服务器地址、端口、设备的ID、设备用户名及密码
    mqttClient.setServer(MQTT_HOST.c_str(), MQTT_PORT);
    mqttClient.setClientId(CLIENT_ID.c_str());
    mqttClient.setCredentials(CLIENT_USERNAME.c_str(), CLIENT_PASSWORD.c_str());
}
