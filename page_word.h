#pragma once
#include "util.h"

// Character-wise wrap for Japanese (no spaces to break on)
int wrapJP(const String& s, int maxW, String* lines, int maxLines) {
  int n = 0;
  String cur;
  for (size_t i = 0; i < s.length();) {
    uint8_t c = s[i];
    int len = (c < 0x80) ? 1 : ((c & 0xE0) == 0xC0) ? 2 : ((c & 0xF0) == 0xE0) ? 3 : 4;
    String ch = s.substring(i, i + len);
    if (cur.length() && d.textWidth(cur + ch) > maxW) {
      if (n >= maxLines - 1) { lines[n++] = cur; return n; }
      lines[n++] = cur;
      cur = "";
    }
    cur += ch;
    i += len;
  }
  if (cur.length() && n < maxLines) lines[n++] = cur;
  return n;
}

// ---- Furigana (ruby) rendering ----
// Tatoeba transcriptions look like: "彼[かれ]は 学生[がくせい]です。"
// A [reading] applies to the run of kanji immediately before it.

uint32_t utf8At(const String& s, int i, int* len) {
  uint8_t c = s[i];
  if (c < 0x80)            { *len = 1; return c; }
  if ((c & 0xE0) == 0xC0)  { *len = 2; return ((uint32_t)(c & 0x1F) << 6)  | (s[i+1] & 0x3F); }
  if ((c & 0xF0) == 0xE0)  { *len = 3; return ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(s[i+1] & 0x3F) << 6) | (s[i+2] & 0x3F); }
  *len = 4;
  return ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(s[i+1] & 0x3F) << 12) |
         ((uint32_t)(s[i+2] & 0x3F) << 6) | (s[i+3] & 0x3F);
}

bool isKanjiCp(uint32_t cp) {
  return (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0x3400 && cp <= 0x4DBF) || cp == 0x3005;
}

struct RubySeg { String base; String ruby; };

// Split a furigana-marked string into (base, ruby) segments
int parseRuby(const String& s, RubySeg* segs, int maxSegs) {
  int n = 0;
  String plain;
  for (size_t i = 0; i < s.length();) {
    char c = s[i];
    if (c == '[') {
      int close = s.indexOf(']', i);
      if (close < 0) break;
      String ruby = s.substring(i + 1, close);
      // peel the trailing kanji run off `plain` — that's what the ruby covers
      int cut = plain.length();
      while (cut > 0) {
        int p = cut - 1;
        while (p > 0 && ((uint8_t)plain[p] & 0xC0) == 0x80) p--;
        int cl;
        if (!isKanjiCp(utf8At(plain, p, &cl))) break;
        cut = p;
      }
      if (cut > 0 && n < maxSegs) segs[n++] = { plain.substring(0, cut), "" };
      if (cut < (int)plain.length() && n < maxSegs) segs[n++] = { plain.substring(cut), ruby };
      plain = "";
      i = close + 1;
    } else if (c == ' ') {
      i++;                      // Tatoeba word separators — drop them
    } else {
      int len;
      utf8At(s, i, &len);
      plain += s.substring(i, i + len);
      i += len;
    }
  }
  if (plain.length() && n < maxSegs) segs[n++] = { plain, "" };
  return n;
}

