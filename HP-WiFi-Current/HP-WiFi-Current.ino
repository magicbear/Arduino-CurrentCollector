#define MQTT_MAX_PACKET_SIZE 384

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

#include <math.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <EEPROM.h>

#ifndef STASSID
#define STASSID "WIFI-SSID"
#define STAPSK  "WIFI-PSK"
#define MQTT_CLASS "HP-CURRENT"
#define VERSION "1.0"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;
const char* mqtt_server = "MQTT-SRV.bilibili.com";//服务器的地址 
const int port = 1883;//服务器端口号
char mqtt_cls[32];
char msg_buf[256];
char serial_buf[256];
int serial_len = 0;

WiFiClient espClient;
PubSubClient client(espClient);

char last_state = -1;
char set_state = 1;
long last_rssi = -1;
unsigned long last_send_rssi;
bool otaMode = true;                             //OTA mode flag

int samples = 0;
int freq_A = 0, freq_B = 0, freq_C = 0;
float DC_A = 0, DC_B = 0, DC_C = 0;
float AC_A = 0, AC_B = 0, AC_C = 0;
float Current_A = 0, Current_B = 0, Current_C = 0;

void setup() {
  // put your setup code here, to run once:
  pinMode(0, OUTPUT);
  digitalWrite(0, set_state == 0 ? 1 : 0);

  Serial.begin(115200);
  Serial1.begin(115200);
  
  byte mac[6];
  WiFi.macAddress(mac);
  sprintf(mqtt_cls, MQTT_CLASS"-%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  WiFi.mode(WIFI_STA);
//  wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
  WiFi.begin(ssid, password);

  Serial.println();
  Serial.printf("Flash: %d\n", ESP.getFlashChipRealSize());
  Serial.printf("Version: %s\n", VERSION);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  
  Serial.print("Connected to wifi. My address:");
  IPAddress myAddress = WiFi.localIP();
  Serial.println(myAddress);  
  
  WiFi.setSleepMode(WIFI_LIGHT_SLEEP, 1000);
  client.setServer(mqtt_server, port);//端口号
  client.setCallback(callback); //用于接收服务器接收的数据

  EEPROM.begin(256);
  for (int i = 0; i < sizeof(MQTT_CLASS) - 1; i++)
  {
    if (EEPROM.read(i) != mqtt_cls[i])
    {
      initEEPROM();
      break;
    }
  }
}


void initEEPROM()
{
  for (int i = 0; i < sizeof(MQTT_CLASS) - 1; i++)
  {
    EEPROM.write(i, mqtt_cls[i]);
  }
  EEPROM.write(16, 0);
  EEPROM.write(64, 0);
  EEPROM.commit();
}

char *loadEEPName(char *buffer)
{
    uint8_t len = EEPROM.read(16);
    for (uint8_t i = 0; i < len; i++)
    {
        buffer[i] = EEPROM.read(i + 17);
    }
    return buffer + len;
}

char *loadEEPData(char *buffer)
{
    uint8_t len = EEPROM.read(64);
    for (uint8_t i = 0; i < len; i++)
    {
        buffer[i] = EEPROM.read(i + 65);
    }
    return buffer + len;
}

void callback(char* topic, byte* payload, unsigned int length) {//用于接收数据
  int l=0;
  int p=1;
  if (strcmp(topic, "ota") == 0)
  {
    WiFiClient ota_client;

    char firmware_url[128];
    memcpy(firmware_url, payload, length);
    firmware_url[length] = 0;
    Serial.println("Start OTA...");
    sprintf(msg_buf, "{\"ota\":true}");
    client.publish("status", msg_buf);
    t_httpUpdate_return ret = ESPhttpUpdate.update(ota_client, firmware_url);
    // Or:
    //t_httpUpdate_return ret = ESPhttpUpdate.update(client, "server", 80, "file.bin");

    switch (ret) {
      case HTTP_UPDATE_FAILED:
        sprintf(msg_buf, "{\"ota\":\"%s\"}", ESPhttpUpdate.getLastErrorString().c_str());
        client.publish("status", msg_buf);
        Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
        break;

      case HTTP_UPDATE_NO_UPDATES:
        sprintf(msg_buf, "{\"ota\":\"no updates\"}");
        client.publish("status", msg_buf);
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        break;

      case HTTP_UPDATE_OK:
        sprintf(msg_buf, "{\"ota\":\"success\"}");
        client.publish("status", msg_buf);
        Serial.println("HTTP_UPDATE_OK");
        break;
    }
  } else if (strcmp(topic, "setName") == 0)
  {
    if (length > 64 - 21 - EEPROM.read(64))
    {
      length = 64 - 21 - EEPROM.read(64);
    }
    EEPROM.write(16, length);
    for (int i = 0; i < length;i++)
    {
      EEPROM.write(17+i, payload[i]);
    }
    EEPROM.commit();
    sendMeta();
  } else if (strcmp(topic, "setData") == 0)
  {
    if (length > 64 - 21 - EEPROM.read(16))
    {
      length = 64 - 21 - EEPROM.read(16);
    }
    EEPROM.write(64, length);
    for (int i = 0; i < length;i++)
    {
      EEPROM.write(65+i, payload[i]);
    }
    EEPROM.commit();
    sendMeta();
  }
//  Serial.print("payload topic: ");
//  Serial.println(topic);
//  Serial.print("payload length: ");
//  Serial.println(length);
//  Serial.println((char *)payload);
}

void sendMeta()
{
  // max length = 64 - 21
    char *p = msg_buf + sprintf(msg_buf, "{\"name\":\"");
    p = loadEEPName(p);
    p = p + sprintf(p, "\",\"data\":\"");
    p = loadEEPData(p);
    p = p + sprintf(p, "\"}");    
    client.publish("dev", msg_buf);
}

void reconnect() {//等待，直到连接上服务器
  while (!client.connected()) {//如果没有连接上
    if (client.connect(mqtt_cls)) {//接入时的用户名，尽量取一个很不常用的用户名
      Serial.println("Connect to MQTT server Success!");
      sendMeta();
      client.subscribe("ota");//接收外来的数据时的intopic
      client.subscribe("setName");
      client.subscribe("setData");
      last_rssi = -1;
      last_state = -1;
    } else {
      Serial.print("failed, rc=");//连接失败
      Serial.print(client.state());//重新连接
      Serial.println(" try again in 5 seconds");//延时5秒后重新连接
      delay(5000);
    }
  }
}

float readFloat(char *serial_buf, int serial_len, int *i)
{
    float rc = atof(serial_buf+*i);
    while (++*i < serial_len && serial_buf[*i] != '/' && serial_buf[*i] != ' ');
    return rc;
}

void loop() {
   reconnect();//确保连上服务器，否则一直等待。
   client.loop();//MUC接收数据的主循环函数。
//   Serial.println("sending message");

   long rssi = WiFi.RSSI();
   while (Serial.available())
   {
      serial_buf[serial_len++] = Serial.read();
      if (serial_len >= sizeof(serial_buf))
      {
          serial_len = 0;
          Serial.println("Warning: Buffer Overflow");
          Serial.println(serial_buf);
          continue;
      }
      if (serial_buf[serial_len-1] == '\n')
      {
          serial_buf[serial_len] = '\0';
          for (int i = 0; i < serial_len; i++)
          {
              if (strncmp(serial_buf + i, "Samples: ", 9) == 0) {
                  samples = atoi(serial_buf+i+9);                  
                  i += 9 + ceil(log10(samples));   // One more for skip space
                  continue;
              } else if (strncmp(serial_buf + i, "HZ: ", 4) == 0) {
                  freq_A = atoi(serial_buf+i+4);
                  i += 4 + ceil(log10(freq_A));   // One more for skip space
                  if (serial_buf[i] == '/')
                  {
                      i++;
                      freq_B = atoi(serial_buf+i);
                      i += ceil(log10(freq_B));   // One more for skip space
                  }
                  if (serial_buf[i] == '/')
                  {
                      i++;
                      freq_C = atoi(serial_buf+i);
                      i += ceil(log10(freq_C));   // One more for skip space
                  }
                  continue;
              } else if (strncmp(serial_buf + i, "DC RMS: ", 8) == 0) {
                  i += 8;
                  DC_A = readFloat(serial_buf, serial_len, &i);
                  if (serial_buf[i] == '/')
                  {
                      i++;
                      DC_B = readFloat(serial_buf, serial_len, &i);
                  }
                  if (serial_buf[i] == '/')
                  {
                      i++;
                      DC_C = readFloat(serial_buf, serial_len, &i);
                  }
                  continue;
              } else if (strncmp(serial_buf + i, "AC RMS: ", 8) == 0) {
                  i += 8;
                  AC_A = readFloat(serial_buf, serial_len, &i);
                  if (serial_buf[i] == '/')
                  {
                      i++;
                      AC_B = readFloat(serial_buf, serial_len, &i);
                  }
                  if (serial_buf[i] == '/')
                  {
                      i++;
                      AC_C = readFloat(serial_buf, serial_len, &i);
                  }
                  continue;
              } else if (strncmp(serial_buf + i, "AC Amps: ", 9) == 0) {
                  i += 9;
                  Current_A = readFloat(serial_buf, serial_len, &i);
                  if (serial_buf[i] == '/')
                  {
                      i++;
                      Current_B = readFloat(serial_buf, serial_len, &i);
                  }
                  if (serial_buf[i] == '/')
                  {
                      i++;
                      Current_C = readFloat(serial_buf, serial_len, &i);
                  }
                  last_state = -1;
                  continue;
              }
          }
          serial_len = 0;
      }      
   }
//  Serial.println(last_send_rssi); //prints time since program started
   if (last_state != set_state || (abs(rssi - last_rssi) >= 3 && millis() - last_send_rssi >= 5000))
   {
      last_send_rssi = millis();
      last_state = set_state;
      last_rssi = rssi;
      char *p = msg_buf;
      p += sprintf(p, "{\"rssi\":%ld,\"version\":\"%s\",\"ota\":\"unset\",\"HZ_A\":%d,\"HZ_B\":%d,\"HZ_C\":%d,\"Amp_A\":", rssi, VERSION,
        freq_A, freq_B, freq_C);
      p = dtostrf(Current_A, 1, 4, p) + 5;
      p+= sprintf(p, ",\"Amp_B\":");
      p = dtostrf(Current_B, 1, 4, p) + 5;
      p+= sprintf(p, ",\"Amp_C\":");
      p = dtostrf(Current_C, 1, 4, p) + 5;
      p+= sprintf(p, "}");
      client.publish("status", msg_buf);
   }
   
   delay(500);
}
