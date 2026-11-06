#pragma once

// Six small monochrome drawings, sized to the active font. Private-use
// markers are internal only; calendar payloads retain their original UTF-8.
String emojiText(const String& input) {
  String s = input;
  s.replace("🎂", "\xEE\x80\x81");
  s.replace("✈", "\xEE\x80\x82");
  s.replace("❤", "\xEE\x80\x83");
  s.replace("🎉", "\xEE\x80\x84");
  s.replace("🏠", "\xEE\x80\x85");
  s.replace("😄", "\xEE\x80\x86");
  return stripEmoji(s); // removes variation selectors and unsupported emoji
}

int emojiMarker(const String& s, int i) {
  if (i + 2 >= (int)s.length()) return 0;
  if ((uint8_t)s[i] != 0xEE || (uint8_t)s[i + 1] != 0x80) return 0;
  int id = (uint8_t)s[i + 2] - 0x80;
  return id >= 1 && id <= 6 ? id : 0;
}

int emojiSize() { return d.fontHeight() * 9 / 10; }

int emojiTextWidth(const String& s) {
  int width = 0, start = 0;
  for (int i = 0; i < (int)s.length();) {
    if (emojiMarker(s, i)) {
      width += d.textWidth(s.substring(start, i)) + emojiSize() + 4;
      i += 3;
      start = i;
    } else ++i;
  }
  return width + d.textWidth(s.substring(start));
}

// Remove one complete UTF-8 character, including a complete icon marker.
void emojiPop(String& s) {
  if (!s.length()) return;
  int i = s.length() - 1;
  while (i > 0 && ((uint8_t)s[i] & 0xC0) == 0x80) --i;
  s.remove(i);
}

String fitEmojiText(const String& s, int maxWidth) {
  if (emojiTextWidth(s) <= maxWidth) return s;
  String t = s;
  while (t.length() && emojiTextWidth(t + "...") > maxWidth) emojiPop(t);
  return t + "...";
}

void drawEmojiIcon(int id, int x, int y, int size, uint32_t color) {
  // Design coordinates run from 0 to 24; no bitmap/font allocation.
  auto X = [&](int v) { return x + v * size / 24; };
  auto Y = [&](int v) { return y + v * size / 24; };
  auto rect = [&](int a, int b, int w, int h, uint32_t c) {
    d.fillRect(X(a), Y(b), w * size / 24 + 1, h * size / 24 + 1, c);
  };
  auto tri = [&](int a, int b, int c, int e, int f, int g) {
    d.fillTriangle(X(a), Y(b), X(c), Y(e), X(f), Y(g), color);
  };
  auto line = [&](int a, int b, int c, int e) {
    d.drawLine(X(a), Y(b), X(c), Y(e), color);
  };
  auto dot = [&](int a, int b, int r) {
    d.fillCircle(X(a), Y(b), r * size / 24 + 1, color);
  };
  switch (id) {
    case 1: // Birthday cake, three candles and a white icing band
      rect(3, 12, 18, 10, color);
      rect(5, 15, 14, 2, TFT_WHITE);
      for (int a = 6; a <= 18; a += 6) {
        rect(a, 7, 1, 4, color);
        tri(a - 1, 5, a + 2, 5, a + 1, 1);
      }
      rect(1, 22, 22, 1, color);
      break;
    case 2: // Airplane seen from above
      tri(12, 1, 10, 19, 14, 19);
      tri(12, 7, 1, 15, 23, 15);
      tri(12, 17, 6, 22, 18, 22);
      break;
    case 3: // Heart
      dot(7, 8, 5); dot(17, 8, 5);
      tri(2, 10, 22, 10, 12, 23);
      break;
    case 4: // Party popper and confetti
      tri(2, 22, 6, 9, 15, 18);
      line(8, 8, 12, 3); line(14, 12, 21, 9);
      line(16, 16, 22, 18);
      dot(18, 3, 1); dot(22, 5, 1); dot(5, 3, 1);
      break;
    case 5: // House, chimney, door and window
      rect(16, 3, 3, 6, color);
      tri(1, 12, 12, 2, 23, 12);
      rect(4, 11, 16, 12, color);
      rect(9, 16, 5, 7, TFT_WHITE);
      rect(16, 14, 2, 3, TFT_WHITE);
      break;
    case 6: // Grinning face with smiling eyes
      dot(12, 12, 10);
      d.fillCircle(X(12), Y(12), 8 * size / 24, TFT_WHITE);
      line(5, 9, 8, 6); line(8, 6, 10, 9);
      line(14, 9, 17, 6); line(17, 6, 20, 9);
      // Open grin with a white band of teeth.
      d.fillCircle(X(12), Y(15), 5 * size / 24, color);
      rect(7, 10, 10, 3, TFT_WHITE);
      rect(9, 14, 6, 1, TFT_WHITE);
      break;

  }
}

// Caller uses top-left text datum. Draw regular text in runs so kerning and
// existing fonts are unchanged, reserving a full icon advance per marker.
void drawEmojiText(const String& s, int x, int y, uint32_t color = TFT_BLACK) {
  int start = 0;
  for (int i = 0; i < (int)s.length();) {
    int id = emojiMarker(s, i);
    if (!id) { ++i; continue; }
    String run = s.substring(start, i);
    d.drawString(run, x, y);
    x += d.textWidth(run);
    drawEmojiIcon(id, x + 2, y + (d.fontHeight() - emojiSize()) / 2, emojiSize(), color);
    x += emojiSize() + 4;
    i += 3;
    start = i;
  }
  d.drawString(s.substring(start), x, y);
}
