#pragma once

// Decode decimal/hex numeric entities as UTF-8 before font normalization.
String decodeEntities(String s) {
  s.replace("&amp;", "&");
  s.replace("&lt;", "<");
  s.replace("&gt;", ">");
  s.replace("&quot;", "\"");
  s.replace("&apos;", "'");
  s.replace("&lsquo;", "'"); s.replace("&rsquo;", "'");
  s.replace("&sbquo;", "'");
  s.replace("&ldquo;", "\""); s.replace("&rdquo;", "\"");
  s.replace("&bdquo;", "\"");
  s.replace("&ndash;", "-"); s.replace("&mdash;", "-");
  s.replace("&hellip;", "..."); s.replace("&nbsp;", " ");
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length();) {
    if (s[i] == '&' && i + 2 < s.length() && s[i + 1] == '#') {
      size_t j = i + 2;
      int base = 10;
      if (s[j] == 'x' || s[j] == 'X') { base = 16; ++j; }
      size_t digits = j;
      uint32_t cp = 0;
      while (j < s.length() && j - digits < 8) {
        char c = s[j];
        int v = c >= '0' && c <= '9' ? c - '0'
              : c >= 'a' && c <= 'f' ? c - 'a' + 10
              : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (v < 0 || v >= base) break;
        cp = cp * base + v;
        ++j;
      }
      if (j > digits && j < s.length() && s[j] == ';' && cp > 0 &&
          cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff)) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) {
          out += (char)(0xc0 | (cp >> 6)); out += (char)(0x80 | (cp & 63));
        } else if (cp < 0x10000) {
          out += (char)(0xe0 | (cp >> 12));
          out += (char)(0x80 | ((cp >> 6) & 63)); out += (char)(0x80 | (cp & 63));
        } else {
          out += (char)(0xf0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 63));
          out += (char)(0x80 | ((cp >> 6) & 63)); out += (char)(0x80 | (cp & 63));
        }
        i = j + 1;
        continue;
      }
    }
    out += s[i++];
  }
  return out;
}

// These typographic glyphs are absent from the dashboard's compact fonts.
String normalizeNewsPunctuation(String s) {
  s.replace("‘", "'"); s.replace("’", "'"); s.replace("‚", "'");
  s.replace("‛", "'"); s.replace("ʼ", "'");
  s.replace("“", "\""); s.replace("”", "\""); s.replace("„", "\"");
  s.replace("‟", "\"");
  s.replace("–", "-"); s.replace("—", "-"); s.replace("‑", "-");
  s.replace("−", "-"); s.replace("…", "...");
  s.replace("\xC2\xA0", " "); s.replace("\xE2\x80\xAF", " ");
  return s;
}

String cleanTitle(String t) {
  if (t.startsWith("<![CDATA[")) {
    t = t.substring(9);
    t.replace("]]>", "");
  }
  t.replace("\n", " ");
  t.replace("\r", " ");
  t.replace("\t", " ");
  int dash = t.lastIndexOf(" - ");
  if (dash > 10) t = t.substring(0, dash);    // drop " - Source Name" suffix
  t = stripEmoji(normalizeNewsPunctuation(decodeEntities(t)));
  t.trim();
  return t;
}