// Draw furigana text with wrapping. Returns number of lines used.
int drawRubyText(const String& rubyStr, int x0, int y0, int maxW, int maxLines) {
  RubySeg segs[24];
  int ns = parseRuby(rubyStr, segs, 24);
  const int rubyH = 20;    // furigana row height
  const int lineH = 58;    // furigana + base line
  int x = x0, line = 0;

  for (int si = 0; si < ns; si++) {
    // ruby segments are atomic; plain segments break per character
    int ci = 0;
    while (true) {
      String base, ruby;
      if (segs[si].ruby.length()) {
        base = segs[si].base;
        ruby = segs[si].ruby;
      } else {
        if (ci >= (int)segs[si].base.length()) break;
        int len;
        utf8At(segs[si].base, ci, &len);
        base = segs[si].base.substring(ci, ci + len);
        ci += len;
      }

      d.setFont(&fonts::lgfxJapanGothic_28);
      int bw = d.textWidth(base);
      int rw = 0;
      if (ruby.length()) {
        d.setFont(&fonts::lgfxJapanGothic_16);
        rw = d.textWidth(ruby);
      }
      int w = (bw > rw) ? bw : rw;

      if (x + w > x0 + maxW && x > x0) {
        line++;
        x = x0;
        if (line >= maxLines) return maxLines;
      }

      if (ruby.length()) {
        d.setFont(&fonts::lgfxJapanGothic_16);
        d.setTextColor(TFT_DARKGREY, TFT_WHITE);
        d.drawString(ruby, x + (w - rw) / 2, y0 + line * lineH);
        d.setTextColor(TFT_BLACK, TFT_WHITE);
      }
      d.setFont(&fonts::lgfxJapanGothic_28);
      d.drawString(base, x + (w - bw) / 2, y0 + line * lineH + rubyH);
      x += w;

      if (segs[si].ruby.length()) break;   // atomic segment done
    }
  }
  return line + 1;
}

void renderWord() {
  d.fillScreen(TFT_WHITE);
  drawHeader("Kotoba", gWotdLevel);

  // Word (2x scale = ~80px) + reading
  d.setFont(&fonts::lgfxJapanGothic_40);
  d.setTextSize(2);
  d.setTextColor(TFT_BLACK, TFT_WHITE);
  d.drawString(gWotdWord, 40, 122);
  int wordW = d.textWidth(gWotdWord);
  d.setTextSize(1);

  if (gWotdReading.length() && gWotdReading != gWotdWord) {
    d.setFont(&fonts::lgfxJapanGothic_28);
    d.setTextColor(TFT_DARKGREY, TFT_WHITE);
    d.drawString(gWotdReading, 40 + wordW + 32, 165);
    d.setTextColor(TFT_BLACK, TFT_WHITE);
  }

  // Meaning: big font if it fits on one line, otherwise smaller on two
  int yAfter;
  d.setFont(&fonts::DejaVu40);
  if (d.textWidth(gWotdMeaning) <= W - 80) {
    d.drawString(gWotdMeaning, 40, 228);
    yAfter = 285;
  } else {
    d.setFont(&fonts::DejaVu24);
    String ml[2];
    int nm = wrapMessage(gWotdMeaning, W - 80, ml, 2);
    if (ml[nm - 1].length() + ml[0].length() < gWotdMeaning.length()) {
      ml[nm - 1] = fitText(ml[nm - 1] + " ...", W - 80);
    }
    for (int i = 0; i < nm; i++) d.drawString(ml[i], 40, 228 + i * 34);
    yAfter = 228 + nm * 34 + 12;
  }

  d.fillRect(40, yAfter, W - 80, 1, TFT_LIGHTGREY);

  // Usage example: furigana version when available, plain otherwise
  int yU = yAfter + 20;
  int usedLines;
  if (gWotdUsageRuby.length()) {
    usedLines = drawRubyText(gWotdUsageRuby, 40, yU, W - 100, 2);
    yU += usedLines * 58 + 10;
  } else {
    d.setFont(&fonts::lgfxJapanGothic_28);
    String jp[2];
    int nj = wrapJP(gWotdUsage, W - 100, jp, 2);
    for (int i = 0; i < nj; i++) d.drawString(jp[i], 40, yU + i * 42);
    yU += nj * 42 + 10;
  }

  // Translation
  d.setFont(&fonts::DejaVu24);
  d.setTextColor(TFT_DARKGREY, TFT_WHITE);
  String en[2];
  int ne = wrapMessage(gWotdUsageEn, W - 100, en, 2);
  for (int i = 0; i < ne; i++) {
    d.drawString(en[i], 40, yU + i * 32);
  }
  d.setTextColor(TFT_BLACK, TFT_WHITE);

  d.display();
}
