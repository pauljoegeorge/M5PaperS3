#pragma once
#include <WiFi.h>
#include <HTTPClient.h>
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
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  http.begin(url);
  int code = http.GET();
  Serial.printf("GET -> %d (%s...)\n", code, url.substring(0, 60).c_str());
  String body = (code == HTTP_CODE_OK) ? http.getString() : "";
  http.end();
  return body;
}

String weatherUrl() {
  String u = "https://api.open-meteo.com/v1/forecast";
  u += "?latitude=" + String(WEATHER_LAT, 4);
  u += "&longitude=" + String(WEATHER_LON, 4);
  u += "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m";
  u += "&hourly=temperature_2m,precipitation_probability";
  u += "&daily=weather_code,temperature_2m_max,temperature_2m_min,"
       "precipitation_probability_max,sunrise,sunset";
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

// Pull up to 2 not-yet-shown <item><title>s out of an RSS feed.
// Returns false if the feed had nothing new (current headlines are kept).
bool parseNews(const String& rss) {
  String fresh[2];
  int freshCount = 0;
  int pos = 0;
  while (freshCount < 2) {
    int item = rss.indexOf("<item>", pos);
    if (item < 0) break;
    int t0 = rss.indexOf("<title>", item);
    int t1 = rss.indexOf("</title>", t0);
    if (t0 < 0 || t1 < 0) break;
    String t = rss.substring(t0 + 7, t1);
    if (t.startsWith("<![CDATA[")) {
      t = t.substring(9);
      t.replace("]]>", "");
    }
    t = stripEmoji(decodeEntities(t));
    t.replace("\n", " ");
    int dash = t.lastIndexOf(" - ");
    if (dash > 10) t = t.substring(0, dash);    // drop " - Source Name" suffix
    t.trim();
    pos = t1;
    if (!t.length() || newsSeen(t)) continue;   // skip already-shown stories
    fresh[freshCount++] = t;
  }

  if (freshCount == 0) {
    Serial.println("News: nothing new, keeping current headlines");
    return false;
  }
  gNewsCount = freshCount;
  for (int i = 0; i < freshCount; i++) {
    gNews[i] = fresh[i];
    rememberNews(fresh[i]);
  }
  Serial.printf("News headlines: %d new\n", gNewsCount);
  return true;
}

// Which news slot applies right now.
// The last slot wraps past midnight, so it's also the default.
int currentNewsSlot() {
  int h = localHour();
  int slot = NEWS_SLOT_COUNT - 1;
  for (int i = 0; i < NEWS_SLOT_COUNT; i++) {
    if (h >= NEWS_SLOTS[i].startHour) slot = i;
  }
  return slot;
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
    if (jp.length() && en.length() && utf8Len(jp) <= 50 && en.length() <= 110) {
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

// Check P2PQuake for a recent quake of magnitude >= EQ_MIN_MAG.
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
    break;   // history is newest-first
  }
}

// Fetch calendar + weather in one WiFi session.
// Only overwrites the global docs on success, so stale-but-valid
// data survives a failed refresh. Returns true if calendar parsed.
bool refreshData() {
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

      // Pull out a special message event ("MSG: ..." title) and
      // hide it from the event list
      gMessage = "";
      JsonArray evs = calDoc["events"];
      for (size_t i = 0; i < evs.size(); i++) {
        String t = evs[i]["title"] | "";
        if (t.startsWith("MSG:")) {
          t.remove(0, 4);
          t.trim();
          gMessage = stripEmoji(t);
          evs.remove(i);
          break;
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

  // News: refresh hourly, or immediately when the time-of-day slot changes
  int slot = currentNewsSlot();
  if (gNewsCount == 0 || slot != gNewsSlot ||
      nowMs() - gLastNewsMs >= (int64_t)NEWS_REFRESH_MIN * 60000) {
    body = httpGET(NEWS_SLOTS[slot].url);
    if (body.length() && parseNews(body)) {
      gLastNewsMs = nowMs();
      gNewsSlot   = slot;
      gNewsLabel  = NEWS_SLOTS[slot].label;
    }
  }

  fetchWotd();
  checkQuakes();

  wifiOff();
  return calOk;
}
