#pragma once
#include "util.h"

void renderNews() {
  d.fillScreen(TFT_WHITE);
  drawHeader("News", gNewsLabel);   // e.g. "News · Tech"

  if (gNewsCount == 0) {
    drawCentered("No headlines yet", H / 2 - 40, &fonts::DejaVu40);
    d.setTextColor(TFT_DARKGREY, TFT_WHITE);
    drawCentered(gNewsDebug.length() ? gNewsDebug : "fetch not attempted yet",
                 H / 2 + 30, &fonts::DejaVu24);
    d.setTextColor(TFT_BLACK, TFT_WHITE);
    d.display();
    return;
  }

  int y = 136;
  const int rowH = 190;
  const int textX = 130;
  const int textW = W - textX - 40;

  for (int i = 0; i < gNewsCount && i < 2; i++) {
    // Number badge
    d.setFont(&fonts::DejaVu40);
    d.fillRoundRect(30, y + 4, 60, 60, 14, TFT_BLACK);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setTextDatum(top_center);
    d.drawString(String(i + 1), 30 + 30, y + 13);
    d.setTextDatum(top_left);
    d.setTextColor(TFT_BLACK, TFT_WHITE);

    // Headline, wrapped to max 3 lines, ellipsis if still too long
    d.setFont(&fonts::DejaVu40);
    String lines[3];
    int n = wrapMessage(gNews[i], textW, lines, 3);
    int usedLen = 0;
    for (int j = 0; j < n; j++) usedLen += lines[j].length() + (j ? 1 : 0);
    if (usedLen < (int)gNews[i].length()) {
      lines[n - 1] = fitText(lines[n - 1] + " ...", textW);
    }
    for (int j = 0; j < n; j++) {
      d.drawString(lines[j], textX, y + 2 + j * 50);
    }

    if (i > 0) d.fillRect(30, y - 20, W - 60, 1, TFT_LIGHTGREY);
    y += rowH;
  }

  d.display();
}
