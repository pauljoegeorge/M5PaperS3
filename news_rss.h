#pragma once
#include "news_core.h"

// Streaming tag parser: only captures title, pubDate and article identity.
// Tags and values have hard caps; descriptions and the full XML are never stored.
class RssNewsSink : public Stream {
 public:
  NewsArticle* articles;
  int count = 0;
  int items = 0;
  bool complete = false;
  explicit RssNewsSink(NewsArticle* target) : articles(target) {
    tag.reserve(80); value.reserve(512);
  }
  size_t write(uint8_t c) override { feed((char)c); return 1; }
  size_t write(const uint8_t* data, size_t len) override {
    for (size_t i = 0; i < len; ++i) feed((char)data[i]);
    return len;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
 private:
  String tag, value, title, date, identity;
  bool inTag = false, inItem = false, cdata = false;
  int field = 0;
  void finishItem() {
    ++items;
    int64_t published = newsPubDate(date.c_str());
    String cleaned = cleanTitle(title);
    if (!cleaned.length() || !newsEligible(published, time(nullptr))) return;
    NewsArticle item = {};
    newsCopyTitle(item.title, cleaned.c_str());
    item.published = published;
    item.titleHash = newsHash(cleaned.c_str(), true);
    item.idHash = identity.length() ? newsHash(identity.c_str()) : 0;
    newsInsert(articles, count, item);
  }
  void handleTag() {
    if (tag == "item") {
      inItem = true; title = ""; date = ""; identity = ""; field = 0;
    } else if (tag == "/item") {
      if (inItem) finishItem();
      inItem = false; field = 0;
    } else if (tag == "/rss") {
      complete = true;
    } else if (inItem) {
      if (tag == "title") { field = 1; value = ""; }
      else if (tag == "pubDate") { field = 2; value = ""; }
      else if (tag == "guid" || tag.startsWith("guid ")) { field = 3; value = ""; }
      else if (tag == "link" && !identity.length()) { field = 4; value = ""; }
      else if (tag == "/title" && field == 1) { title = value; field = 0; }
      else if (tag == "/pubDate" && field == 2) { date = value; field = 0; }
      else if ((tag == "/guid" && field == 3) || (tag == "/link" && field == 4)) {
        identity = value; field = 0;
      }
    }
  }
  void feed(char c) {
    if (cdata) {
      // Keep two trailing brackets until we know whether they close CDATA.
      if (c == ']') { ++brackets; return; }
      if (c == '>' && brackets >= 2) {
        for (int i = 2; i < brackets; ++i) append(']');
        brackets = 0; cdata = false; return;
      }
      while (brackets > 0) { append(']'); --brackets; }
      append(c); return;
    }
    if (inTag) {
      if (c == '>') { handleTag(); inTag = false; }
      else if (tag.length() < 80) {
        tag += c;
        if (tag == "![CDATA[") { inTag = false; cdata = true; brackets = 0; }
      }
    } else if (c == '<') { inTag = true; tag = ""; }
    else append(c);
  }
  int brackets = 0;
  void append(char c) { if (field && value.length() < 512) value += c; }
};

