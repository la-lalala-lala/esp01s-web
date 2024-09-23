#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <AsyncMqttClient.h>

extern AsyncWebServer server;
extern AsyncWebSocket ws;

// 定义全局变量
extern char jsonBuffer[256]; // 使用较小的内存缓冲区
extern StaticJsonDocument<256> jsonDoc; // 使用较小的 JSON 文档


/**
 * 登录接口
 */
void handLogin(AsyncWebServerRequest * request){
  AsyncWebServerResponse *response;
  if(request->hasArg("name") && request->hasArg("password")) {
    String name = request->arg("name");
    String password = request->arg("password");
        // 创建要发送的 JSON 数据
    jsonDoc.clear(); // 清空 JSON 文档以便重用
    jsonDoc["name"] = name;
    jsonDoc["password"] = password;
    // 序列化 JSON 数据并发送到串口
    serializeJson(jsonDoc, jsonBuffer, sizeof(jsonBuffer));
    ws.textAll(jsonBuffer);
    response = request->beginResponse(200, "text/json", "{\"code\":0,\"msg\":\"登录成功\"}");
  }else{
    response = request->beginResponse(200, "text/json", "{\"code\":-2,\"msg\":\"缺少参数\"}");
  }
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
}

/**
 * 获取主页信息
 */
void handGetBoard(AsyncWebServerRequest * request){
  AsyncWebServerResponse *response;
  response = request->beginResponse(200, "text/json", "{\"code\":-2,\"msg\":\"缺少参数\"}");
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
}

/**
 * 根据文件后缀获取html协议的返回内容类型
 */
String getContentType(String fileName,AsyncWebServerRequest * request){
  if(request->hasParam("download")){
    return "application/octet-stream";
  }else if(fileName.endsWith(".htm")){
    return "text/html";
  }else if(fileName.endsWith(".html")){
    return "text/html";
  }else if(fileName.endsWith(".css")){
    return "text/css";
  }else if(fileName.endsWith(".js")){
    return "text/javascript";
  }else if(fileName.endsWith(".png")){
    return "image/png";
  }else if(fileName.endsWith(".gif")){
    return "image/gif";
  }else if(fileName.endsWith(".jpg")){
    return "image/jpeg";
  }else if(fileName.endsWith(".ico")){
    return "image/x-icon";
  }else if(fileName.endsWith(".xml")){
    return "text/xml";
  }else if(fileName.endsWith(".pdf")){
    return "application/x-pdf";
  }else if(fileName.endsWith(".zip")){
    return "application/x-zip";
  }else if(fileName.endsWith(".gz")){
    return "application/x-gzip";
  }else{
    return "text/plain";
  }
}

/* NotFound处理 
 * 用于处理没有注册的请求地址 
 * 一般是处理一些页面请求 
 */
void handleNotFound(AsyncWebServerRequest * request){
  String path = request->url();
  Serial.printf("{type:console,load_url:'%s'}\n",path.c_str());
  String contentType = getContentType(path,request);
  String pathWithGz = path + ".gz";
  if(LittleFS.exists(pathWithGz) || LittleFS.exists(path)){
    if(LittleFS.exists(pathWithGz)){
      path += ".gz";
    }
    //Send index.htm as text
    request->send(LittleFS, path, contentType);
    return;
  }
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += request->url();
  message += "\nMethod: ";
  message += (request->method() == HTTP_GET ) ? "GET" : "POST";
  request->send(404, "text/plain", message);
}

/**
 * 控制led
*/
void ledCtrl(int pin,bool status){
  if(status){
    digitalWrite(pin,HIGH);
  }else{
    digitalWrite(pin,LOW);
  }
}

/**
 * 回调监听mqtt
*/
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total){
  Serial.printf("{type:console,message:'mqtt message arrived,topic:%s,message:%s'}\n",topic,payload);
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
      Serial.printf("{type:console,message:'deserializeJson() failed:%s'}\n",error.c_str());
      doc.clear();
      return;
  }
  // 获取功能模式
  String model = doc["model"].as<String>();
  if(!model.isEmpty() && 0 == model.compareTo("led")){
    //bool status = doc["status"].as<bool>();
    // {"model":"led","status":true}
    // 收到数据时的指示灯
    // ledCtrl(test_status_pin,status);
  }
  doc.clear();
}

/**
 * 收到数据
*/
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    if (strcmp((char*)data, "toggle") == 0) {
      ws.textAll((char*)data);
    }
  }
}

/**
 * websocket事件
*/
void onSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      // 连接
      Serial.printf("{type:console,message:'WebSocket client #%u connected from %s'}\n",client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      // 断开连接
      Serial.printf("{type:console,message:'WebSocket client #%u disconnected'}\n",client->id());
      break;
    case WS_EVT_DATA:
      // 接收数据
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}
