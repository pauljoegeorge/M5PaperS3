#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static const int NEWS_CAPACITY = 64;
static const int NEWS_TITLE_BYTES = 320;
struct NewsArticle {
  char title[NEWS_TITLE_BYTES + 1];
  int64_t published;
  uint64_t titleHash;
  uint64_t idHash;
};

// RSS RFC 822 dates are GMT. No local timezone conversion is needed.
inline int64_t newsPubDate(const char* s) {
  char weekday[4], month[4], zone[8];
  int day, year, hour, minute, second;
  if (sscanf(s, "%3s, %d %3s %d %d:%d:%d %7s", weekday, &day,
             month, &year, &hour, &minute, &second, zone) != 8) return 0;
  if (strcmp(zone, "GMT") && strcmp(zone, "UTC") && strcmp(zone, "+0000")) return 0;
  int m = 0;
  const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  for (int i = 0; i < 12; ++i) if (!strcmp(month, months[i])) m = i + 1;
  if (!m || year < 2020 || year > 2100 || hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) return 0;
  const int lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  if (day < 1 || day > lengths[m-1] + (m == 2 && leap)) return 0;
  int y = year - (m <= 2);
  int era = y / 400;
  unsigned yo = y - era * 400;
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  unsigned doe = yo * 365 + yo / 4 - yo / 100 + doy;
  return ((int64_t)era * 146097 + doe - 719468) * 86400 + hour * 3600 + minute * 60 + second;
}
inline bool newsEligible(int64_t published, int64_t now) {
  return now > 1600000000 && published > 0 && published <= now &&
         published / 86400 >= now / 86400 - 2;
}
inline uint64_t newsHash(const char* s, bool normalize = false) {
  uint64_t hash = 14695981039346656037ULL;
  for (; *s; ++s) {
    unsigned char c = (unsigned char)*s;
    if (normalize && c < 128 && !isalnum(c)) continue;
    if (normalize && c < 128) c = (unsigned char)tolower(c);
    hash = (hash ^ c) * 1099511628211ULL;
  }
  return hash;
}
inline void newsCopyTitle(char* dst, const char* src) {
  size_t n = strlen(src);
  if (n > NEWS_TITLE_BYTES) {
    n = NEWS_TITLE_BYTES;
    while (n && ((unsigned char)src[n] & 0xc0) == 0x80) --n;
  }
  memcpy(dst, src, n); dst[n] = 0;
}
// Sorted bounded insertion, with deduplication against the entire retained set.
inline void newsInsert(NewsArticle* articles, int& count, const NewsArticle& item) {
  for (int i = 0; i < count; ++i)
    if (articles[i].titleHash == item.titleHash ||
        (item.idHash && articles[i].idHash == item.idHash)) return;
  int pos = 0;
  while (pos < count && articles[pos].published >= item.published) ++pos;
  if (pos >= NEWS_CAPACITY) return;
  if (count < NEWS_CAPACITY) ++count;
  for (int i = count - 1; i > pos; --i) articles[i] = articles[i-1];
  articles[pos] = item;
}
inline int newsNextPosition(int pos, int count) {
  return pos + 2 < count ? pos + 2 : 0;
}
