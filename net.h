#pragma once
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_heap_caps.h>
#include "config.h"
#include "util.h"
#include "word_data.h"

bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for (int i = 0; i < 40; i++) {          // ~20 s timeout
    if (WiFi.status() == WL_CONNECTED) return true;
    delay(500);
  }
  return false;
}

void wifiOff() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

String httpGET(const String& url) {
  HTTPClient http;
  http.setTimeout(15000);

  String currentUrl = url;
  int code = -1;
  const char* headerKeys[] = {"Location"};

  for (int redirectCount = 0; redirectCount < 4; redirectCount++) {
    http.begin(currentUrl);
    http.collectHeaders(headerKeys, 1);
    code = http.GET();
    Serial.printf("GET -> %d (%s...)\n", code, currentUrl.substring(0, 60).c_str());

    if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
      String newUrl = http.header("Location");
      http.end();
      if (newUrl.length() > 0) {
        currentUrl = newUrl;
        continue;
      }
    }
    break;
  }

  String body = (code == HTTP_CODE_OK) ? http.getString() : "";
  http.end();
  return body;
}

String weatherUrl() {
  String u = "https://api.open-meteo.com/v1/forecast";
  u += "?latitude=" + String(WEATHER_LAT, 4);
  u += "&longitude=" + String(WEATHER_LON, 4);
  u += "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m";
  u += "&hourly=temperature_2m,precipitation";
  u += "&daily=weather_code,temperature_2m_max,temperature_2m_min,"
       "precipitation_sum,sunrise,sunset";
  u += "&timezone=auto&forecast_days=1";
  return u;
}

String decodeEntities(String s) {
  s.replace("&amp;", "&");
  s.replace("&lt;", "<");
  s.replace("&gt;", ">");
  s.replace("&quot;", "\"");
  s.replace("&#39;", "'");
  s.replace("&apos;", "'");
  return s;
}

String cleanTitle(String t) {
  if (t.startsWith("<![CDATA[")) {
    t = t.substring(9);
    t.replace("]]>", "");
  }
  t = stripEmoji(decodeEntities(t));
  t.replace("\n", " ");
  int dash = t.lastIndexOf(" - ");
  if (dash > 10) t = t.substring(0, dash);    // drop " - Source Name" suffix
  t.trim();
  return t;
}

#include "news_core.h"

NewsArticle* gNewsCache = nullptr;
int gNewsCacheCount = 0;
int gNewsPosition = 0;
int64_t gNewsRotateMs = 0;
int64_t gNewsNextFetchMs = 0;
int gNewsAttemptSlot = -1;

int currentNewsSlot() {
  int slot = 0;
  for (int i = 0; i < NEWS_SLOT_COUNT; ++i)
    if (localHour() >= NEWS_SLOTS[i].startHour) slot = i;
  return slot;
}

bool newsNeedsFetch() {
  return currentNewsSlot() != gNewsAttemptSlot || nowMs() >= gNewsNextFetchMs;
}

