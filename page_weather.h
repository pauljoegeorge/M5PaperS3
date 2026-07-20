#pragma once
#include "config.h"
#include "util.h"

// ---- WMO weather code -> label ----
const char* wxCondition(int code) {
  if (code == 0)  return "Clear";
  if (code <= 2)  return "Partly cloudy";
  if (code == 3)  return "Cloudy";
  if (code <= 48) return "Fog";
  if (code <= 57) return "Drizzle";
  if (code <= 67) return "Rain";
  if (code <= 77) return "Snow";
  if (code <= 82) return "Showers";
  if (code <= 86) return "Snow showers";
  return "Thunderstorm";
}

// ---- Icon primitives (monochrome, drawn with shapes) ----
void wxSun(int cx, int cy, int r) {
  d.fillCircle(cx, cy, r * 11 / 20, TFT_BLACK);
  for (int k = 0; k < 8; k++) {
    float a = k * PI / 4.0f;
    int x0 = cx + (int)(cosf(a) * r * 0.72f);
    int y0 = cy + (int)(sinf(a) * r * 0.72f);
    int x1 = cx + (int)(cosf(a) * r);
    int y1 = cy + (int)(sinf(a) * r);
    int px = (int)(-sinf(a) * 3), py = (int)(cosf(a) * 3);
    d.fillTriangle(x0 + px, y0 + py, x0 - px, y0 - py, x1, y1, TFT_BLACK);
  }
}

void wxCloud(int cx, int cy, int r, uint32_t col) {
  d.fillCircle(cx - r * 9 / 20, cy + r / 10,  r * 42 / 100, col);
  d.fillCircle(cx + r / 10,     cy - r * 22 / 100, r * 52 / 100, col);
  d.fillCircle(cx + r * 52 / 100, cy + r * 12 / 100, r * 38 / 100, col);
  d.fillRect(cx - r * 9 / 20, cy + r / 10, r * 97 / 100, r * 42 / 100, col);
}

void wxRainDrops(int cx, int cy, int r, int count) {
  for (int i = 0; i < count; i++) {
    int x = cx - r * 2 / 5 + i * (r * 4 / 5) / (count - 1);
    for (int o = 0; o < 2; o++) {
      d.drawLine(x + o, cy + r * 62 / 100, x - r * 12 / 100 + o, cy + r * 95 / 100, TFT_BLACK);
    }
  }
}

void drawWeatherIcon(int cx, int cy, int r, int code) {
  if (code == 0 || code == 1) {                       // clear / mostly clear
    wxSun(cx, cy, r);
  } else if (code == 2) {                             // partly cloudy
    wxSun(cx + r / 3, cy - r / 3, r * 7 / 10);
    wxCloud(cx - r / 8, cy + r / 6, r, TFT_WHITE);    // punch out behind cloud
    wxCloud(cx - r / 8, cy + r / 6, r * 9 / 10, TFT_BLACK);
  } else if (code == 3) {                             // overcast
    wxCloud(cx, cy, r, TFT_BLACK);
  } else if (code <= 48) {                            // fog
    wxCloud(cx, cy - r / 4, r * 4 / 5, TFT_BLACK);
    for (int i = 0; i < 3; i++) {
      d.fillRect(cx - r * 3 / 5, cy + r * 2 / 5 + i * (r / 4), r * 6 / 5, 4, TFT_DARKGREY);
    }
  } else if (code <= 67 || (code >= 80 && code <= 82)) {  // drizzle / rain / showers
    wxCloud(cx, cy - r / 6, r, TFT_BLACK);
    wxRainDrops(cx, cy, r, 4);
  } else if (code <= 77 || code == 85 || code == 86) {    // snow
    wxCloud(cx, cy - r / 6, r, TFT_BLACK);
    for (int i = 0; i < 4; i++) {
      int x = cx - r * 2 / 5 + i * (r * 4 / 5) / 3;
      d.fillCircle(x, cy + r * 3 / 4, 4, TFT_BLACK);
    }
  } else {                                            // thunderstorm
    wxCloud(cx, cy - r / 6, r, TFT_BLACK);
    d.fillTriangle(cx + r / 8,  cy + r / 3,  cx - r / 4, cy + r * 4 / 5,
                   cx + r / 12, cy + r * 3 / 5, TFT_BLACK);
    d.fillTriangle(cx + r / 12, cy + r * 3 / 5, cx + r / 3, cy + r / 2,
                   cx - r / 12, cy + r * 11 / 10, TFT_BLACK);
  }
}

