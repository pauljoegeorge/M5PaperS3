#pragma once
#include <Arduino.h>

// Best-effort Japanese -> English translator for JMA earthquake hypocenter
// names (e.g. "岩手県沖" -> "Iwate Pref. (offshore)"). The P2PQuake API only
// gives the Japanese name, so this composes an English rendering from a
// prefecture/region table + a standard-suffix table rather than needing
// an exact match against JMA's full ~200-entry hypocenter name list.

struct NameEntry { const char* jp; const char* en; };

// Japan's 47 prefectures plus JMA's separate island-chain hypocenter
// regions (not folded into their administrative prefecture in JMA's
// naming). Longer/more specific entries are listed first so a shorter
// name can't shadow a longer one during prefix matching.
const NameEntry PREFECTURES[] = {
  {"伊豆諸島",   "Izu Islands"},
  {"小笠原諸島", "Ogasawara Islands"},
  {"南西諸島",   "Nansei Islands"},
  {"先島諸島",   "Sakishima Islands"},
  {"大東島近海", "Daito Islands"},
  {"硫黄島近海", "Iwo Jima"},
  {"沖縄本島近海", "Okinawa main island"},

  {"北海道",   "Hokkaido"},
  {"青森県",   "Aomori Pref."},
  {"岩手県",   "Iwate Pref."},
  {"宮城県",   "Miyagi Pref."},
  {"秋田県",   "Akita Pref."},
  {"山形県",   "Yamagata Pref."},
  {"福島県",   "Fukushima Pref."},
  {"茨城県",   "Ibaraki Pref."},
  {"栃木県",   "Tochigi Pref."},
  {"群馬県",   "Gunma Pref."},
  {"埼玉県",   "Saitama Pref."},
  {"千葉県",   "Chiba Pref."},
  {"東京都",   "Tokyo"},
  {"神奈川県", "Kanagawa Pref."},
  {"新潟県",   "Niigata Pref."},
  {"富山県",   "Toyama Pref."},
  {"石川県",   "Ishikawa Pref."},
  {"福井県",   "Fukui Pref."},
  {"山梨県",   "Yamanashi Pref."},
  {"長野県",   "Nagano Pref."},
  {"岐阜県",   "Gifu Pref."},
  {"静岡県",   "Shizuoka Pref."},
  {"愛知県",   "Aichi Pref."},
  {"三重県",   "Mie Pref."},
  {"滋賀県",   "Shiga Pref."},
  {"京都府",   "Kyoto Pref."},
  {"大阪府",   "Osaka Pref."},
  {"兵庫県",   "Hyogo Pref."},
  {"奈良県",   "Nara Pref."},
  {"和歌山県", "Wakayama Pref."},
  {"鳥取県",   "Tottori Pref."},
  {"島根県",   "Shimane Pref."},
  {"岡山県",   "Okayama Pref."},
  {"広島県",   "Hiroshima Pref."},
  {"山口県",   "Yamaguchi Pref."},
  {"徳島県",   "Tokushima Pref."},
  {"香川県",   "Kagawa Pref."},
  {"愛媛県",   "Ehime Pref."},
  {"高知県",   "Kochi Pref."},
  {"福岡県",   "Fukuoka Pref."},
  {"佐賀県",   "Saga Pref."},
  {"長崎県",   "Nagasaki Pref."},
  {"熊本県",   "Kumamoto Pref."},
  {"大分県",   "Oita Pref."},
  {"宮崎県",   "Miyazaki Pref."},
  {"鹿児島県", "Kagoshima Pref."},
  {"沖縄県",   "Okinawa Pref."},
};

// Standard suffixes JMA appends directly to a prefecture name. Longer/more
// specific suffixes are listed first so e.g. "東方沖" isn't matched as "沖".
const NameEntry SUFFIXES[] = {
  {"東方沖",   "eastern offshore waters"},
  {"西方沖",   "western offshore waters"},
  {"北方沖",   "northern offshore waters"},
  {"南方沖",   "southern offshore waters"},
  {"沖",       "offshore"},
  {"沿岸北部", "northern coastal area"},
  {"沿岸中部", "central coastal area"},
  {"沿岸南部", "southern coastal area"},
  {"沿岸部",   "coastal area"},
  {"内陸北部", "northern inland area"},
  {"内陸南部", "southern inland area"},
  {"地方",     "region"},
  {"北部",     "northern part"},
  {"中部",     "central part"},
  {"南部",     "southern part"},
  {"東部",     "eastern part"},
  {"西部",     "western part"},
  {"近海",     "nearby waters"},
};

// Named sub-regions JMA inserts between the prefecture name and a suffix
// (e.g. "熊本県熊本地方" = 熊本県 + 熊本 + 地方). Not exhaustive — JMA has
// ~200 total hypocenter names — but covers commonly-quoted ones so more
// names resolve to English instead of falling back to Japanese.
const NameEntry REGIONS[] = {
  // Hokkaido
  {"十勝",       "Tokachi"},
  {"石狩",       "Ishikari"},
  {"日高",       "Hidaka"},
  {"渡島",       "Oshima"},
  {"檜山",       "Hiyama"},
  {"後志",       "Shiribeshi"},
  {"胆振",       "Iburi"},
  {"上川",       "Kamikawa"},
  {"留萌",       "Rumoi"},
  {"宗谷",       "Soya"},
  {"根室",       "Nemuro"},
  {"釧路",       "Kushiro"},
  // Tohoku
  {"津軽",       "Tsugaru"},
  // Kyushu
  {"熊本",       "Kumamoto"},
  {"天草・芦北", "Amakusa-Ashikita"},
  {"阿蘇",       "Aso"},
  {"球磨",       "Kuma"},
  {"薩摩",       "Satsuma"},
  {"大隅",       "Osumi"},
};

// Best-effort English rendering of a JMA hypocenter name; "" if the name
// isn't a recognized prefecture (+ sub-region) + suffix combination, so
// callers can fall back to showing the original Japanese string.
String translatePlaceName(const String& jp) {
  for (const NameEntry& p : PREFECTURES) {
    String prefix(p.jp);
    if (!jp.startsWith(prefix)) continue;

    String rest = jp.substring(prefix.length());
    if (!rest.length()) return String(p.en);

    // "<prefecture><suffix>" e.g. "岩手県沖" -> "Iwate Pref. (offshore)"
    for (const NameEntry& s : SUFFIXES) {
      if (rest == s.jp) return String(p.en) + " (" + s.en + ")";
    }

    // "<prefecture><region><suffix>" e.g. "熊本県熊本地方" ->
    // "Kumamoto Pref. (Kumamoto region)"
    for (const NameEntry& s : SUFFIXES) {
      String suf(s.jp);
      if (rest.length() <= suf.length() || !rest.endsWith(suf)) continue;
      String region = rest.substring(0, rest.length() - suf.length());
      for (const NameEntry& r : REGIONS) {
        if (region == r.jp) return String(p.en) + " (" + r.en + " " + s.en + ")";
      }
    }
    return "";   // unrecognized -> let caller fall back to Japanese
  }
  return "";
}
