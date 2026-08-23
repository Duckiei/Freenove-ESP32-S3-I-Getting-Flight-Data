//========= Libraries =========\\

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <display.h>
#include <vector>
#include <Plane_Icon_30x30px_TrueColourAlpha.h>
#include "tokens.h"

//========= Initialization =========\\

const String ssid = WIFI_SSID;
const String pass = WIFI_PASSWORD;

const float lamin = 43.5;
const float lomin = -79.898071;
const float lamax = 44.276;
const float lomax = -78.733521;

const int SCREEN_WIDTH = 480;
const int SCREEN_HEIGHT = 320;

std::vector<lv_obj_t> squares;
lv_obj_t *label;
Display screen;

float mapFloat(float start, float fromLow, float fromMax, float toLow, float toMax);

void btn_event_cb(lv_event_t *e)
{
  lv_obj_t *obj = lv_event_get_target(e);
  const char *callsign = (const char *)lv_obj_get_user_data(obj);
  lv_label_set_text(label, callsign);
}

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(9600);
  delay(1750);

  screen.init();
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x181a26), 0);
  label = lv_label_create(lv_scr_act());
  lv_label_set_text(label, "Callsign: ");
  lv_obj_align_to(label, lv_scr_act(), LV_ALIGN_TOP_LEFT, 20, 20);
  lv_obj_set_style_text_color(label, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);

  Serial.println("Connecting...");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(5);
  }

  Serial.println(WiFi.localIP());

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  String url = "https://opensky-network.org/api/states/all?";
  url += "lamin=" + String(lamin);
  url += "&lomin=" + String(lomin);
  url += "&lamax=" + String(lamax);
  url += "&lomax=" + String(lomax);

  Serial.println(url);
  if (https.begin(client, url))
  {
    int httpCode = https.GET();
    Serial.printf("Http Code: %d \n", httpCode);
    if (httpCode > 0)
    {
      Serial.printf("HTTP response code: %d\n", httpCode);

      if (httpCode == HTTP_CODE_OK)
      {
        String payload = https.getString();
        JsonDocument doc;

        deserializeJson(doc, payload);

        JsonArray states = doc["states"].as<JsonArray>();

        for (JsonArray state : states)
        {
          if (!state[8])
          {
            const char *callsign = state[1];
            float lat = state[6];
            float lon = state[5];
            int heading = state[10];

            float lat_mapped = mapFloat(lat, lamin, lamax, SCREEN_HEIGHT, 0);
            float lon_mapped = mapFloat(lon, lomin, lomax, 0, SCREEN_WIDTH);

            lv_obj_t *square = lv_img_create(lv_scr_act());
            lv_img_set_src(square, &Plane_Icon_30x30px);
            lv_obj_add_flag(square, LV_OBJ_FLAG_CLICKABLE);

            lv_obj_set_pos(square, lon_mapped - 15, lat_mapped - 15);
            lv_obj_set_style_img_recolor(square, lv_color_hex(0xffffff), 0);
            lv_obj_set_style_img_recolor_opa(square, LV_OPA_COVER, 0);
            lv_img_set_angle(square, heading * 10);

            char *callsignCopy = strdup(callsign);
            lv_obj_set_user_data(square, callsignCopy);
            lv_obj_add_event_cb(square, btn_event_cb, LV_EVENT_CLICKED, NULL);
            squares.push_back(*square);

            Serial.printf("%s: %.4f, %.4f\n", callsign, lat_mapped, lon_mapped);
          }
        }
        Serial.println("Finished printing values.");
      }
    }
    else
    {
      Serial.println("Failed to connect");
      Serial.println("Restrying...");
    }
  }
  https.end();
}

void loop()
{
  // put your main code here, to run repeatedly:
  screen.routine();
  delay(5);
}

float mapFloat(float start, float fromLow, float fromMax, float toLow, float toMax)
{
  return (((start - fromLow) * (toMax - toLow)) / (fromMax - fromLow)) + toLow;
}
