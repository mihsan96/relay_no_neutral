#include <Arduino.h>
#include <FileData.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
// #include <AutoOTA.h>
#include <WiFiConnector.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define light_1_pin LED_BUILTIN//D5 // 14
#define light_2_pin D1 // 5

#define button_1_pin D6 // 12
#define button_2_pin D2 // 4

#define wifi_check_timeout 5000
#define mqtt_check_timeout 5000
#define button_timeout 100

#define def_topic_1 String("light/" + mqtt.id + "/1").c_str()
#define def_topic_2 String("light/" + mqtt.id + "/2").c_str()

volatile bool light_1_status = false;
volatile bool light_2_status = false;

volatile bool pressed_flag_1 = false;
volatile bool pressed_flag_2 = false;

uint32_t check_button_timmer_1 = 0;
uint32_t check_button_timmer_2 = 0;

uint32_t check_update_timer = 0;
uint32_t check_wifi_timer = 0;
uint32_t check_mqtt_timer = 0;

JsonDocument json_1;
JsonDocument json_2;

struct WIFIStruct
{
  char ssid[32];
  char pass[32];
};

struct MQTTStruct
{
  String id = "ESP-" + String(ESP.getChipId(), HEX);
  char server[32];
  int port;
  char login[32];
  char pass[32];
};

WIFIStruct wifi;
MQTTStruct mqtt;

FileData wifi_data(&LittleFS, "/wifi.dat", 'B', &wifi, sizeof(wifi));
FileData mqtt_data(&LittleFS, "/mqtt.dat", 'A', &mqtt, sizeof(mqtt));
// AutoOTA ota("0.01", "mihsan96/relay_no_neutral");
ESP8266WebServer server(80);

WiFiClient espClient;
PubSubClient client(espClient);

String html()
{
  String html;
  html = R"rawliteral(<!DOCTYPE HTML>
    <meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
    <style type="text/css">
        input {
            margin-bottom: 8px;
            font-size: 20px;
        }

        input[type="submit"] {
            width: 180px;
            height: 60px;
            margin-bottom: 8px;
            font-size: 20px;
        }
    </style>
    <center>
        <h3>WiFi settings</h3>)rawliteral";
  int numberOfNetworks = WiFi.scanNetworks();

  for (int i = 0; i < numberOfNetworks; i++)
  {
    html += "<dir><button class=\"wifi_ssid_button\" data-text=\"";
    html += WiFi.SSID(i);
    html += "\">";
    html += WiFi.SSID(i);
    html += "</button></dir>";
  }
  html += R"rawliteral(
        <form action="/connect" method="POST">
            <div><input type="text" name="wifi_ssid" placeholder="WiFi SSID" id="ssid" value=""></div>
            <div><input type="password" name="wifi_pass" placeholder="WiFi Password" value=""></div>
            <div><input type="text" name="mqtt_server" placeholder="MQTT Server" value=""></div>
            <div><input type="number" name="mqtt_port" placeholder="MQTT Port" value="1883"></div>
            <div><input type="text" name="mqtt_login" placeholder="MQTT Login" value=""></div>
            <div><input type="password" name="mqtt_pass" placeholder="MQTT Password" value=""></div>
            <input type="submit" value="Submit">
        </form>
        </center>
    <script>
        const buttons = document.querySelectorAll('.wifi_ssid_button');
        const input = document.getElementById('ssid');

        buttons.forEach(button => {
            button.addEventListener('click', function () {
                input.value = this.getAttribute('data-text');
            });
        });
    </script>
</body>
</html>)rawliteral";
  return html;
}
String success_html()
{
  String html = R"rawliteral(<!DOCTYPE HTML>
    <meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
    <style type="text/css">
        input[type="text"] {
            margin-bottom: 8px;
            font-size: 20px;
        }

        input[type="submit"] {
            width: 180px;
            height: 60px;
            margin-bottom: 8px;
            font-size: 20px;
        }
    </style>
    <center>
        <h3>Saved!</h3>
    </center>
</body>
</html>)rawliteral";
  return html;
}

void AutoDiscovery()
{
  char discover_1[256];
  json_1["name"] = String("Light_" + mqtt.id + "_1");
  json_1["command_topic"] = String(def_topic_1);
  json_1["payload_on"] = "1";
  json_1["payload_off"] = "0";
  json_1["retain"] = true;
  json_1["unique_id"] = String(mqtt.id + "_1");
  serializeJson(json_1, discover_1);

  char discover_2[256];
  json_2["name"] = String("Light_" + mqtt.id + "_2");
  json_2["command_topic"] = String(def_topic_2);
  json_2["payload_on"] = "1";
  json_2["payload_off"] = "0";
  json_2["retain"] = true;
  json_2["unique_id"] = String(mqtt.id + "_2");
  serializeJson(json_2, discover_2);

  client.publish(String("homeassistant/light/" + mqtt.id + "_1" + "/config").c_str(), discover_1);
  client.publish(String("homeassistant/light/" + mqtt.id + "_2" + "/config").c_str(), discover_2);
}

