#pragma once
#include <M5GFX.h>
#include <ArduinoJson.h>
#include <sys/time.h>
#include "config.h"

// ---- Globals shared by all pages ----
M5GFX d;
int W = 960, H = 540;

JsonDocument calDoc;    // last successfully parsed calendar payload
JsonDocument wxDoc;     // last successfully parsed weather payload
String gUpdated;        // "HH:MM" of last successful fetch
String gMessage;        // special message from a "MSG: ..." calendar event

// Countdown targets from "CNT: name / YYYY-MM-DD" calendar events
String gCntName[4];
String gCntDate[4];
int gCntDays[4];
int gCntCount = 0;
String gNews[3];        // latest headlines
String gNewsLabel;      // which feed they came from ("Tech", "Japan", ...)
String gNewsDebug;      // last fetch outcome, shown on-screen when empty
int gNewsCount = 0;
int gNewsSlot = -1;
int64_t gLastNewsMs = 0;

// Ring buffer of recently shown headlines, so refreshes bring new stories
String gSeenNews[16];
int gSeenPos = 0;

bool newsSeen(const String& t) {
  for (int i = 0; i < 16; i++) {
    if (gSeenNews[i] == t) return true;
  }
  return false;
}

void rememberNews(const String& t) {
  gSeenNews[gSeenPos] = t;
  gSeenPos = (gSeenPos + 1) % 16;
}

// Word of the day (Jisho + Tatoeba, falls back to word_data.h)
String gWotdWord, gWotdReading, gWotdMeaning, gWotdUsage, gWotdUsageEn, gWotdLevel;
String gWotdUsageRuby;   // example with furigana markers: 漢字[かんじ]...
int gWotdDay = -1;   // local day the API word was fetched for

// Earthquake alert state
bool   gQuakeActive = false;
float  gQuakeMag = 0;
int    gQuakeScale = -1;      // JMA maxScale (x10)
String gQuakePlace;           // epicenter name (Japanese)
String gQuakeTime;            // "HH:MM"

// Monotonic-enough clock that keeps counting through light sleep
int64_t nowMs() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// Local hour of day (fixed TZ offset; valid once NTP has synced)
int localHour() {
  return (int)((time(nullptr) / 3600 + TZ_OFFSET_HOURS) % 24);
}

bool quietHours() {
  int h = localHour();
  return (QUIET_START_HOUR > QUIET_END_HOUR)
             ? (h >= QUIET_START_HOUR || h < QUIET_END_HOUR)
             : (h >= QUIET_START_HOUR && h < QUIET_END_HOUR);
}

// Battery on GPIO3 through a 2:1 divider (same as M5Unified's PaperS3 config).
// LiPo: ~4.15 V full, ~3.30 V empty. Approximate, reads high while charging.
int batteryPercent() {
  uint32_t mv = 0;
  for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(3);
  mv = (mv / 8) * 2;
  int pct = (int)(((int)mv - 3300) * 100 / (4150 - 3300));
  return constrain(pct, 0, 100);
}

uint32_t utf8At(const String& s, int i, int* len) {
  uint8_t c = s[i];
  if (c < 0x80)            { *len = 1; return c; }
  if ((c & 0xE0) == 0xC0)  { *len = 2; return ((uint32_t)(c & 0x1F) << 6)  | (s[i+1] & 0x3F); }
  if ((c & 0xF0) == 0xE0)  { *len = 3; return ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(s[i+1] & 0x3F) << 6) | (s[i+2] & 0x3F); }
  *len = 4;
  return ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(s[i+1] & 0x3F) << 12) |
         ((uint32_t)(s[i+2] & 0x3F) << 6) | (s[i+3] & 0x3F);
}

// Codepoints the DejaVu/Japanese fonts can't draw (emoji & friends)
bool isEmojiCp(uint32_t cp) {
  return (cp >= 0x1F000)                ||  // emoji, symbols, flags
         (cp >= 0x2100 && cp <= 0x2BFF) ||  // arrows, dingbats, misc technical
         (cp >= 0xFE00 && cp <= 0xFE0F) ||  // variation selectors
         cp == 0x200D || cp == 0x20E3   ||  // ZWJ, keycap combiner
         cp == 0x203C || cp == 0x2049   ||
         cp == 0x3030 || cp == 0x303D   ||
         cp == 0x3297 || cp == 0x3299;
}

