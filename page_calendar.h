#pragma once
#include "util.h"
#include "calendar_core.h"

// Minutes since local midnight for "HH:MM" strings, -1 if unparsable
int parseHM(const String& s) {
  int colon = s.indexOf(':');
  if (colon < 1) return -1;
  return s.substring(0, colon).toInt() * 60 + s.substring(colon + 1).toInt();
}

// True once a timed event's end time has passed (all-day events never expire)
bool eventEnded(JsonObject e, int nowMin) {
  bool allDay = e["allDay"] | false;
  int64_t endEpoch = e["endEpoch"] | (int64_t)0;
  int endMin = parseHM(e["end"] | "");
  int startMin = parseHM(e["start"] | "");
  return calendarEventEnded(allDay, (int64_t)time(nullptr), endEpoch, nowMin, startMin, endMin);
}

void renderCalendar() {
  d.fillScreen(TFT_WHITE);

  if (calDoc.isNull()) {
    drawHeader("Calendar", "", false);
    drawCentered("No data - check WiFi", H / 2 - 20, &fonts::DejaVu40);
    d.display();
    return;
  }

  time_t now = time(nullptr);
  time_t local = now + (time_t)TZ_OFFSET_HOURS * 3600;
  char todayISO[11], tomorrowISO[11];
  strftime(todayISO, sizeof(todayISO), "%Y-%m-%d", gmtime(&local));
  time_t next = local + 86400;
  strftime(tomorrowISO, sizeof(tomorrowISO), "%Y-%m-%d", gmtime(&next));
  int nowMin = (int)((local / 60) % 1440);

  // Reuse yesterday's prefetched tomorrow as today after a failed refresh.
  JsonObject day = calDoc.as<JsonObject>();
  String iso = day["isoDate"] | "";
  if (iso.length() && iso != todayISO) {
    day = calDoc["tomorrow"].as<JsonObject>();
    if (String(day["isoDate"] | "") != todayISO) {
      drawHeader("Calendar", "", false);
      drawCentered("Calendar data out of date", H / 2 - 20, &fonts::DejaVu24);
      d.display();
      return;
    }
  }
  JsonArray events = day["events"].as<JsonArray>();
  bool todayHasEventsRemaining = false;
  for (JsonObject e : events) {
    String title = e["title"] | "";
    if (title.startsWith("MSG:") || title.startsWith("CNT:")) continue;
    if (!eventEnded(e, nowMin)) todayHasEventsRemaining = true;
  }
  JsonObject nextDay = calDoc["tomorrow"].as<JsonObject>();
  bool tomorrowHasEvents = false;
  if (String(nextDay["isoDate"] | "") == tomorrowISO &&
      nextDay["events"].is<JsonArray>()) {
    for (JsonObject e : nextDay["events"].as<JsonArray>()) {
      String title = e["title"] | "";
      if (!title.startsWith("MSG:") && !title.startsWith("CNT:")) {
        tomorrowHasEvents = true;
        break;
      }
    }
  }
  // Keep today's view when tomorrow is empty or unavailable.
  bool tomorrow = calendarShowTomorrow(nowMin, CALENDAR_TOMORROW_HOUR,
                                       todayHasEventsRemaining, tomorrowHasEvents);
  if (tomorrow) {
    day = nextDay;
    events = day["events"].as<JsonArray>();
  }
  String date = day["date"] | "Today";
  drawHeader(tomorrow ? "Tomorrow" : date, tomorrow ? date : "", false);

  int upcoming = 0;
  for (JsonObject e : events) {
    String title = e["title"] | "";
    if (title.startsWith("MSG:") || title.startsWith("CNT:")) continue;
    if (tomorrow || !eventEnded(e, nowMin)) upcoming++;
  }

  if (upcoming == 0 && (tomorrow || events.size() == 0)) {
    drawCentered(tomorrow ? "No events tomorrow" : "No events today", H / 2 - 40, &fonts::DejaVu40);
    d.setTextColor(TFT_DARKGREY, TFT_WHITE);
    drawCentered("Enjoy your free day!", H / 2 + 20, &fonts::DejaVu24);
    d.setTextColor(TFT_BLACK, TFT_WHITE);
    d.display();
    return;
  }

  if (upcoming == 0) {
    drawCentered("All done for today", H / 2 - 40, &fonts::DejaVu40);
    d.setTextColor(TFT_DARKGREY, TFT_WHITE);
    drawCentered(String(events.size()) + " event" + (events.size() > 1 ? "s" : "") + " finished",
                 H / 2 + 20, &fonts::DejaVu24);
    d.setTextColor(TFT_BLACK, TFT_WHITE);
    d.display();
    return;
  }

  // ---- Event list ----
  int y = 124;
  const int rowH = 80;
  const int maxRows = (H - y - 16) / rowH;   // 5 rows
  int shown = 0;

  const int timeX = 56;    // time column
  const int textX = 230;   // title/location column

  for (JsonObject e : events) {
    if (shown >= maxRows) break;
    String rawTitle = e["title"] | "";
    if (rawTitle.startsWith("MSG:") || rawTitle.startsWith("CNT:")) continue;
    if (!tomorrow && eventEnded(e, nowMin)) continue;

    String title    = emojiText(e["title"]    | "");
    String start    = e["start"]    | "";
    String end      = e["end"]      | "";
    String location = emojiText(e["location"] | "");
    String calTag   = stripEmoji(e["cal"]      | "");
    bool allDay     = e["allDay"]   | false;
    title.replace("\n", " ");
    location.replace("\n", ", ");   // addresses often have line breaks

    // Left accent bar
    d.fillRect(30, y + 8, 8, rowH - 20, TFT_BLACK);

    // Time column
    d.setFont(&fonts::DejaVu24);
    if (allDay) {
      d.setCursor(timeX, y + 22);
      d.print("All day");
    } else {
      d.setCursor(timeX, y + 6);
      d.print(start);
      d.setFont(&fonts::DejaVu18);
      d.setTextColor(TFT_DARKGREY, TFT_WHITE);
      d.setCursor(timeX, y + 40);
      d.print(end);
      d.setTextColor(TFT_BLACK, TFT_WHITE);
    }

    // Calendar tag (small inverted pill, right-aligned)
    int tagLeft = W - 40;
    if (calTag.length()) {
      d.setFont(&fonts::DejaVu18);
      int tw = d.textWidth(calTag);
      tagLeft = W - 40 - tw - 24;
      d.fillRoundRect(tagLeft, y + 8, tw + 24, 34, 8, TFT_BLACK);
      d.setTextColor(TFT_WHITE, TFT_BLACK);
      d.setCursor(tagLeft + 12, y + 15);
      d.print(calTag);
      d.setTextColor(TFT_BLACK, TFT_WHITE);
    }

    // Title (drawString never wraps — always a single line)
    d.setFont(&fonts::DejaVu40);
    drawEmojiText(fitEmojiText(title, tagLeft - textX - 20), textX, y + 2);

    // Location
    if (location.length()) {
      d.setFont(&fonts::DejaVu18);
      d.setTextColor(TFT_DARKGREY, TFT_WHITE);
      drawEmojiText(fitEmojiText(location, W - textX - 60), textX, y + 48, TFT_DARKGREY);
      d.setTextColor(TFT_BLACK, TFT_WHITE);
    }

    // Row separator
    if (shown > 0) {
      d.fillRect(30, y - 6, W - 60, 1, TFT_LIGHTGREY);
    }

    y += rowH;
    shown++;
  }

  // "+N more" footer if truncated
  int remaining = upcoming - shown;
  if (remaining > 0) {
    d.setFont(&fonts::DejaVu18);
    d.setTextColor(TFT_DARKGREY, TFT_WHITE);
    d.setCursor(30, H - 28);
    d.printf("+ %d more event%s", remaining, remaining > 1 ? "s" : "");
    d.setTextColor(TFT_BLACK, TFT_WHITE);
  }

  d.display();
}