void MQTTReconnect()
{
  if (String(mqtt.server).length() and WiFi.isConnected() and (millis() - check_mqtt_timer > mqtt_check_timeout))
  {
    check_mqtt_timer = millis();
    if (client.connect(mqtt.id.c_str(), mqtt.login, mqtt.pass))
    {
      Serial.println(client.state());
      Serial.println(def_topic_1);
      Serial.println(def_topic_2);

      client.subscribe(def_topic_1);
      client.subscribe(def_topic_2);

      AutoDiscovery();

      client.publish(def_topic_1, String(light_1_status).c_str());
      client.publish(def_topic_2, String(light_2_status).c_str());
    }
  }
}

void handleConnect()
{
  server.arg("wifi_ssid").toCharArray(wifi.ssid, sizeof(wifi.ssid));
  server.arg("wifi_pass").toCharArray(wifi.pass, sizeof(wifi.pass));

  server.arg("mqtt_server").toCharArray(mqtt.server, sizeof(mqtt.server));
  mqtt.port = server.arg("mqtt_port").toInt();
  server.arg("mqtt_login").toCharArray(mqtt.login, sizeof(mqtt.login));
  server.arg("mqtt_pass").toCharArray(mqtt.pass, sizeof(mqtt.pass));

  WiFiConnector.connect(wifi.ssid, wifi.pass);
  if (String(mqtt.server).length())
  {
    client.setServer(mqtt.server, mqtt.port);
    MQTTReconnect();
  }

  server.send(200, "text/html", success_html());

  wifi_data.updateNow();
  mqtt_data.updateNow();
}

void handleSendIndex()
{
  server.send(200, "text/html", html());
  Serial.println("send index");
}

void handleErrorConnect()
{
  Serial.println("connect error");
}

void handleConnected()
{
  Serial.println("connect OK");
}

IRAM_ATTR void SwitchHandler1()
{
  pressed_flag_1 = true;
  check_button_timmer_1 = millis();
}

IRAM_ATTR void SwitchHandler2()
{
  pressed_flag_2 = true;
  check_button_timmer_2 = millis();
  Serial.println(111);
}

void WiFiReconnect()
{
  if (!WiFiConnector.connected() && ((millis() - check_wifi_timer) > wifi_check_timeout))
  {
    check_wifi_timer = millis();
    WiFiConnector.connect(wifi.ssid, wifi.pass);
  }
}

void MQTTHandler(char *topic, byte *payload, unsigned int length)
{
  String Payload = "";
  for (unsigned int i = 0; i < length; i++)
    Payload += (char)payload[i];
  Serial.println(Payload);
  Serial.println(topic);

  if (String(topic) == def_topic_1 && Payload != String(light_1_status))
  {
    SwitchHandler1();
  }
  else if (String(topic) == def_topic_2 && Payload != String(light_2_status))
  {
    SwitchHandler2();
    Serial.println(213);
  }
}

void setup()
{
  Serial.begin(74880);
  LittleFS.begin();

  WiFi.setHostname(String(mqtt.id).c_str());

  wifi_data.read();
  mqtt_data.read();

  WiFiConnector.connect(wifi.ssid, wifi.pass);
  WiFiConnector.onConnect(handleConnected);
  WiFiConnector.onError(handleErrorConnect);

  Serial.println(String(mqtt.server).length());

  if (String(mqtt.server).length())
  {
    client.setServer(mqtt.server, mqtt.port);
    client.setCallback(MQTTHandler);
    MQTTReconnect();
  }

  server.on("/", HTTP_GET, handleSendIndex);
  server.on("/connect", HTTP_POST, handleConnect);
  server.begin();

  pinMode(button_1_pin, INPUT);
  pinMode(button_2_pin, INPUT);

  pinMode(light_1_pin, OUTPUT);
  pinMode(light_2_pin, OUTPUT);

  attachInterrupt(button_1_pin, SwitchHandler1, CHANGE);
  attachInterrupt(button_2_pin, SwitchHandler2, CHANGE);

  digitalWrite(light_1_pin, light_1_status);
  digitalWrite(light_2_pin, light_2_status);
}

void loop()
{

  if (!WiFiConnector.tick())
  {
    WiFiReconnect();
  }

  if (!client.loop())
  {
    MQTTReconnect();
  }

  server.handleClient();

  if (pressed_flag_1 and (millis() - check_button_timmer_1 > button_timeout))
  {
    Serial.println("swiched_1");
    light_1_status = !light_1_status;
    digitalWrite(light_1_pin, light_1_status);
    client.publish(def_topic_1, String(light_1_status).c_str());
    pressed_flag_1 = false;
  }

  if (pressed_flag_2 and (millis() - check_button_timmer_2 > button_timeout))
  {
    Serial.println("swiched_2");
    light_2_status = !light_2_status;
    digitalWrite(light_2_pin, light_2_status);
    client.publish(def_topic_2, String(light_2_status).c_str());
    pressed_flag_2 = false;
  }
  // String ver, notes;
  // if (ota.checkUpdate(&ver, &notes)) {
  //   Serial.println(ver);
  //   Serial.println(notes);
  // }
}