// Remove emoji and other symbols the DejaVu fonts can't draw
String stripEmoji(const String& s) {
  String out;
  out.reserve(s.length());
  size_t i = 0;
  while (i < s.length()) {
    uint8_t c = s[i];
    uint32_t cp = 0;
    size_t len = 1;
    if      (c < 0x80)           { cp = c; }
    else if ((c & 0xE0) == 0xC0) { len = 2; cp = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { len = 3; cp = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { len = 4; cp = c & 0x07; }
    else { i++; continue; }                       // invalid byte, skip
    if (i + len > s.length()) break;
    for (size_t j = 1; j < len; j++) cp = (cp << 6) | (s[i + j] & 0x3F);
    i += len;

    if (isEmojiCp(cp)) continue;

    if (cp < 0x80) out += (char)cp;
    else if (cp < 0x800) {
      out += (char)(0xC0 | (cp >> 6));
      out += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      out += (char)(0xE0 | (cp >> 12));
      out += (char)(0x80 | ((cp >> 6) & 0x3F));
      out += (char)(0x80 | (cp & 0x3F));
    }
  }
  out.trim();
  return out;
}

// Truncate a string with "..." so it fits within maxWidth in the current font
String fitText(const String& s, int maxWidth) {
  if (d.textWidth(s) <= maxWidth) return s;
  String t = s;
  while (t.length() > 1 && d.textWidth(t + "...") > maxWidth) t.remove(t.length() - 1);
  return t + "...";
}

// Word-wrap msg into lines fitting maxW (current font). Returns line count.
int wrapMessage(const String& msg, int maxW, String* lines, int maxLines) {
  int n = 0;
  String cur = "";
  int start = 0;
  while (start <= (int)msg.length() && n < maxLines) {
    int sp = msg.indexOf(' ', start);
    String word = (sp < 0) ? msg.substring(start) : msg.substring(start, sp);
    String cand = cur.length() ? cur + " " + word : word;
    if (!cur.length() || d.textWidth(cand) <= maxW) cur = cand;
    else { lines[n++] = cur; cur = word; }
    if (sp < 0) break;
    start = sp + 1;
  }
  if (cur.length() && n < maxLines) lines[n++] = cur;
  return n;
}

// Draw text at (x, y), rendering '°' (U+00B0) as a hand-drawn circle,
// because the built-in DejaVu fonts lack the degree glyph (shows a box).
void drawDegString(const String& s, int x, int y, uint32_t color) {
  int fh = d.fontHeight();
  int r = fh / 9;
  if (r < 2) r = 2;
  d.setTextDatum(top_left);
  String seg;
  for (size_t i = 0; i < s.length(); i++) {
    if ((uint8_t)s[i] == 0xC2 && i + 1 < s.length() && (uint8_t)s[i + 1] == 0xB0) {
      d.drawString(seg, x, y);
      x += d.textWidth(seg);
      seg = "";
      int cx = x + r + 2;
      int cy = y + fh / 6 + r;
      d.drawCircle(cx, cy, r, color);
      d.drawCircle(cx, cy, r - 1, color);   // second pass for thickness
      x += 2 * r + 6;
      i++;                                  // skip the 0xB0 byte
    } else {
      seg += s[i];
    }
  }
  if (seg.length()) d.drawString(seg, x, y);
}

void drawCentered(const String& s, int y, const lgfx::IFont* font) {
  d.setFont(font);
  d.setTextDatum(top_center);
  d.drawString(s, W / 2, y);
  d.setTextDatum(top_left);
}

void showMessage(const String& msg) {
  d.fillScreen(TFT_WHITE);
  d.setTextColor(TFT_BLACK, TFT_WHITE);
  drawCentered(msg, H / 2 - 20, &fonts::DejaVu40);
  d.display();
  d.waitDisplay();
}

// True if measurable rain is forecast between now and this evening
bool umbrellaNeeded() {
  JsonArray hr = wxDoc["hourly"]["precipitation"];
  if (hr.isNull()) return false;
  int from = localHour();
  int to = (from > 21) ? 23 : 21;
  for (int i = from; i <= to && i < (int)hr.size(); i++) {
    if ((float)(hr[i] | 0.0f) >= RAIN_MM_MIN) return true;
  }
  return false;
}

void drawUmbrella(int cx, int cy, int r, uint32_t col) {
  d.fillArc(cx, cy, 0, r, 180, 360, col);                       // canopy
  d.fillRect(cx - 1, cy, 3, r, col);                            // stem
  d.fillArc(cx - r / 4, cy + r, r / 4 - 1, r / 4 + 1, 0, 180, col);  // hook
}

// Shared page header: big title left (+ optional grey subtitle beside it),
// battery + updated time right, divider
void drawHeader(const String& title, const String& subtitle = "") {
  d.setTextColor(TFT_BLACK, TFT_WHITE);
  d.setFont(&fonts::DejaVu56);
  d.setCursor(30, 24);
  d.print(title);

  if (subtitle.length()) {
    int tx = 30 + d.textWidth(title) + 24;
    d.setFont(&fonts::DejaVu24);
    d.setTextColor(TFT_DARKGREY, TFT_WHITE);
    d.drawString(subtitle, tx, 52);   // baseline-aligned with the big title
    d.setTextColor(TFT_BLACK, TFT_WHITE);
  }

  d.setFont(&fonts::DejaVu18);
  d.setTextColor(TFT_DARKGREY, TFT_WHITE);
  d.setTextDatum(top_right);
  d.drawString("battery " + String(batteryPercent()) + "%", W - 30, 24);
  if (gUpdated.length()) {
    d.drawString("updated " + gUpdated, W - 30, 52);
  }
  d.setTextDatum(top_left);
  d.setTextColor(TFT_BLACK, TFT_WHITE);

  if (umbrellaNeeded()) {
    drawUmbrella(W - 215, 36, 16, TFT_BLACK);   // rain expected today
  }

  d.fillRect(30, 100, W - 60, 3, TFT_BLACK);
}
