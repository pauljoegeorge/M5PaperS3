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
static const float RAIN_MM_MIN        = 0.2f;  // an hour "has rain" at/above this (mm)
static const float RAIN_FULL_SCALE_MM = 6.0f;  // rainfall that fills the chart bar height

// ---- News (schedule in JST; article dates filtered in GMT) ----
static const int TZ_OFFSET_HOURS = 9;
static const int NEWS_REFRESH_MIN = 60;
static const int NEWS_ROTATE_MIN = 15;
static const int NEWS_RETRY_MIN = 15;
struct NewsSlot { int startHour; const char* label; const char* url; };
// Tech wraps past midnight; quiet hours still suspend all operations.
static const NewsSlot NEWS_SLOTS[] = {
  { 0, "Tech", "https://news.google.com/rss/topics/CAAqJggKIiBDQkFTRWdvSUwyMHZNRGRqTVhZU0FtVnVHZ0pWVXlnQVAB?hl=en-US&gl=US&ceid=US:en" },
  {10, "Science", "https://news.google.com/rss/topics/CAAqJggKIiBDQkFTRWdvSUwyMHZNRFp0Y1RjU0FtVnVHZ0pWVXlnQVAB?hl=en-US&gl=US&ceid=US:en" },
  {14, "Japan", "https://news.google.com/rss/search?q=Japan&hl=en-US&gl=US&ceid=US:en" },
  {15, "Chennai", "https://news.google.com/rss/search?q=Chennai&hl=en-IN&gl=IN&ceid=IN:en" },
  {17, "Kerala", "https://news.google.com/rss/search?q=Kerala&hl=en-IN&gl=IN&ceid=IN:en" },
  {19, "India", "https://news.google.com/rss?hl=en-IN&gl=IN&ceid=IN:en" },
  {20, "International", "https://news.google.com/rss/topics/CAAqJggKIiBDQkFTRWdvSUwyMHZNRGx1YlY4U0FtVnVHZ0pWVXlnQVAB?hl=en-US&gl=US&ceid=US:en" },
};
static const int NEWS_SLOT_COUNT = sizeof(NEWS_SLOTS) / sizeof(NEWS_SLOTS[0]);

// ---- Quiet hours (no slideshow flips at night; touch still works) ----
static const int QUIET_START_HOUR = 23;   // 11 PM
static const int QUIET_END_HOUR   = 6;

// ---- Earthquake alerts (P2PQuake public API, covers JMA reports) ----
static const float EQ_MIN_MAG    = 3.0f;  // alert on magnitude >= this
static const int   EQ_WINDOW_MIN = 60;    // ...that happened within the last N minutes
static const int   EQ_CHECK_MIN  = 5;     // earthquake-only Wi-Fi check interval (minutes)
static_assert(EQ_CHECK_MIN > 0, "EQ_CHECK_MIN must be positive");

// ---- Photo page ----
// How much to zoom into the photo: 0.0 = show the whole image, letterboxed
// if its aspect ratio doesn't match the panel; 1.0 = zoom until it fills the
// panel completely, cropping whichever side overflows. Values in between
// blend the two — a partial zoom that shows more letterbox-free area
// without cropping as aggressively as full cover.
static const float PHOTO_FILL = 0.3f;

// ---- Calendar ----
static const int CALENDAR_TOMORROW_HOUR = 18; // JST; also wait for all today's visible events to end

// ---- Timing ----
static const int REFRESH_MIN  = 30;     // re-fetch calendar + weather (minutes)
static const int SLIDE_MIN    = 5;      // auto-flip between pages (minutes)
// Touch is now caught by a GPIO interrupt (near-instant), so this is just
// a fallback nap length / periodic timer check — safe to set higher than
// the old 400ms poll interval.
static const int TOUCH_POLL_MS = 2000;
