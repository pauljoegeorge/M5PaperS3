#pragma once
#include "util.h"

void renderCountdown() {
  d.fillScreen(TFT_WHITE);
  drawHeader("Countdown");

  int rowH = (H - 150) / gCntCount;
  if (rowH > 150) rowH = 150;
  bool big = rowH >= 120;                     // 1-2 items get the huge layout
  int y = 140;

  for (int i = 0; i < gCntCount; i++) {
    // Days number, right-aligned in the left column
    d.setFont(big ? &fonts::DejaVu72 : &fonts::DejaVu56);
    d.setTextDatum(top_right);
    d.drawString(String(gCntDays[i]), 210, y + 4);
    d.setTextDatum(top_left);

    d.setFont(&fonts::DejaVu18);
    d.setTextColor(TFT_DARKGREY, TFT_WHITE);
    d.setTextDatum(top_right);
    d.drawString(gCntDays[i] == 0 ? "TODAY!" : (gCntDays[i] == 1 ? "day" : "days"),
                 210, y + (big ? 82 : 64));
    d.setTextDatum(top_left);
    d.setTextColor(TFT_BLACK, TFT_WHITE);

    // Name + target date
    d.setFont(&fonts::DejaVu40);
    d.drawString(fitText(stripEmoji(gCntName[i]), W - 260 - 40), 260, y + 8);
    d.setFont(&fonts::DejaVu18);
    d.setTextColor(TFT_DARKGREY, TFT_WHITE);
    d.drawString(gCntDate[i], 260, y + 58);
    d.setTextColor(TFT_BLACK, TFT_WHITE);

    // Accent bar between number and text
    d.fillRect(232, y + 8, 6, (big ? 96 : 78), TFT_BLACK);

    y += rowH;
  }

  d.display();
}