void logNewsMemory(const char* stage) {
  Serial.printf("News %s: heap %u, largest %u, PSRAM %u\n", stage,
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

void showNewsBatch() {
  gNewsCount = 0;
  for (int i = gNewsPosition; i < gNewsCacheCount && gNewsCount < 2; ++i)
    gNews[gNewsCount++] = gNewsCache[i].title;
  for (int i = gNewsCount; i < 3; ++i) gNews[i] = "";
}

// Also expire old entries after a failed refresh or a GMT date rollover.
bool rotateNews() {
  int oldCount = gNewsCacheCount;
  uint64_t anchor = oldCount ? gNewsCache[gNewsPosition].titleHash : 0;
  int retained = 0;
  for (int i = 0; i < oldCount; ++i)
    if (newsEligible(gNewsCache[i].published, time(nullptr)))
      gNewsCache[retained++] = gNewsCache[i];
  gNewsCacheCount = retained;
  bool changed = oldCount != retained;
  if (changed) {
    gNewsPosition = 0;
    for (int i = 0; i < retained; ++i)
      if (gNewsCache[i].titleHash == anchor) gNewsPosition = i;
  }
  if (nowMs() - gNewsRotateMs >= (int64_t)NEWS_ROTATE_MIN * 60000) {
    if (retained) gNewsPosition = newsNextPosition(gNewsPosition, retained);
    gNewsRotateMs = nowMs();
    changed = true;
  }
  if (changed) showNewsBatch();
  return changed;
}

#include "news_rss.h"

bool updateNews(const char* url) {
  logNewsMemory("before fetch");
  NewsArticle* replacement = (NewsArticle*)heap_caps_malloc(
    sizeof(NewsArticle) * NEWS_CAPACITY, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!replacement) { gNewsDebug = "News cache needs PSRAM"; return false; }
  HTTPClient http;
  http.setTimeout(15000);
  http.setConnectTimeout(15000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setRedirectLimit(4);
  if (!http.begin(url)) {
    free(replacement); gNewsDebug = "News connection failed"; return false;
  }
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    gNewsDebug = "http error " + String(code);
    http.end(); free(replacement); return false;
  }
  RssNewsSink sink(replacement);
  int written = http.writeToStream(&sink);
  logNewsMemory("during fetch");
  http.end();
  if (written < 0 || !sink.complete) {
    gNewsDebug = "Incomplete news response";
    free(replacement); return false;
  }
  uint64_t anchor = gNewsCacheCount ? gNewsCache[gNewsPosition].titleHash : 0;
  free(gNewsCache);
  gNewsCache = replacement;
  gNewsCacheCount = sink.count;
  gNewsPosition = 0;
  for (int i = 0; i < sink.count; ++i)
    if (replacement[i].titleHash == anchor) gNewsPosition = i;
  showNewsBatch();
  if (!anchor) gNewsRotateMs = nowMs();
  gLastNewsMs = nowMs();
  gNewsDebug = sink.count ? "" : "No articles from the last 3 GMT dates";
  Serial.printf("News: %d items, %d retained; cache %u bytes\n", sink.items,
    sink.count, (unsigned)(sizeof(NewsArticle) * NEWS_CAPACITY));
  logNewsMemory("after fetch");
  return true;
}

void selectNewsSlot() {
  int slot = currentNewsSlot();
  if (slot != gNewsSlot) {
    gNewsSlot = slot; gNewsLabel = NEWS_SLOTS[slot].label;
    free(gNewsCache); gNewsCache = nullptr; gNewsCacheCount = 0;
    gNewsPosition = 0; gNewsRotateMs = nowMs(); showNewsBatch();
  }
}

void refreshNews(bool force = false) {
  if (!force && !newsNeedsFetch()) return;
  selectNewsSlot();
  int slot = gNewsSlot;
  gNewsAttemptSlot = slot;
  bool success = updateNews(NEWS_SLOTS[slot].url);
  gNewsNextFetchMs = nowMs() + (int64_t)(success ? NEWS_REFRESH_MIN : NEWS_RETRY_MIN) * 60000;
  rotateNews();
}

String urlEncode(const String& s) {
  const char* hex = "0123456789ABCDEF";
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = s[i];
    if (isalnum(c)) out += (char)c;
    else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
  }
  return out;
}

int utf8Len(const String& s) {
  int n = 0;
  for (size_t i = 0; i < s.length(); n++) {
    uint8_t c = s[i];
    i += (c < 0x80) ? 1 : ((c & 0xE0) == 0xC0) ? 2 : ((c & 0xF0) == 0xE0) ? 3 : 4;
  }
  return n;
}

int wotdToday() {
  return (int)((time(nullptr) + (time_t)TZ_OFFSET_HOURS * 3600) / 86400);
}

void wotdFallback(int day) {
  const WotdEntry& w = WOTD[day % WOTD_COUNT];
  gWotdWord    = w.word;
  gWotdReading = w.reading;
  gWotdMeaning = w.meaning;
  gWotdUsage   = w.usage;
  gWotdUsageEn = w.usage_en;
  gWotdUsageRuby = "";
  gWotdLevel   = "N3-N2";
}

// Word of the day: pick a JLPT-tagged word from Jisho (N3/N2 alternating
// daily) and an example sentence from Tatoeba. If either API fails or has
// no screen-sized example, today's curated fallback word stays.
void fetchWotd() {
  int day = wotdToday();
  if (day == gWotdDay && gWotdWord.length()) return;   // already have today's
  wotdFallback(day);                                    // provisional word

  const char* level = (day & 1) ? "n2" : "n3";
  String url = "https://jisho.org/api/v1/search/words?keyword=%23jlpt-";
  url += level;
  url += "&page=";
  url += String(1 + (day * 7) % 40);   // pseudo-shuffled page of 20 words
  String body = httpGET(url);
  if (!body.length()) return;

  JsonDocument filter;
  filter["data"][0]["japanese"][0]["word"] = true;
  filter["data"][0]["japanese"][0]["reading"] = true;
  filter["data"][0]["senses"][0]["english_definitions"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return;
  JsonArray data = doc["data"];
  if (data.size() == 0) return;

  JsonObject entry = data[day % data.size()];
  String word    = entry["japanese"][0]["word"]    | "";
  String reading = entry["japanese"][0]["reading"] | "";
  if (!word.length()) word = reading;               // kana-only words
  if (!word.length()) return;

  String meaning;
  JsonArray defs = entry["senses"][0]["english_definitions"];
  for (size_t i = 0; i < defs.size() && i < 3; i++) {
    if (meaning.length()) meaning += "; ";
    meaning += (const char*)defs[i];
  }
  if (!meaning.length()) return;

  // Example sentence with translation from Tatoeba
  body = httpGET("https://tatoeba.org/en/api_v0/search?from=jpn&to=eng"
                 "&sort=relevance&query=" + urlEncode(word));
  if (!body.length()) return;

  JsonDocument f2;
  f2["results"][0]["text"] = true;
  f2["results"][0]["translations"][0][0]["text"] = true;
  f2["results"][0]["transcriptions"][0]["text"] = true;   // furigana markup
  JsonDocument d2;
  if (deserializeJson(d2, body, DeserializationOption::Filter(f2))) return;

  String usage, usageEn, usageRuby;
  for (JsonObject r : d2["results"].as<JsonArray>()) {
    String jp = r["text"] | "";
    if (jp.indexOf(word) < 0) continue;   // must actually contain today's word
    String en;
    for (JsonArray grp : r["translations"].as<JsonArray>()) {
      if (grp.size()) {
        en = (const char*)(grp[0]["text"] | "");
        if (en.length()) break;
      }
    }
    // must fit two lines on screen
    if (jp.length() && en.length() && utf8Len(jp) <= 44 && en.length() <= 110) {
      usage = jp;
      usageEn = en;
      usageRuby = (const char*)(r["transcriptions"][0]["text"] | "");
      break;
    }
  }
  if (!usage.length()) return;   // no usable example -> keep curated word

  gWotdWord    = word;
  gWotdReading = reading;
  gWotdMeaning = meaning;
  gWotdUsage   = usage;
  gWotdUsageEn = usageEn;
  gWotdUsageRuby = usageRuby;
  gWotdLevel   = (day & 1) ? "N2" : "N3";
  gWotdDay     = day;
  Serial.printf("WOTD from Jisho: %s (%s)\n", word.c_str(), gWotdLevel.c_str());
}

// Check P2PQuake for recent quakes of magnitude >= EQ_MIN_MAG within the
// last EQ_WINDOW_MIN minutes, and surface the biggest one by magnitude.
// Sets/clears the gQuake* globals.
void checkQuakes() {
  String body = httpGET("https://api.p2pquake.net/v2/history?codes=551&limit=10");
  if (!body.length()) return;

  JsonDocument filter;
  filter[0]["earthquake"]["time"] = true;
  filter[0]["earthquake"]["maxScale"] = true;
  filter[0]["earthquake"]["hypocenter"]["name"] = true;
  filter[0]["earthquake"]["hypocenter"]["magnitude"] = true;

  JsonDocument doc;
  if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return;

  gQuakeActive = false;
  for (JsonObject q : doc.as<JsonArray>()) {
    float mag = q["earthquake"]["hypocenter"]["magnitude"] | -1.0f;
    if (mag < EQ_MIN_MAG) continue;
    if (gQuakeActive && mag <= gQuakeMag) continue;   // keep the biggest so far

    // "2026/07/17 12:34:56.789" (JST) -> UTC epoch
    String ts = q["earthquake"]["time"] | "";
    struct tm tmv = {};
    if (sscanf(ts.c_str(), "%d/%d/%d %d:%d:%d",
               &tmv.tm_year, &tmv.tm_mon, &tmv.tm_mday,
               &tmv.tm_hour, &tmv.tm_min, &tmv.tm_sec) != 6) continue;
    tmv.tm_year -= 1900;
    tmv.tm_mon  -= 1;
    time_t utc = mktime(&tmv) - TZ_OFFSET_HOURS * 3600;
    if (time(nullptr) - utc > (time_t)EQ_WINDOW_MIN * 60) continue;

    gQuakeActive = true;
    gQuakeMag    = mag;
    gQuakeScale  = q["earthquake"]["maxScale"] | -1;
    gQuakePlace  = (const char*)(q["earthquake"]["hypocenter"]["name"] | "");
    gQuakeTime   = ts.substring(11, 16);
    Serial.printf("Quake: M%.1f %s\n", mag, gQuakePlace.c_str());
  }
}

// Reads width/height straight from the JPEG's SOF marker (no full decode
// needed) — used to compute a crop-to-cover zoom factor ourselves, since
// M5GFX's public drawJpg() doesn't expose decoded dimensions ahead of time.
bool getJpegSize(const uint8_t* buf, size_t len, int* outW, int* outH) {
  if (len < 4 || buf[0] != 0xFF || buf[1] != 0xD8) return false;
  size_t i = 2;
  while (i + 4 <= len) {
    if (buf[i] != 0xFF) { i++; continue; }
    uint8_t marker = buf[i + 1];
    if (marker == 0xFF) { i++; continue; }   // fill byte before the real marker
    if (marker == 0xD8 || marker == 0xD9 || (marker >= 0xD0 && marker <= 0xD7)) {
      i += 2;
      continue;
    }
    if (i + 4 > len) break;
    uint16_t segLen = (buf[i + 2] << 8) | buf[i + 3];
    bool isSOF = marker >= 0xC0 && marker <= 0xCF &&
                 marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
    if (isSOF) {
      if (i + 9 > len) return false;
      *outH = (buf[i + 5] << 8) | buf[i + 6];
      *outW = (buf[i + 7] << 8) | buf[i + 8];
      return true;
    }
    i += 2 + segLen;
  }
  return false;
}

// Refresh the Photo page's image from the Apps Script "?photo=1" endpoint,
// which returns the Drive file ID of the newest photo in a folder you can
// drop new pictures into any time. Only re-downloads/decodes when that ID
// actually changes, since JPEG decode + dithering is the most expensive
// step on this device and the photo rarely changes between refreshes.
void fetchPhoto() {
  String body = httpGET(String(SCRIPT_URL) + "?photo=1");
  if (!body.length()) return;

  JsonDocument filter;
  filter["id"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return;

  String id = doc["id"] | "";
  if (!id.length() || id == gPhotoId) return;   // no photo, or unchanged

  // Fetch the resized JPEG straight from Google's content CDN, bypassing
  // drive.google.com/thumbnail (which redirects here anyway, but forces
  // the size down to ~160px for anonymous requests regardless of the sz=
  // param). Requesting this URL directly, with our own size suffix, gets
  // the real resize.
  HTTPClient http;
  http.setTimeout(20000);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.begin("https://lh3.googleusercontent.com/d/" + id + "=w960");
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("Photo fetch failed: HTTP %d\n", code);
    http.end();
    return;
  }

  int len = http.getSize();
  if (len <= 0) {
    Serial.println("Photo fetch: no content-length");
    http.end();
    return;
  }
  uint8_t* buf = (uint8_t*)heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
  if (!buf) {
    Serial.printf("Photo: PSRAM alloc failed for %d bytes\n", len);
    http.end();
    return;
  }

  WiFiClient* stream = http.getStreamPtr();
  int got = 0;
  while (got < len && http.connected()) {
    size_t avail = stream->available();
    if (avail) {
      got += stream->readBytes(buf + got, min((size_t)(len - got), avail));
    } else {
      delay(1);
    }
  }
  http.end();

  if (got != len) {                    // incomplete download -> try again next refresh
    Serial.printf("Photo: incomplete download (%d/%d bytes)\n", got, len);
    free(buf);
    return;
  }

  if (!gPhotoSprite.getBuffer()) {
    gPhotoSprite.setColorDepth(lgfx::color_depth_t::grayscale_8bit);
    gPhotoSprite.setPsram(true);
    gPhotoSprite.createSprite(W, H);
  }
  gPhotoSprite.fillScreen(TFT_WHITE);   // fallback padding if size parsing below fails

  int imgW = 0, imgH = 0;
  bool ok;
  if (getJpegSize(buf, len, &imgW, &imgH) && imgW > 0 && imgH > 0) {
    // One uniform zoom (same for both axes, so no distortion), blended
    // between "fit whole image, may letterbox" and "fill completely,
    // crop the overflow" by PHOTO_FILL (config.h).
    float containZoom = min((float)W / imgW, (float)H / imgH);
    float coverZoom   = max((float)W / imgW, (float)H / imgH);
    float zoom = containZoom + PHOTO_FILL * (coverZoom - containZoom);
    ok = gPhotoSprite.drawJpg(buf, len, 0, 0, W, H, 0, 0, zoom, zoom, middle_center);
  } else {
    // Couldn't read dimensions -> fall back to fit-within (may letterbox)
    ok = gPhotoSprite.drawJpg(buf, len, 0, 0, W, H, 0, 0, 0.0f, 0.0f, middle_center);
  }
  free(buf);

  if (ok) {
    ditherSprite(gPhotoSprite, W, H);
    gPhotoId = id;
    Serial.printf("Photo updated: %s\n", id.c_str());
  } else {
    Serial.println("Photo JPEG decode failed");
  }
}

// Fetch calendar + weather in one WiFi session.
// Only overwrites the global docs on success, so stale-but-valid
// data survives a failed refresh. Returns true if calendar parsed.
bool refreshData(bool includePhoto) {
  if (!connectWiFi()) {
    Serial.println("WiFi failed");
    return false;
  }
  Serial.print("WiFi OK, IP: ");
  Serial.println(WiFi.localIP());

  // One-time NTP clock sync (needed to pick the right news slot)
  if (time(nullptr) < 1600000000) {
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    for (int i = 0; i < 20 && time(nullptr) < 1600000000; i++) delay(250);
    Serial.printf("Clock synced: %ld\n", (long)time(nullptr));
  }

  bool calOk = false;
  String body = httpGET(SCRIPT_URL);
  if (body.length()) {
    JsonDocument tmp;
    if (!deserializeJson(tmp, body)) {
      calDoc = tmp;
      gUpdated = calDoc["fetched"] | "";
      calOk = true;

      // Pull out special events (hidden from the event list):
      //   "MSG: text"                  -> message page
      //   "CNT: name / YYYY-MM-DD"     -> countdown page
      gMessage = "";
      gCntCount = 0;
      JsonArray evs = calDoc["events"];
      for (size_t i = 0; i < evs.size();) {
        String t = evs[i]["title"] | "";
        if (t.startsWith("MSG:") && !gMessage.length()) {
          t.remove(0, 4);
          t.trim();
          gMessage = t;
          evs.remove(i);
          continue;
        }
        if (t.startsWith("CNT:") && gCntCount < 4) {
          String spec = t.substring(4);
          int slash = spec.lastIndexOf('/');
          int yy, mm, dd2;
          if (slash > 0 &&
              sscanf(spec.substring(slash + 1).c_str(), "%d-%d-%d", &yy, &mm, &dd2) == 3) {
            String name = spec.substring(0, slash);
            name.trim();
            struct tm tmv = {};
            tmv.tm_year = yy - 1900;
            tmv.tm_mon  = mm - 1;
            tmv.tm_mday = dd2;
            tmv.tm_hour = 12;                       // midday avoids DST/rounding edges
            int target = (int)(mktime(&tmv) / 86400);
            int days = target - wotdToday();        // local day index
            if (days >= 0 && name.length()) {
              gCntName[gCntCount] = name;
              gCntDate[gCntCount] = spec.substring(slash + 1);
              gCntDate[gCntCount].trim();
              gCntDays[gCntCount] = days;
              gCntCount++;
            }
          }
          evs.remove(i);
          continue;
        }
        i++;
      }
      // Preferred source: "countdowns" array from the Apps Script —
      // future events titled "CNT: name" placed on their actual date.
      for (JsonObject c : calDoc["countdowns"].as<JsonArray>()) {
        if (gCntCount >= 4) break;
        String name = c["name"] | "";
        String date = c["date"] | "";
        int yy, mm, dd2;
        if (!name.length() ||
            sscanf(date.c_str(), "%d-%d-%d", &yy, &mm, &dd2) != 3) continue;
        struct tm tmv = {};
        tmv.tm_year = yy - 1900;
        tmv.tm_mon  = mm - 1;
        tmv.tm_mday = dd2;
        tmv.tm_hour = 12;
        int days = (int)(mktime(&tmv) / 86400) - wotdToday();
        if (days < 0) continue;
        gCntName[gCntCount] = name;
        gCntDate[gCntCount] = date;
        gCntDays[gCntCount] = days;
        gCntCount++;
      }

      // sort countdowns soonest-first
      for (int a = 0; a < gCntCount; a++) {
        for (int b = a + 1; b < gCntCount; b++) {
          if (gCntDays[b] < gCntDays[a]) {
            std::swap(gCntDays[a], gCntDays[b]);
            std::swap(gCntName[a], gCntName[b]);
            std::swap(gCntDate[a], gCntDate[b]);
          }
        }
      }
    } else {
      Serial.println("Calendar JSON parse error");
    }
  }

  body = httpGET(weatherUrl());
  if (body.length()) {
    JsonDocument tmp;
    if (!deserializeJson(tmp, body)) {
      wxDoc = tmp;
    } else {
      Serial.println("Weather JSON parse error");
    }
  }

  refreshNews(includePhoto);

  fetchWotd();
  checkQuakes();
  if (includePhoto) fetchPhoto();   // photo only refreshed on demand (boot + long-press), not every periodic refresh

  wifiOff();
  return calOk;
}