// ---- Hourly chart: temperature curve on top, rain probability bars below ----
void drawHourlyChart(int x, int y, int w, int h) {
  JsonArray ht = wxDoc["hourly"]["temperature_2m"];
  JsonArray hp = wxDoc["hourly"]["precipitation_probability"];
  int n = ht.size();
  if (n > 24) n = 24;
  if (n < 2) return;

  float tmin = 1e9f, tmax = -1e9f;
  for (int i = 0; i < n; i++) {
    float v = ht[i] | 0.0f;
    tmin = fminf(tmin, v);
    tmax = fmaxf(tmax, v);
  }
  if (tmax - tmin < 2.0f) { tmax += 1.0f; tmin -= 1.0f; }

  int plotW = w - 50;                 // leave room for labels on the right
  int cy0 = y + 26, ch = h * 42 / 100;      // temp curve area
  int by0 = y + h * 62 / 100, bh = h * 26 / 100;  // rain bars area

  d.setFont(&fonts::DejaVu18);
  d.setTextColor(TFT_DARKGREY, TFT_WHITE);
  d.drawString("temperature", x, y);

  // Rain stats for caption / highlighting
  int pmax = 0, pmaxIdx = -1, rainFirst = -1, rainLast = -1;
  for (int i = 0; i < n; i++) {
    int p = hp[i] | 0;
    if (p > pmax) { pmax = p; pmaxIdx = i; }
    if (p >= 30) { if (rainFirst < 0) rainFirst = i; rainLast = i; }
  }
  String rainCap;
  if (pmax < 20)            rainCap = "rain: not expected";
  else if (rainFirst >= 0)  rainCap = "rain " + String(rainFirst) + ":00-" + String(rainLast + 1) + ":00";
  else                      rainCap = "rain: slight chance";
  d.drawString(rainCap, x, by0 - 26);

  // temp curve (3 passes for thickness)
  int px = -1, py = -1;
  for (int i = 0; i < n; i++) {
    float v = ht[i] | 0.0f;
    int xx = x + i * plotW / (n - 1);
    int yy = cy0 + ch - (int)((v - tmin) / (tmax - tmin) * ch);
    if (px >= 0) {
      for (int o = -1; o <= 1; o++) d.drawLine(px, py + o, xx, yy + o, TFT_BLACK);
    }
    px = xx; py = yy;
  }
  // min/max labels at the right edge
  drawDegString(String(tmax, 0) + "°", x + plotW + 8, cy0 - 8, TFT_DARKGREY);
  drawDegString(String(tmin, 0) + "°", x + plotW + 8, cy0 + ch - 10, TFT_DARKGREY);

  // rain probability bars: black = likely (>=50%), grey = possible;
  // dashed line marks the 50% threshold
  int bw = plotW / n;
  int y50 = by0 + bh / 2;
  for (int xd = x; xd < x + plotW; xd += 12) {
    d.drawFastHLine(xd, y50, 6, TFT_DARKGREY);
  }
  for (int i = 0; i < n; i++) {
    int p = hp[i] | 0;
    int bhh = p * bh / 100;
    if (bhh > 0) {
      d.fillRect(x + i * plotW / n, by0 + bh - bhh, bw - 2, bhh,
                 p >= 50 ? TFT_BLACK : TFT_DARKGREY);
    }
  }
  d.drawFastHLine(x, by0 + bh, plotW, TFT_BLACK);
  d.drawString("50", x + plotW + 8, y50 - 9);

  // label the peak bar with its value
  if (pmax >= 20 && pmaxIdx >= 0) {
    int ly = by0 + bh - pmax * bh / 100 - 22;
    if (ly < by0 - 22) ly = by0 - 22;
    d.setTextDatum(top_center);
    d.drawString(String(pmax) + "%", x + pmaxIdx * plotW / n + bw / 2, ly);
    d.setTextDatum(top_left);
  }

  // hour labels
  for (int hh = 0; hh <= 18; hh += 6) {
    d.drawString(hh < 10 ? "0" + String(hh) : String(hh), x + hh * plotW / 24, by0 + bh + 8);
  }
  d.setTextColor(TFT_BLACK, TFT_WHITE);
}

