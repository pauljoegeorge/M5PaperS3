/**
 * M5PaperS3 — Calendar / Weather / News / Word-of-the-day e-ink dashboard
 *
 * Pages (slideshow, flips every SLIDE_MIN minutes):
 *   Calendar -> Weather -> News -> Word of the day -> Message (if set) -> ...
 *   An earthquake alert page (M >= EQ_MIN_MAG, last EQ_WINDOW_MIN minutes)
 *   takes over the screen when a quake is detected.
 *
 * Touch:
 *   - tap RIGHT half  -> next page
 *   - tap LEFT half   -> previous page
 *   - LONG PRESS (~1.5 s) anywhere -> re-fetch everything now
 *
 * Quiet hours (QUIET_START_HOUR..QUIET_END_HOUR): no automatic page
 * flips at night; touch still works, data still refreshes.
 *
 * Special message: calendar event today titled "MSG: your text".
 *
 * Board: "M5PaperS3" (M5Stack boards package)
 * Libraries: M5GFX, ArduinoJson (v7)
 */

#include <M5GFX.h>
#include <ArduinoJson.h>
#include "config.h"
#include "util.h"
#include "net.h"
#include "page_calendar.h"
#include "page_weather.h"
#include "page_news.h"
#include "page_word.h"
#include "page_countdown.h"
#include "page_message.h"
#include "page_quake.h"

enum Page { PAGE_CALENDAR, PAGE_WEATHER, PAGE_NEWS, PAGE_WORD,
            PAGE_CNT, PAGE_MESSAGE, PAGE_QUAKE };

Page currentPage = PAGE_CALENDAR;
Page pageOrder[7];
int  pageCount = 0;

int64_t lastFetchMs = 0;
int64_t lastFlipMs  = 0;
String  shownQuakeTime;   // which quake we already jumped to

// Rebuild the rotation after every data refresh (pages appear/disappear
// depending on content)
void buildPageList() {
  pageCount = 0;
  if (gQuakeActive)       pageOrder[pageCount++] = PAGE_QUAKE;
  pageOrder[pageCount++] = PAGE_CALENDAR;
  pageOrder[pageCount++] = PAGE_WEATHER;
  pageOrder[pageCount++] = PAGE_NEWS;   // always shown; page explains itself when empty
  pageOrder[pageCount++] = PAGE_WORD;
  if (gCntCount)          pageOrder[pageCount++] = PAGE_CNT;
  if (gMessage.length())  pageOrder[pageCount++] = PAGE_MESSAGE;
}

int pageIndex(Page p) {
  for (int i = 0; i < pageCount; i++) {
    if (pageOrder[i] == p) return i;
  }
  return 0;   // page vanished from rotation -> restart at the top
}

Page nextPage(Page p) { return pageOrder[(pageIndex(p) + 1) % pageCount]; }
Page prevPage(Page p) { return pageOrder[(pageIndex(p) + pageCount - 1) % pageCount]; }

void renderCurrentPage() {
  switch (currentPage) {
    case PAGE_CALENDAR: renderCalendar(); break;
    case PAGE_WEATHER:  renderWeather();  break;
    case PAGE_NEWS:     renderNews();     break;
    case PAGE_WORD:     renderWord();     break;
    case PAGE_CNT:      renderCountdown(); break;
    case PAGE_MESSAGE:  renderMessage();  break;
    case PAGE_QUAKE:    renderQuake();    break;
  }
  d.waitDisplay();
}

// Fetch everything, rebuild the rotation, jump to a fresh quake alert
void doRefresh() {
  refreshData();
  buildPageList();
  if (gQuakeActive && gQuakeTime != shownQuakeTime) {
    currentPage = PAGE_QUAKE;          // new quake -> take over the screen
    shownQuakeTime = gQuakeTime;
  } else if (pageIndex(currentPage) == 0 && pageOrder[0] != currentPage) {
    currentPage = pageOrder[0];        // current page left the rotation
  }
  lastFetchMs = nowMs();
  lastFlipMs  = nowMs();
}

void setup() {
  Serial.begin(115200);

  d.init();
  if (d.isEPD()) d.setEpdMode(epd_mode_t::epd_quality);
  if (d.width() < d.height()) d.setRotation(d.getRotation() ^ 1);  // landscape
  W = d.width();
  H = d.height();

  showMessage("Loading...");
  doRefresh();
  renderCurrentPage();
}

void loop() {
  // Nap briefly, then wake to poll touch and check timers
  esp_sleep_enable_timer_wakeup((uint64_t)TOUCH_POLL_MS * 1000ULL);
  esp_err_t slept = esp_light_sleep_start();
  if (slept != ESP_OK) {
    static int rejected = 0;
    if (++rejected <= 5) Serial.printf("light sleep rejected: %d\n", slept);
    delay(TOUCH_POLL_MS);   // fall back so the loop doesn't spin at full speed
  }

  // ---- Touch: tap = switch page, long press = refresh now ----
  lgfx::touch_point_t tp;
  if (d.getTouch(&tp)) {
    int tx = tp.x;
    int64_t t0 = nowMs();
    while (d.getTouch(&tp)) delay(30);       // wait for finger release
    if (nowMs() - t0 >= 1500) {
      showMessage("Refreshing...");
      doRefresh();
    } else {
      currentPage = (tx >= W / 2) ? nextPage(currentPage) : prevPage(currentPage);
      lastFlipMs = nowMs();
    }
    renderCurrentPage();
    return;
  }

  // ---- Quiet hours: true deep sleep until morning (screen holds image,
  //      touch inactive; device reboots and refreshes at QUIET_END_HOUR) ----
  if (quietHours() && time(nullptr) > 1600000000) {   // only with a synced clock
    int minsNow  = (int)((time(nullptr) / 60 + (time_t)TZ_OFFSET_HOURS * 60) % 1440);
    int sleepMin = QUIET_END_HOUR * 60 - minsNow;
    if (sleepMin <= 0) sleepMin += 1440;
    Serial.printf("Night: deep sleeping %d min\n", sleepMin);
    d.waitDisplay();
    WiFi.mode(WIFI_OFF);
    esp_sleep_enable_timer_wakeup((uint64_t)sleepMin * 60ULL * 1000000ULL);
    esp_deep_sleep_start();
  }

  int64_t t = nowMs();

  // ---- Periodic data refresh ----
  if (t - lastFetchMs >= (int64_t)REFRESH_MIN * 60000) {
    doRefresh();
    renderCurrentPage();
    return;
  }

  // ---- Slideshow auto-flip (paused during quiet hours) ----
  if (!quietHours() && t - lastFlipMs >= (int64_t)SLIDE_MIN * 60000) {
    currentPage = nextPage(currentPage);
    lastFlipMs = nowMs();
    renderCurrentPage();
  }
}
