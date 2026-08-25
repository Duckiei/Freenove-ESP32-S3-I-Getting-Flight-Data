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
#include <mapbox_static_480x320_markers.h>

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
std::vector<lv_obj_t *> planes;

// Object initialization
lv_obj_t *pInfoboxLabel;
lv_obj_t *pStartupScreenText;
String startupScreenText = "";

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

String token;
unsigned long expires_at;
unsigned long whenUpdateScreen;

lv_obj_t *pPreviousSelectedPlane = nullptr;

// Methods Initialization
float mapFloat(float start, float fromLow, float fromMax, float toLow, float toMax);
void btn_event_cb(lv_event_t *e);
lv_obj_t *buildplaneScreen();
void buildPlane(Plane &planeData, lv_obj_t *planeScreen);
lv_obj_t *buildStartupScreen();
void getToken4000();
void drawPlanestoScreen();

void setup()
{
  // Startup
  screen.init();
  lv_obj_t *startupScreen = buildStartupScreen();
  pStartupScreenText = lv_label_create(startupScreen);
  lv_label_set_text(pStartupScreenText, startupScreenText.c_str());
  lv_obj_align_to(pStartupScreenText, startupScreen, LV_ALIGN_TOP_MID, -90, 20);
  lv_obj_set_style_text_font(pStartupScreenText, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(pStartupScreenText, lv_color_hex(0x000000), 0);
  screen.routine();
  Serial.begin(9600);
  delay(1750);

  // Connect to Wifi
  Serial.println("Connecting WiFi...");

  startupScreenText += "Connecting WiFi...";
  lv_label_set_text(pStartupScreenText, startupScreenText.c_str());
  screen.routine();

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(5);
  }

  startupScreenText += "\nWifi Connected!";
  lv_label_set_text(pStartupScreenText, startupScreenText.c_str());
  screen.routine();

  startupScreenText += "\nLocal IP --> " + WiFi.localIP().toString();
  lv_label_set_text(pStartupScreenText, startupScreenText.c_str());
  screen.routine();

  delay(2000);
  getToken4000();
  drawPlanestoScreen();
}

void loop()
{
  // put your main code here, to run repeatedly:
  if (millis() / 1000 == expires_at)
  {
    getToken4000();
  }

  if (millis() / 1000 == whenUpdateScreen)
  {
    drawPlanestoScreen();
  }
  screen.routine();
  delay(5);
}
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//===============================================================================
//============================= FUNCTIONS / METHODS =============================
//===============================================================================

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

  // Set new plane colour
  lv_obj_set_style_img_recolor(planeObj, lv_color_hex(0xffe100), 0);
  if (pPreviousSelectedPlane != nullptr && pPreviousSelectedPlane != planeObj)
  {
    // Set previously selected plane white if its a diffferent plane and its actually something
    lv_obj_set_style_img_recolor(pPreviousSelectedPlane, lv_color_hex(0xffffff), 0);
  }

  pPreviousSelectedPlane = planeObj;

  String callsign = plane->callsign;
  String origin_country = plane->origin_country;
  float baro_altitude = plane->baro_altitude;
  float velocity = plane->velocity;

  lv_label_set_text(pInfoboxLabel, (callsign + "\n" + origin_country + "\n" + baro_altitude * 3.281 + " ft \n" + velocity * 1.944 + " kts").c_str());
}

lv_obj_t *buildplaneScreen()
{
  lv_obj_t *planeScreen = lv_obj_create(NULL);
  lv_obj_set_style_bg_img_src(planeScreen, &mapbox_static_480x320_markers, 0);

  // Infobox
  lv_obj_t *infoBox = lv_obj_create(planeScreen);
  lv_obj_align_to(infoBox, planeScreen, LV_ALIGN_TOP_LEFT, 20, 20);
  lv_obj_set_style_bg_color(infoBox, lv_color_hex(0x000000), 0);
  lv_obj_set_style_opa(infoBox, LV_OPA_50, 0);
  lv_obj_set_size(infoBox, 200, 90);

  // Infobox Text
  pInfoboxLabel = lv_label_create(planeScreen);
  lv_label_set_text(pInfoboxLabel, "Select a \nplane for \ndata: ");
  lv_obj_align_to(pInfoboxLabel, planeScreen, LV_ALIGN_TOP_LEFT, 30, 30);
  lv_obj_set_style_text_color(pInfoboxLabel, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_font(pInfoboxLabel, &lv_font_montserrat_14, 0);
  lv_scr_load(planeScreen);

  lv_obj_clear_flag(planeScreen, LV_OBJ_FLAG_SCROLLABLE);

  // lv_obj_t *pDiagnosticButton = lv_btn_create(lv_scr_act());
  // lv_obj_align_to(pDiagnosticButton, lv_scr_act(), LV_ALIGN_TOP_RIGHT, -20, 20);
  // lv_obj_add_event_cb(pDiagnosticButton, );

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
  planes.push_back(plane);
}

lv_obj_t *buildStartupScreen()
{
  lv_obj_t *startupScreen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(startupScreen, lv_color_hex(0xffffff), 0);
  lv_scr_load(startupScreen);
  return startupScreen;
}

void getToken4000()
{
  String clientID = opensky_clientId;
  String clientSecret = opensky_clientSecret;

  WiFiClientSecure client;
  HTTPClient https;
  JsonDocument doc;
  Serial.begin(9600);
  delay(1750);
  unsigned long expires_in;

  // Build HTTPS Url
  client.setInsecure();
  String url = "https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token";
  // Check if connection to url can be made
  if (https.begin(client, url))
  {
    // Retrieve response code & print it
    https.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String d_Part = "grant_type=client_credentials";
    d_Part += "&client_id=" + clientID;
    d_Part += "&client_secret=" + clientSecret;

    int httpCode = https.POST(d_Part);

    Serial.printf("Token request HTTP code: %d\n", httpCode);

    // Validate connection can even be made
    // Validate connection is proper
    if (httpCode == HTTP_CODE_OK)
    {
      String payload = https.getString();

      deserializeJson(doc, payload);

      token = doc["access_token"].as<String>();
      expires_in = doc["expires_in"] | 1800;

      expires_at = millis() / 1000 + expires_in;
      expires_at = expires_at - 30;
    }
  }
  Serial.println(token);
  Serial.println(expires_in);
  Serial.println(expires_at);
}

void drawPlanestoScreen()
{
  lv_obj_t *planeScreen = buildplaneScreen();

  // Handle encryption
  WiFiClientSecure client;
  HTTPClient https;
  JsonDocument doc;
  client.setInsecure();

  String url = "https://opensky-network.org/api/states/all?";
  url += "lamin=" + String(lamin);
  url += "&lomin=" + String(lomin);
  url += "&lamax=" + String(lamax);
  url += "&lomax=" + String(lomax);

  https.begin(client, url);
  https.addHeader("Authorization", "Bearer " + token);
  int httpCode = https.GET();

  // Validate connection is proper
  if (httpCode == HTTP_CODE_OK)
  {
    // Retrieve JSON & parse it
    String payload = https.getString();
    deserializeJson(doc, payload);

    JsonArray planeStates = doc["states"].as<JsonArray>();

    // clear previous planes
    for (lv_obj_t *object : planes)
    {
      lv_obj_del(object);
    }

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
  whenUpdateScreen = millis() / 1000 + 30;
  pPreviousSelectedPlane = nullptr;
  https.end();
}