// Classic Japanese weather-app feature: is today a laundry-drying day?
const char* laundryVerdict() {
  JsonArray hp = wxDoc["hourly"]["precipitation_probability"];
  int rain = 0;
  for (int i = 9; i <= 17 && i < (int)hp.size(); i++) {
    int p = hp[i] | 0;
    if (p > rain) rain = p;
  }
  int hum = wxDoc["current"]["relative_humidity_2m"] | 50;
  if (rain >= 40)              return "laundry: dry inside today";
  if (rain <= 20 && hum < 65)  return "laundry: great drying day!";
  return "laundry: outside is OK";
}

void renderWeather() {
  d.fillScreen(TFT_WHITE);
  // Date next to the city: prefer the calendar's nicely formatted date,
  // fall back to Open-Meteo's ISO date
  String date = calDoc["date"] | "";
  if (!date.length()) date = wxDoc["daily"]["time"][0] | "";
  drawHeader(CITY_NAME, date);

  if (wxDoc.isNull() || wxDoc["current"].isNull()) {
    drawCentered("Weather unavailable", H / 2 - 20, &fonts::DejaVu40);
    d.display();
    return;
  }

  float tNow    = wxDoc["current"]["temperature_2m"]      | 0.0f;
  int   hum     = wxDoc["current"]["relative_humidity_2m"] | 0;
  float wind    = wxDoc["current"]["wind_speed_10m"]       | 0.0f;
  int   code    = wxDoc["current"]["weather_code"]         | 0;
  float tMax    = wxDoc["daily"]["temperature_2m_max"][0]  | 0.0f;
  float tMin    = wxDoc["daily"]["temperature_2m_min"][0]  | 0.0f;
  int   rainMax = wxDoc["daily"]["precipitation_probability_max"][0] | 0;
  String sunrise = wxDoc["daily"]["sunrise"][0] | "";
  String sunset  = wxDoc["daily"]["sunset"][0]  | "";
  if (sunrise.length() > 11) sunrise = sunrise.substring(11);
  if (sunset.length()  > 11) sunset  = sunset.substring(11);

  // ---- Left panel: now ----
  drawWeatherIcon(140, 230, 62, code);

  d.setFont(&fonts::DejaVu72);
  drawDegString(String(tNow, 0) + "°", 250, 185, TFT_BLACK);

  d.setFont(&fonts::DejaVu24);
  d.drawString(wxCondition(code), 250, 275);
  drawDegString("H " + String(tMax, 0) + "°   L " + String(tMin, 0) + "°", 30, 345, TFT_BLACK);

  d.setFont(&fonts::DejaVu18);
  d.setTextColor(TFT_DARKGREY, TFT_WHITE);
  d.drawString("rain " + String(rainMax) + "%   humidity " + String(hum) + "%   wind " + String(wind, 0) + " km/h", 30, 395);
  d.drawString("sunrise " + sunrise + "   sunset " + sunset, 30, 427);
  d.drawString(laundryVerdict(), 30, 459);
  d.setTextColor(TFT_BLACK, TFT_WHITE);

  // ---- Right panel: hourly chart ----
  drawHourlyChart(500, 130, 430, 340);

  d.display();
}
