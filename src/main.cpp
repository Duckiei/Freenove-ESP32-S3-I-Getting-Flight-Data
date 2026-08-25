//========= Libraries =========\\

#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <display.h>
#include <vector>
#include <unordered_map>
#include <Plane_Icon_30x30px_TrueColourAlpha.h>
#include "tokens.h"
#include <Esp.h>
#include <mapbox_static_480x320_markers.h>
#include <mapbox_static_480x320_Dark.h>

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
lv_obj_t *planeScreen = nullptr;
lv_obj_t *settingsScreen = nullptr;
lv_obj_t *pPreviousSelectedPlane = nullptr;

lv_obj_t *text;

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

// Token & repeating stuff
String token;
unsigned long expires_at;
unsigned long whenUpdateScreen;

// Colours
const int WHITE = 0xffffff;
const int HOT_PINK = 0xff006b;
const int YELLOW = 0xffe100;
const int DARK_GREY = 0x202020;
const int BLACK = 0x000000;

// Colours, Backgrounds, Images
const int planeColourDefault = WHITE;
int planeColourSelected = YELLOW;
const lv_img_dsc_t *mapBackground = &mapbox_static_480x320_markers;
const int startupScreenColour = DARK_GREY;
const int startupScreenTextColour = WHITE;
const int infoBoxColour = BLACK;
const int infoBoxTextColour = WHITE;
const int arrowButtonsColour = BLACK;

bool isLight = true;
bool lookingAtDiagnosticScreen = false;

// Methods Initialization
float mapFloat(float start, float fromLow, float fromMax, float toLow, float toMax);
void user_select_plane(lv_event_t *e);
lv_obj_t *buildplaneScreen();
void buildPlane(Plane &planeData, lv_obj_t *planeScreen);
lv_obj_t *buildStartupScreen();
void getToken4000();
void drawPlanestoScreen(lv_obj_t *planeScreen);
lv_obj_t *buildSettingsScreen();

