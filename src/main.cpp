#include <Arduino.h>
#include <FileData.h>
#include <LittleFS.h>
#include <SimplePortal.h>
#include <ESP8266WiFi.h>

#define detect_zero_pin 5
#define relay_1_pin 6
#define button_1_pin 7

bool flag_zero = false;
bool relay_1_status = false;
int last_zero;

struct WIFIStruct
{
  char ssid[32];
  char pass[32];
};

WIFIStruct wifi;

FileData data(&LittleFS, "/wifi.dat", 'A', &wifi, sizeof(wifi));

void connect_to_wifi()
{
  Serial.println(wifi.pass);
  Serial.println(wifi.ssid);
  if (wifi.ssid)
  {
    WiFi.begin(wifi.ssid, wifi.pass);
  }
  else
  {
    portalStart();
  }
  bool status = 0;
  while (!status)
  {
    switch (WiFi.status())
    {
    case WL_CONNECT_FAILED:
    case WL_NO_SSID_AVAIL:
    case WL_WRONG_PASSWORD:
      portalStart();
      status = 1;
      strcpy(wifi.ssid, "");
      strcpy(wifi.pass, "");
      break;
    case WL_CONNECTED:
      strcpy(wifi.ssid, portalCfg.SSID);
      strcpy(wifi.pass, portalCfg.pass);
      status = 1;
      data.updateNow();
    default:
      break;
    }
  }
}

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

  connect_to_wifi();

  pinMode(button_1_pin, INPUT);
  pinMode(relay_1_pin, OUTPUT);
  pinMode(detect_zero_pin, INPUT);

  attachInterrupt(detect_zero_pin, DetectorZeroHandler, RISING);
  attachInterrupt(button_1_pin, SwitchHandler, CHANGE);
}

void loop()
{
  data.tick();
  if (portalTick())
  {
    if (portalStatus() == SP_SUBMIT)
    {
      connect_to_wifi();
    }
  }
  if (relay_1_status & flag_zero & (millis() - last_zero >= 8))
  {
    digitalWrite(relay_1_pin, relay_1_status);
    flag_zero = false;
  }
}
