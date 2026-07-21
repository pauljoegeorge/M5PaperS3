#pragma once
#include "secrets.h"

// ---- Credentials / endpoints (values live in secrets.h) ----
static const char* WIFI_SSID  = WIFI_SSID_SECRET;
static const char* WIFI_PASS  = WIFI_PASS_SECRET;
static const char* SCRIPT_URL = SCRIPT_URL_SECRET;   // Apps Script /exec URL

// ---- Weather (Open-Meteo, no API key needed) ----
static const float WEATHER_LAT = WEATHER_LAT_SECRET;    // coordinates live in secrets.h
static const float WEATHER_LON = WEATHER_LON_SECRET;
static const char* CITY_NAME   = "Tokyo";

// ---- News (Google News RSS, source rotates by time of day) ----
static const int TZ_OFFSET_HOURS = 9;     // JST; used for the clock + news slots
static const int NEWS_REFRESH_MIN = 60;   // re-fetch headlines (minutes)

struct NewsSlot { int startHour; const char* label; const char* url; };
// Must be sorted by startHour; the last slot wraps past midnight
static const NewsSlot NEWS_SLOTS[] = {
  {  5, "Tech",
     "https://news.google.com/rss/topics/CAAqKggKIiRDQkFTRlFvSUwyMHZNRGRqTVhZU0JXVnVMVlZUR2dKVlV5Z0FQAQ?hl=en-US&gl=US&ceid=US:en" },
  { 12, "Japan",
     "https://news.google.com/rss/search?q=Japan&hl=en-US&gl=US&ceid=US:en" },
  { 17, "World",
     "https://news.google.com/rss/topics/CAAqKggKIiRDQkFTRlFvSUwyMHZNRGx1YlY4U0JXVnVMVlZUR2dKVlV5Z0FQAQ?hl=en-US&gl=US&ceid=US:en" },
  { 21, "India",
     "https://news.google.com/rss?hl=en-IN&gl=IN&ceid=IN:en" },
};
static const int NEWS_SLOT_COUNT = sizeof(NEWS_SLOTS) / sizeof(NEWS_SLOTS[0]);

// ---- Quiet hours (no slideshow flips at night; touch still works) ----
static const int QUIET_START_HOUR = 0;    // midnight
static const int QUIET_END_HOUR   = 6;

// ---- Earthquake alerts (P2PQuake public API, covers JMA reports) ----
static const float EQ_MIN_MAG    = 3.0f;  // alert on magnitude >= this
static const int   EQ_WINDOW_MIN = 60;    // ...that happened within the last N minutes

// ---- Timing ----
static const int REFRESH_MIN   = 30;    // re-fetch calendar + weather (minutes)
static const int SLIDE_MIN     = 5;     // auto-flip between pages (minutes)
static const int TOUCH_POLL_MS = 400;   // touch poll interval; larger = better battery
