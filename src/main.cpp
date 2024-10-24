#include <Arduino.h>
#include <FileData.h>
#include <LittleFS.h>
#include <SimplePortal.h>
#include <ESP8266WiFi.h>

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
  Serial.println(123);
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

void setup()
{
  Serial.begin(9600);
  LittleFS.begin();
  data.read();

  connect_to_wifi();
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
}