void setup()
{
  // Startup
  screen.init();
  lv_obj_t *startupScreen = buildStartupScreen();
  lv_scr_load(startupScreen);
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
  settingsScreen = buildSettingsScreen();
  planeScreen = buildplaneScreen();
  drawPlanestoScreen(planeScreen);
  lv_scr_load(planeScreen);
  lv_obj_del(startupScreen);
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
    drawPlanestoScreen(planeScreen);
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

void user_select_plane(lv_event_t *e)
{
  // Get data
  lv_obj_t *planeObj = lv_event_get_target(e);
  Plane *plane = (Plane *)lv_obj_get_user_data(planeObj);

  // Colouring highlighted plane

  // Set new plane colour
  lv_obj_set_style_img_recolor(planeObj, lv_color_hex(planeColourSelected), 0);
  if (pPreviousSelectedPlane != nullptr && pPreviousSelectedPlane != planeObj)
  {
    // Set previously selected plane white if its a diffferent plane and its actually something
    lv_obj_set_style_img_recolor(pPreviousSelectedPlane, lv_color_hex(planeColourDefault), 0);
  }

  pPreviousSelectedPlane = planeObj;

  String callsign = plane->callsign;
  String origin_country = plane->origin_country;
  float baro_altitude = plane->baro_altitude;
  float velocity = plane->velocity;

  lv_label_set_text(pInfoboxLabel, (callsign + "\n" + origin_country + "\n" + baro_altitude * 3.281 + " ft \n" + velocity * 1.944 + " kts").c_str());
}

void toggleViewMode(lv_event_t *e)
{
  if (isLight)
  {
    mapBackground = &mapbox_static_480x320_Dark;
    planeColourSelected = HOT_PINK;
  }
  else
  {
    mapBackground = &mapbox_static_480x320_markers;
    planeColourSelected = YELLOW;
  }

  lv_obj_set_style_bg_img_src(planeScreen, mapBackground, 0);

  isLight = !isLight;
}

void refreshDiagnostics(lv_event_t *e)
{
  std::unordered_map<int, String> wifiCodes;
  wifiCodes[0] = "Idle";
  wifiCodes[1] = "Network not found";
  wifiCodes[3] = "Connected";
  wifiCodes[4] = "Connection failed";
  wifiCodes[6] = "Disconnected";

  String stats = "";
  stats += "RAM";
  stats += "\n\t\t Total Heap (KB): " + String(ESP.getHeapSize() / 1000);
  stats += "\n\t\t Used Heap (KB): " + String((ESP.getHeapSize() - ESP.getFreeHeap()) / 1000);
  stats += "\n\n WiFi";
  stats += "\n\t\t Status: " + wifiCodes[WiFi.status()];
  stats += "\n\t\t IP: " + WiFi.localIP().toString();
  stats += "\n\t\t RSSI: " + String(WiFi.RSSI());
  stats += "\n\n Miscellaneous.";
  stats += "\n\t\t Runtime (Seconds): " + String(millis() / 1000);

  lv_label_set_text(text, stats.c_str());
}

void seeDiagnosticScreen(lv_event_t *e)
{
  if (lookingAtDiagnosticScreen)
  {
    lv_scr_load(planeScreen);
  }
  else
  {
    refreshDiagnostics(e);
    lv_scr_load(settingsScreen);
  }
  lookingAtDiagnosticScreen = !lookingAtDiagnosticScreen;
}

lv_obj_t *buildplaneScreen()
{
  // Screen itself
  lv_obj_t *planeScreen = lv_obj_create(NULL);
  lv_obj_set_style_bg_img_src(planeScreen, mapBackground, 0);
  lv_obj_clear_flag(planeScreen, LV_OBJ_FLAG_SCROLLABLE);

  // Infobox
  lv_obj_t *infoBox = lv_obj_create(planeScreen);
  lv_obj_align_to(infoBox, planeScreen, LV_ALIGN_TOP_LEFT, 20, 20);
  lv_obj_set_style_bg_color(infoBox, lv_color_hex(infoBoxColour), 0);
  lv_obj_set_style_opa(infoBox, LV_OPA_50, 0);
  lv_obj_set_size(infoBox, 200, 90);

  // Infobox Text
  pInfoboxLabel = lv_label_create(planeScreen);
  lv_label_set_text(pInfoboxLabel, "Select a \nplane for \ndata: ");
  lv_obj_align_to(pInfoboxLabel, planeScreen, LV_ALIGN_TOP_LEFT, 30, 30);
  lv_obj_set_style_text_color(pInfoboxLabel, lv_color_hex(infoBoxTextColour), 0);
  lv_obj_set_style_text_font(pInfoboxLabel, &lv_font_montserrat_14, 0);

  // Corner arrow buttons
  lv_obj_t *pRightArrow = lv_btn_create(planeScreen);
  lv_obj_t *pLeftArrow = lv_btn_create(planeScreen);
  lv_obj_set_size(pRightArrow, 40, 40);
  lv_obj_set_size(pLeftArrow, 40, 40);

  lv_obj_align(pRightArrow, LV_ALIGN_TOP_RIGHT, -10, 20);
  lv_obj_align(pLeftArrow, LV_ALIGN_TOP_RIGHT, -55, 20);

  lv_obj_set_style_bg_color(pRightArrow, lv_color_hex(arrowButtonsColour), 0);
  lv_obj_set_style_bg_color(pLeftArrow, lv_color_hex(arrowButtonsColour), 0);

  lv_obj_set_style_opa(pRightArrow, LV_OPA_50, 0);
  lv_obj_set_style_opa(pLeftArrow, LV_OPA_50, 0);

  lv_obj_t *rightArrowLabel = lv_label_create(pRightArrow);
  lv_obj_t *leftArrowLabel = lv_label_create(pLeftArrow);

  lv_obj_align(rightArrowLabel, LV_ALIGN_CENTER, 0, 0);
  lv_obj_align(leftArrowLabel, LV_ALIGN_CENTER, 0, 0);

  lv_label_set_text(rightArrowLabel, LV_SYMBOL_SETTINGS);
  lv_label_set_text(leftArrowLabel, LV_SYMBOL_EYE_OPEN);

  lv_obj_set_style_text_color(rightArrowLabel, lv_color_hex(WHITE), 0);
  lv_obj_set_style_text_color(leftArrowLabel, lv_color_hex(WHITE), 0);

  lv_obj_add_event_cb(pLeftArrow, toggleViewMode, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(pRightArrow, seeDiagnosticScreen, LV_EVENT_CLICKED, NULL);

  return planeScreen;
}

void buildPlane(Plane &planeData, lv_obj_t *planeScreen)
{
  lv_obj_t *plane = lv_img_create(planeScreen);
  lv_img_set_src(plane, &Plane_Icon_30x30px);
  lv_obj_add_flag(plane, LV_OBJ_FLAG_CLICKABLE);

  float lat_mapped = mapFloat(planeData.latitude, lamin, lamax, SCREEN_HEIGHT, 0);
  float lon_mapped = mapFloat(planeData.longitude, lomin, lomax, 0, SCREEN_WIDTH);

  lv_obj_set_pos(plane, lon_mapped - 15, lat_mapped - 15);
  lv_obj_set_style_img_recolor(plane, lv_color_hex(planeColourDefault), 0);
  lv_obj_set_style_img_recolor_opa(plane, LV_OPA_COVER, 0);
  lv_img_set_angle(plane, planeData.heading * 10);

  Plane *planeCopy = new Plane(planeData); // heap-allocated copy of the whole struct
  lv_obj_set_user_data(plane, planeCopy);
  lv_obj_add_event_cb(plane, user_select_plane, LV_EVENT_CLICKED, NULL);
  planes.push_back(plane);
}

lv_obj_t *buildStartupScreen()
{
  lv_obj_t *startupScreen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(startupScreen, lv_color_hex(startupScreenColour), 0);

  pStartupScreenText = lv_label_create(startupScreen);
  lv_label_set_text(pStartupScreenText, startupScreenText.c_str());
  lv_obj_align_to(pStartupScreenText, startupScreen, LV_ALIGN_TOP_MID, -90, 20);
  lv_obj_set_style_text_font(pStartupScreenText, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(pStartupScreenText, lv_color_hex(startupScreenTextColour), 0);

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
  doc.clear();
  https.end();
  Serial.println(token);
  Serial.println(expires_in);
  Serial.println(expires_at);
}

void drawPlanestoScreen(lv_obj_t *planeScreen)
{

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
  const char *keys[] = {"X-Rate-Limit-Remaining"};
  https.collectHeaders(keys, 1);
  int httpCode = https.GET();

  // Validate connection is proper
  if (httpCode == HTTP_CODE_OK)
  {
    // Retrieve JSON & parse it
    String rateLimitRemaining = https.header("X-Rate-Limit-Remaining");
    String payload = https.getString();
    deserializeJson(doc, payload);

    JsonArray planeStates = doc["states"].as<JsonArray>();

    // clear previous planes
    for (lv_obj_t *object : planes)
    {
      lv_obj_del(object);
    }

    planes.clear();
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
    Serial.println("Rate limit remaining: " + rateLimitRemaining);
  }
  whenUpdateScreen = millis() / 1000 + 30;
  pPreviousSelectedPlane = nullptr;
  doc.clear();
  https.end();
}

lv_obj_t *buildSettingsScreen()
{
  lv_obj_t *diagnosticScreen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(diagnosticScreen, lv_color_hex(DARK_GREY), 0);

  lv_obj_t *diagnosticTitle = lv_label_create(diagnosticScreen);
  lv_obj_set_style_text_font(diagnosticTitle, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(diagnosticTitle, lv_color_hex(WHITE), 0);
  lv_label_set_text(diagnosticTitle, "Diagnostics...");
  lv_obj_align(diagnosticTitle, LV_ALIGN_TOP_LEFT, 60, 30);

  // Make back arrow button
  lv_obj_t *backArrowButton = lv_btn_create(diagnosticScreen);
  lv_obj_align(backArrowButton, LV_ALIGN_TOP_LEFT, 10, 20);
  lv_obj_set_size(backArrowButton, 40, 40);
  lv_obj_set_style_bg_color(backArrowButton, lv_color_hex(BLACK), 0);
  lv_obj_set_style_bg_opa(backArrowButton, LV_OPA_50, 0);
  lv_obj_add_event_cb(backArrowButton, seeDiagnosticScreen, LV_EVENT_CLICKED, NULL);

  // Text in back arrow
  lv_obj_t *backArrowText = lv_label_create(backArrowButton);
  lv_obj_set_style_text_font(backArrowText, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(backArrowText, lv_color_hex(WHITE), 0);
  lv_label_set_text(backArrowText, LV_SYMBOL_LEFT);
  lv_obj_align(backArrowText, LV_ALIGN_CENTER, 0, 0);

  // Refresh button
  lv_obj_t *refreshButton = lv_btn_create(diagnosticScreen);
  lv_obj_align(refreshButton, LV_ALIGN_TOP_RIGHT, -10, 20);
  lv_obj_set_size(refreshButton, 40, 40);
  lv_obj_set_style_bg_color(refreshButton, lv_color_hex(BLACK), 0);
  lv_obj_set_style_bg_opa(refreshButton, LV_OPA_50, 0);
  lv_obj_add_event_cb(refreshButton, refreshDiagnostics, LV_EVENT_CLICKED, NULL);

  // Text in back arrow
  lv_obj_t *refreshText = lv_label_create(refreshButton);
  lv_obj_set_style_text_font(refreshText, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(refreshText, lv_color_hex(WHITE), 0);
  lv_label_set_text(refreshText, LV_SYMBOL_REFRESH);
  lv_obj_align(refreshText, LV_ALIGN_CENTER, 0, 0);

  // Square at bottom

  lv_obj_t *backgroundSquare = lv_obj_create(diagnosticScreen);
  lv_obj_set_size(backgroundSquare, 460, 240);
  lv_obj_align(backgroundSquare, LV_ALIGN_BOTTOM_LEFT, 10, -10);
  lv_obj_set_style_bg_color(backgroundSquare, lv_color_hex(BLACK), 0);
  lv_obj_set_style_bg_opa(backgroundSquare, LV_OPA_20, 0);

  // Left side text
  text = lv_label_create(backgroundSquare);
  lv_obj_set_style_text_font(text, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(text, lv_color_hex(WHITE), 0);
  lv_obj_align(text, LV_ALIGN_TOP_LEFT, 0, 0);

  // Return
  return diagnosticScreen;
}