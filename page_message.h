#pragma once
#include "util.h"

// 4-point sparkle star
void sparkle(int cx, int cy, int r, uint32_t col) {
  d.fillTriangle(cx, cy - r, cx - r / 3, cy, cx + r / 3, cy, col);
  d.fillTriangle(cx, cy + r, cx - r / 3, cy, cx + r / 3, cy, col);
  d.fillTriangle(cx - r, cy, cx, cy - r / 3, cx, cy + r / 3, col);
  d.fillTriangle(cx + r, cy, cx, cy - r / 3, cx, cy + r / 3, col);
}

void renderMessage() {
  d.fillScreen(TFT_WHITE);

  // Double decorative frame
  for (int i = 0; i < 4; i++) {
    d.drawRoundRect(24 + i, 24 + i, W - 48 - 2 * i, H - 48 - 2 * i, 20, TFT_BLACK);
  }
  d.drawRoundRect(46, 46, W - 92, H - 92, 14, TFT_DARKGREY);

  // Corner sparkles (big + small companion)
  sparkle(100, 105, 24, TFT_BLACK);
  sparkle(138, 140, 11, TFT_DARKGREY);
  sparkle(W - 100, 105, 24, TFT_BLACK);
  sparkle(W - 138, 140, 11, TFT_DARKGREY);
  sparkle(100, H - 105, 24, TFT_BLACK);
  sparkle(138, H - 140, 11, TFT_DARKGREY);
  sparkle(W - 100, H - 105, 24, TFT_BLACK);
  sparkle(W - 138, H - 140, 11, TFT_DARKGREY);

  // Message text: pick the biggest font that fits in 3 lines
  String msg = stripEmoji(gMessage);
  const int maxW = W - 240;
  String lines[4];
  d.setFont(&fonts::DejaVu72);
  int n = wrapMessage(msg, maxW, lines, 4);
  if (n > 2) { d.setFont(&fonts::DejaVu56); n = wrapMessage(msg, maxW, lines, 4); }
  if (n > 3) { d.setFont(&fonts::DejaVu40); n = wrapMessage(msg, maxW, lines, 4); }

  int lh = d.fontHeight() + 16;
  int y = H / 2 - (n * lh) / 2 + 6;
  d.setTextColor(TFT_BLACK, TFT_WHITE);
  d.setTextDatum(top_center);
  for (int i = 0; i < n; i++) {
    d.drawString(lines[i], W / 2, y + i * lh);
  }
  d.setTextDatum(top_left);

  d.display();
}
