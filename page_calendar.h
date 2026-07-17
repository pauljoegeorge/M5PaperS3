#pragma once
#include "util.h"

// Minutes since local midnight for "HH:MM" strings, -1 if unparsable
int parseHM(const String& s) {
  int colon = s.indexOf(':');
  if (colon < 1) return -1;
  return s.substring(0, colon).toInt() * 60 + s.substring(colon + 1).toInt();
}

// True once a timed event's end time has passed (all-day events never expire)
bool eventEnded(JsonObject e, int nowMin) {
  if (e["allDay"] | false) return false;
  int endMin = parseHM(e["end"] | "");
  return endMin >= 0 && endMin <= nowMin;
}

void renderCalendar() {
  d.fillScreen(TFT_WHITE);

  if (calDoc.isNull()) {
    drawHeader("Calendar");
    drawCentered("No data - check WiFi", H / 2 - 20, &fonts::DejaVu40);
    d.display();
    return;
  }

  String date = calDoc["date"] | "Today";
  drawHeader(date);

  JsonArray events = calDoc["events"];

  // Hide events that have already ended, so later ones rise into view
  int nowMin = (int)((time(nullptr) / 60 + (time_t)TZ_OFFSET_HOURS * 60) % 1440);
  int upcoming = 0;
  for (JsonObject e : events) {
    if (!eventEnded(e, nowMin)) upcoming++;
  }

  if (events.size() == 0) {
    drawCentered("No events today", H / 2 - 40, &fonts::DejaVu40);
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
    if (eventEnded(e, nowMin)) continue;   // already over -> hide

    String title    = stripEmoji(e["title"]    | "");
    String start    = e["start"]    | "";
    String end      = e["end"]      | "";
    String location = stripEmoji(e["location"] | "");
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
    d.drawString(fitText(title, tagLeft - textX - 20), textX, y + 2);

    // Location
    if (location.length()) {
      d.setFont(&fonts::DejaVu18);
      d.setTextColor(TFT_DARKGREY, TFT_WHITE);
      d.drawString(fitText(location, W - textX - 60), textX, y + 48);
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
