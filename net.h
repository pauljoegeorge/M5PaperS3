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

// A write-only Stream that extracts <item><title>s from RSS flowing
// through it. Used with HTTPClient::writeToStream so the library handles
// chunked encoding / connection details; memory stays ~6 KB even though
// Google News feeds run 150-300 KB.
class RssTitleSink : public Stream {
 public:
  static const int MAX_TITLES = 16;
  String titles[MAX_TITLES];
  int count = 0;

  size_t write(uint8_t c) override { feed((const char*)&c, 1); return 1; }
  size_t write(const uint8_t* data, size_t len) override {
    feed((const char*)data, len);
    return len;   // claim it all so the transfer keeps going
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }

 private:
  String buf;
  bool inItem = false;

  void feed(const char* data, size_t len) {
    if (count >= MAX_TITLES) return;      // got enough, ignore the rest
    buf.concat(data, len);
    while (count < MAX_TITLES) {
      if (!inItem) {
        int ip = buf.indexOf("<item>");
        if (ip < 0) break;
        buf.remove(0, ip + 6);
        inItem = true;
      }
      int t0 = buf.indexOf("<title>");
      if (t0 < 0) break;
      int t1 = buf.indexOf("</title>", t0);
      if (t1 < 0) break;                  // title incomplete, wait for more data
      titles[count++] = buf.substring(t0 + 7, t1);
      buf.remove(0, t1 + 8);
      inItem = false;
    }
    // cap the buffer, keeping a tail in case a marker straddles chunks
    if (!inItem && buf.length() > 6000) buf.remove(0, buf.length() - 64);
  }
};

int fetchRssTitles(const char* url, String* titles, int maxTitles) {
  HTTPClient http;
  http.setTimeout(15000);

  String currentUrl = url;
  int code = -1;
  const char* headerKeys[] = {"Location"};

  for (int redirectCount = 0; redirectCount < 4; redirectCount++) {
    http.begin(currentUrl);
    http.collectHeaders(headerKeys, 1);
    code = http.GET();
    Serial.printf("GET -> %d (rss: %s...)\n", code, currentUrl.substring(0, 45).c_str());

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

  if (code != HTTP_CODE_OK) {
    gNewsDebug = "http error " + String(code);
    http.end();
    return -1;
  }

  RssTitleSink sink;
  int written = http.writeToStream(&sink);   // streams the whole body through us
  http.end();

  int n = (sink.count < maxTitles) ? sink.count : maxTitles;
  for (int i = 0; i < n; i++) titles[i] = sink.titles[i];
  Serial.printf("RSS titles read: %d (stream ret %d)\n", n, written);
  if (n == 0) gNewsDebug = "http 200, stream " + String(written) + ", 0 titles";
  return n;
}

// Refresh gNews with up to 2 not-yet-shown headlines.
// Returns false if the fetch failed or the feed had nothing new.
bool updateNews(const char* url) {
  String raw[16];
  int n = fetchRssTitles(url, raw, 16);
  if (n <= 0) return false;

  String fresh[2];
  int freshCount = 0;
  for (int i = 0; i < n && freshCount < 2; i++) {
    String t = cleanTitle(raw[i]);
    if (!t.length() || newsSeen(t)) continue;
    fresh[freshCount++] = t;
  }
  if (freshCount == 0) {
    Serial.println("News: nothing new, keeping current headlines");
    gNewsDebug = String(n) + " titles, all seen";
    return false;
  }
  gNewsCount = freshCount;
  for (int i = 0; i < freshCount; i++) {
    gNews[i] = fresh[i];
    rememberNews(fresh[i]);
  }
  gNewsDebug = "";
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

  // News: refresh hourly, or immediately when the time-of-day slot changes
  int slot = currentNewsSlot();
  if (gNewsCount == 0 || slot != gNewsSlot ||
      nowMs() - gLastNewsMs >= (int64_t)NEWS_REFRESH_MIN * 60000) {
    if (updateNews(NEWS_SLOTS[slot].url)) {
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
