#include <Arduino.h>
#include <FileData.h>
#include <LittleFS.h>
#include <SimplePortal.h>
#include <ESP8266WiFi.h>

#define detect_zero_pin 5
#define relay_1_pin 6
#define button_1_pin 7
#define wifi_check_timeout 5000
#define check_wifi_counter_max 12

bool flag_zero = false;
bool relay_1_status = false;
int last_zero;
bool status = false;
uint32_t check_wifi_timer;
int check_wifi_counter = 0;
struct WIFIStruct
{
  char ssid[32];
  char pass[32];
};

WIFIStruct wifi;

FileData data(&LittleFS, "/wifi.dat", 'B', &wifi, sizeof(wifi));

IRAM_ATTR void DetectorZeroHandler()
{
  if (relay_1_status)
  {
    flag_zero = true;
    last_zero = millis();
    digitalWrite(relay_1_pin, relay_1_status);
  }
}

IRAM_ATTR void SwitchHandler()
{
  relay_1_status = !relay_1_status;
  digitalWrite(relay_1_pin, relay_1_status);
}

void setup()
{
  Serial.begin(9600);
  LittleFS.begin();
  data.read();

  if (wifi.ssid && wifi.ssid[0])
  {
    WiFi.begin(wifi.ssid, wifi.pass);
    check_wifi_timer = millis();
  }
  else
  {
    portalStart();
  }

  pinMode(button_1_pin, INPUT);
  pinMode(relay_1_pin, OUTPUT);
  pinMode(detect_zero_pin, INPUT);

  attachInterrupt(detect_zero_pin, DetectorZeroHandler, RISING);
  attachInterrupt(button_1_pin, SwitchHandler, CHANGE);
}

void loop()
{
  Serial.println(wifi.ssid);
  data.tick();
  if (portalTick())
  {
    if (portalStatus() == SP_SUBMIT)
    {
      strcpy(wifi.ssid, portalCfg.SSID);
      strcpy(wifi.pass, portalCfg.pass);
      WiFi.begin(wifi.ssid, wifi.pass);
    }
  }
  else if (millis() - check_wifi_timer >= wifi_check_timeout)
  {
    check_wifi_timer = millis();
    switch (WiFi.status())
    {
    case WL_CONNECT_FAILED:
    case WL_NO_SSID_AVAIL:
    case WL_WRONG_PASSWORD:
    case WL_IDLE_STATUS:
      portalStart();
      strcpy(wifi.ssid, "");
      strcpy(wifi.pass, "");
      break;
    case WL_CONNECTED:
      strcpy(wifi.ssid, portalCfg.SSID);
      strcpy(wifi.pass, portalCfg.pass);
      data.updateNow();
    case WL_CONNECTION_LOST:
    case WL_DISCONNECTED:
      if (check_wifi_counter < check_wifi_counter_max)
      {
        WiFi.begin(wifi.ssid, wifi.pass);
        check_wifi_counter += 1;
        break;
      }
      else
      {
        portalStart();
        strcpy(wifi.ssid, "");
        strcpy(wifi.pass, "");
        check_wifi_counter = 0;
        break;
      }
    case WL_NO_SHIELD:
    case WL_SCAN_COMPLETED:
      break;
    }
  }
  if (relay_1_status && flag_zero && (millis() - last_zero >= 8))
  {
    digitalWrite(relay_1_pin, !relay_1_status);
    flag_zero = false;
  }
}
