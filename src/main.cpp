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
#include <Esp.h>

//=========Initialization =========\\

// Wifi
const String ssid = WIFI_SSID;
const String pass = WIFI_PASSWORD;

// Bounding Box
const float lamin = 43.5;
const float lomin = -79.898071;
const float lamax = 44.276;
const float lomax = -78.733521;

// Screen dimensions
const int SCREEN_WIDTH = 480;
const int SCREEN_HEIGHT = 320;

// Hold all plane obj's
std::vector<lv_obj_t> planes;

// Object initialization
lv_obj_t *callsign_label;

// LVGL --> Display wrapper
Display screen;

// Struct
struct Plane
{
  String callsign;
  String origin_country;
  float longitude;
  float latitude;
  float baro_altitude;
  bool on_ground;
  float velocity;
  float heading;
  float vertical_rate;
  int category;
};

lv_obj_t *pPreviousSelectedPlane = nullptr;

// Methods Initialization
float mapFloat(float start, float fromLow, float fromMax, float toLow, float toMax);
void btn_event_cb(lv_event_t *e);
lv_obj_t *buildplaneScreen();
void buildPlane(Plane &planeData, lv_obj_t *planeScreen);
lv_obj_t *buildStartupScreen();

void setup()
{
  screen.init();
  lv_obj_t *startupScreen = buildStartupScreen();
  Serial.begin(9600);
  delay(1750);

  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x181a26), 0);

  // Connect to Wifi
  Serial.println("Connecting WiFi...");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(5);
  }
  Serial.println(WiFi.localIP());

  // Handle encryption
  WiFiClientSecure client;
  client.setInsecure();

  // Build HTTPS Url
  HTTPClient https;
  String url = "https://opensky-network.org/api/states/all?";
  url += "lamin=" + String(lamin);
  url += "&lomin=" + String(lomin);
  url += "&lamax=" + String(lamax);
  url += "&lomax=" + String(lomax);
  Serial.println(url);

  // Check if connection to url can be made
  if (https.begin(client, url))
  {
    // Retrieve response code & print it
    int httpCode = https.GET();
    Serial.printf("Http Code: %d \n", httpCode);

    // Validate connection can even be made
    if (httpCode > 0)
    {
      // Validate connection is proper
      if (httpCode == HTTP_CODE_OK)
      {
        JsonDocument doc;

        // Retrieve JSON & parse it
        String payload = https.getString();
        deserializeJson(doc, payload);

        // Explicitly convert json into accessible array
        JsonArray planeStates = doc["states"].as<JsonArray>();

        lv_obj_t *planeScreen = buildplaneScreen();

        // Create all plane objects
        for (JsonArray planeState : planeStates)
        {
          Plane plane;

          plane.callsign = planeState[1].as<String>();
          plane.origin_country = planeState[2].as<String>();
          plane.longitude = planeState[5];
          plane.latitude = planeState[6];
          plane.baro_altitude = planeState[7];
          plane.on_ground = planeState[8];
          plane.velocity = planeState[9];
          plane.heading = planeState[10];
          plane.vertical_rate = planeState[11];
          plane.category = planeState[17];

          if (!plane.on_ground)
          {
            buildPlane(plane, planeScreen);
          }
        }
      }
      Serial.println("Finished printing values.");
    }
  }
  else
  {
    Serial.println("Failed to connect");
  }
  https.end();
}

void loop()
{
  // put your main code here, to run repeatedly:
  screen.routine();
  delay(5);
}

//========= FUNCTIONS / METHODS =========\\

float mapFloat(float start, float fromLow, float fromMax, float toLow, float toMax)
{
  return (((start - fromLow) * (toMax - toLow)) / (fromMax - fromLow)) + toLow;
}

void btn_event_cb(lv_event_t *e)
{
  // Get data
  lv_obj_t *planeObj = lv_event_get_target(e);
  Plane *plane = (Plane *)lv_obj_get_user_data(planeObj);

  // Colouring highlighted plane

  // Set new plane red
  lv_obj_set_style_img_recolor(planeObj, lv_color_hex(0x992855), 0);
  if (pPreviousSelectedPlane != nullptr && pPreviousSelectedPlane != planeObj)
  {
    // Set previously selected plane white if its a diffferent plane and its actually something
    lv_obj_set_style_img_recolor(pPreviousSelectedPlane, lv_color_hex(0xffffff), 0);
  }

  pPreviousSelectedPlane = planeObj;
  lv_label_set_text(callsign_label, plane->callsign.c_str());
}

lv_obj_t *buildplaneScreen() // return a pointer
{
  lv_obj_t *planeScreen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(planeScreen, lv_color_hex(0x181a26), 0);

  callsign_label = lv_label_create(planeScreen);
  lv_label_set_text(callsign_label, "Callsign: ");
  lv_obj_align_to(callsign_label, planeScreen, LV_ALIGN_TOP_LEFT, 20, 20);
  lv_obj_set_style_text_color(callsign_label, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_font(callsign_label, &lv_font_montserrat_20, 0);
  lv_scr_load(planeScreen);

  return planeScreen; // return the actual pointer, no dereference
}

void buildPlane(Plane &planeData, lv_obj_t *planeScreen)
{
  lv_obj_t *plane = lv_img_create(planeScreen);
  lv_img_set_src(plane, &Plane_Icon_30x30px);
  lv_obj_add_flag(plane, LV_OBJ_FLAG_CLICKABLE);

  float lat_mapped = mapFloat(planeData.latitude, lamin, lamax, SCREEN_HEIGHT, 0);
  float lon_mapped = mapFloat(planeData.longitude, lomin, lomax, 0, SCREEN_WIDTH);

  lv_obj_set_pos(plane, lon_mapped - 15, lat_mapped - 15);
  lv_obj_set_style_img_recolor(plane, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_img_recolor_opa(plane, LV_OPA_COVER, 0);
  lv_img_set_angle(plane, planeData.heading * 10);

  Plane *planeCopy = new Plane(planeData); // heap-allocated copy of the whole struct
  lv_obj_set_user_data(plane, planeCopy);
  lv_obj_add_event_cb(plane, btn_event_cb, LV_EVENT_CLICKED, NULL);
  planes.push_back(*plane);
}

lv_obj_t *buildStartupScreen()
{
  lv_obj_t *startupScreen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(startupScreen, lv_color_hex(0xffffff), 0);
  lv_scr_load(startupScreen);
  return startupScreen;
}
