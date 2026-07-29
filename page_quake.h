#pragma once
#include "util.h"
#include "place_names.h"

// JMA maxScale (x10) -> shindo intensity label
String shindoText(int scale) {
  switch (scale) {
    case 10: return "1";
    case 20: return "2";
    case 30: return "3";
    case 40: return "4";
    case 45: return "5-";
    case 50: return "5+";
    case 55: return "6-";
    case 60: return "6+";
    case 70: return "7";
  }
  return "-";
}

void renderQuake() {
  d.fillScreen(TFT_WHITE);

  // Alert banner
  d.fillRect(0, 0, W, 110, TFT_BLACK);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  drawCentered("EARTHQUAKE", 28, &fonts::DejaVu56);
  d.setFont(&fonts::DejaVu18);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.setTextDatum(top_right);
  d.drawString(formatDate(), W - 30, 20);
  d.setTextDatum(top_left);
  d.setTextColor(TFT_BLACK, TFT_WHITE);

  // Magnitude (left) and max intensity (right)
  d.setFont(&fonts::DejaVu18);
  d.setTextColor(TFT_DARKGREY, TFT_WHITE);
  d.drawString("magnitude", 80, 160);
  d.drawString("max intensity (shindo)", W - 420, 160);
  d.setTextColor(TFT_BLACK, TFT_WHITE);

  d.setFont(&fonts::DejaVu72);
  d.drawString("M " + String(gQuakeMag, 1), 80, 195);
  d.drawString(shindoText(gQuakeScale), W - 420, 195);

  d.fillRect(60, 310, W - 120, 2, TFT_BLACK);

  // Epicenter: English (when recognized) with Japanese below; else Japanese alone
  String placeEn = translatePlaceName(gQuakePlace);
  bool haveEn = placeEn.length() > 0;

  d.setFont(haveEn ? (const lgfx::IFont*)&fonts::DejaVu40 : (const lgfx::IFont*)&fonts::lgfxJapanGothic_40);
  d.drawString(haveEn ? placeEn : gQuakePlace, 80, 330);

  if (haveEn) {
    d.setFont(&fonts::lgfxJapanGothic_24);
    d.setTextColor(TFT_DARKGREY, TFT_WHITE);
    d.drawString(gQuakePlace, 80, 378);
    d.setTextColor(TFT_BLACK, TFT_WHITE);
  }

  d.setFont(&fonts::DejaVu24);
  d.setTextColor(TFT_DARKGREY, TFT_WHITE);
  d.drawString(gQuakeTime + " JST    source: P2PQuake / JMA", 80, 430);
  d.setTextColor(TFT_BLACK, TFT_WHITE);

  d.display();
}
