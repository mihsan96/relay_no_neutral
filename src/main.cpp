#include <Arduino.h>
#include <FileData.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
// #include <AutoOTA.h>
#include <WiFiConnector.h>
#include <ESP8266WebServer.h>

#define detect_zero_pin D1 // 5
#define relay_1_pin D5     // 14
#define button_1_pin D6    // 12
#define wifi_check_timeout 5000
#define check_wifi_counter_max 2

volatile bool relay_1_status = true;
volatile bool pressed_flag;

uint32_t check_button_timmer;
uint32_t check_update_timer;
ESP8266WebServer server(80);

struct WIFIStruct
{
  char ssid[32];
  char pass[32];
};

struct MQTTStruct
{
  char server[32];
  int port;
  char login[32];
  char pass[32];
};

WIFIStruct wifi;
MQTTStruct mqtt;

FileData wifi_data(&LittleFS, "/wifi.dat", 'B', &wifi, sizeof(wifi));
FileData mqtt_data(&LittleFS, "/mqtt.dat", 'B', &mqtt, sizeof(mqtt));
// AutoOTA ota("0.01", "mihsan96/relay_no_neutral");

String html()
{
  String html;
  html = R"rawliteral(<!DOCTYPE HTML>
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
            <input type="text" name="wifi_ssid" placeholder="WiFi SSID" id="ssid" value="">
            <input type="password" name="wifi_pass" placeholder="WiFi Password" value="">
            <input type="text" name="mqtt_server" placeholder="MQTT Server" value="">
            <input type="number" name="mqtt_port" placeholder="MQTT Port" value="1883">
            <input type="text" name="mqtt_login" placeholder="MQTT Login" value="">
            <input type="password" name="mqtt_pass" placeholder="MQTT Password" value="">
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

void handleConnect()
{
  server.arg("wifi_ssid").toCharArray(wifi.ssid, sizeof(wifi.ssid));
  server.arg("wifi_pass").toCharArray(wifi.pass, sizeof(wifi.pass));
  server.arg("mqtt_server").toCharArray(mqtt.server, sizeof(mqtt.server));
  mqtt.port = server.arg("mqtt_port").toInt();
  server.arg("mqtt_login").toCharArray(mqtt.login, sizeof(mqtt.login));
  server.arg("mqtt_pass").toCharArray(mqtt.pass, sizeof(mqtt.pass));

  WiFiConnector.connect(wifi.ssid, wifi.pass);

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

IRAM_ATTR void SwitchHandler()
{
  pressed_flag = true;
  check_button_timmer = millis();
}

void setup()
{
  Serial.begin(74880);
  LittleFS.begin();
  WiFiConnector.connect(wifi.ssid, wifi.pass);
  WiFiConnector.onConnect(handleConnected);
  WiFiConnector.onError(handleErrorConnect);
  server.on("/", HTTP_GET, handleSendIndex);
  server.on("/connect", HTTP_POST, handleConnect);
  server.begin();

  pinMode(button_1_pin, INPUT_PULLUP);
  pinMode(relay_1_pin, OUTPUT);

  attachInterrupt(button_1_pin, SwitchHandler, CHANGE);
  digitalWrite(relay_1_pin, true);
}

void loop()
{

  WiFiConnector.tick();
  server.handleClient();

  // wifi_data.tick();
  // mqtt_data.tick();

  if (pressed_flag and millis() - check_button_timmer > 10)
  {
    Serial.println("swiched");
    relay_1_status = !relay_1_status;
    digitalWrite(relay_1_pin, relay_1_status);
    pressed_flag = false;
  }
  // String ver, notes;
  // if (ota.checkUpdate(&ver, &notes)) {
  //   Serial.println(ver);
  //   Serial.println(notes);
  // }
}
