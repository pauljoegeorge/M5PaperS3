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
// Tatoeba transcriptions look like: "[腕|うで]を[前後|ぜん|ご]に[振|ふ]りなさい。"
// Bracket groups hold the kanji and its reading(s), pipe-separated;
// multiple pipes are per-character readings (joined for display).

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
      String grp = s.substring(i + 1, close);
      int bar = grp.indexOf('|');
      if (bar > 0) {
        if (plain.length() && n < maxSegs) { segs[n++] = { plain, "" }; }
        plain = "";
        String base = grp.substring(0, bar);
        String ruby = grp.substring(bar + 1);
        ruby.replace("|", "");          // join per-character readings
        if (n < maxSegs) segs[n++] = { base, ruby };
      } else {
        plain += grp;                   // bracket without reading: keep text
      }
      i = close + 1;
    } else if (c == ' ') {
      i++;                              // word separators — drop them
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

// Layout (and optionally draw) furigana text with wrapping.
// Returns the total number of lines the text NEEDS (not capped), so callers
// can measure with draw=false, pick a size, then draw for real.
int rubyText(const String& rubyStr, int x0, int y0, int maxW, int maxLines,
             bool small, bool draw) {
  RubySeg segs[24];
  int ns = parseRuby(rubyStr, segs, 24);
  const lgfx::IFont* baseF = small ? (const lgfx::IFont*)&fonts::lgfxJapanGothic_24
                                   : (const lgfx::IFont*)&fonts::lgfxJapanGothic_28;
  const lgfx::IFont* rubyF = small ? (const lgfx::IFont*)&fonts::lgfxJapanGothic_12
                                   : (const lgfx::IFont*)&fonts::lgfxJapanGothic_16;
  const int rubyH = small ? 15 : 20;   // furigana row height
  const int lineH = small ? 45 : 58;   // furigana + base line
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

      d.setFont(baseF);
      int bw = d.textWidth(base);
      int rw = 0;
      if (ruby.length()) {
        d.setFont(rubyF);
        rw = d.textWidth(ruby);
      }
      int w = (bw > rw) ? bw : rw;

      if (x + w > x0 + maxW && x > x0) {
        line++;
        x = x0;
      }

      if (draw && line < maxLines) {
        if (ruby.length()) {
          d.setFont(rubyF);
          d.setTextColor(TFT_DARKGREY, TFT_WHITE);
          d.drawString(ruby, x + (w - rw) / 2, y0 + line * lineH);
          d.setTextColor(TFT_BLACK, TFT_WHITE);
        }
        d.setFont(baseF);
        d.drawString(base, x + (w - bw) / 2, y0 + line * lineH + rubyH);
      }
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

  // Usage example: furigana version when available, plain otherwise.
  // Measure first; if it needs more than 2 big lines, render smaller with
  // 3 lines so the sentence is always complete.
  int yU = yAfter + 20;
  bool cramped = false;
  if (gWotdUsageRuby.length()) {
    int need = rubyText(gWotdUsageRuby, 40, yU, W - 100, 9, false, false);
    if (need <= 2) {
      rubyText(gWotdUsageRuby, 40, yU, W - 100, 2, false, true);
      yU += need * 58 + 10;
    } else {
      need = rubyText(gWotdUsageRuby, 40, yU, W - 100, 9, true, false);
      rubyText(gWotdUsageRuby, 40, yU, W - 100, 3, true, true);
      yU += ((need < 3) ? need : 3) * 45 + 8;
      cramped = true;
    }
  } else {
    d.setFont(&fonts::lgfxJapanGothic_28);
    String jp[3];
    int nj = wrapJP(gWotdUsage, W - 100, jp, 3);
    for (int i = 0; i < nj; i++) d.drawString(jp[i], 40, yU + i * 42);
    yU += nj * 42 + 10;
    cramped = (nj > 2);
  }

  // Translation (smaller when the Japanese needed 3 lines)
  d.setFont(cramped ? &fonts::DejaVu18 : &fonts::DejaVu24);
  int enStep = cramped ? 26 : 32;
  d.setTextColor(TFT_DARKGREY, TFT_WHITE);
  String en[2];
  int ne = wrapMessage(gWotdUsageEn, W - 100, en, 2);
  for (int i = 0; i < ne; i++) {
    d.drawString(en[i], 40, yU + i * enStep);
  }
  d.setTextColor(TFT_BLACK, TFT_WHITE);

  d.display();
}
