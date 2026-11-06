// Host test: c++ -std=c++17 tests/news_test.cpp -o /tmp/news-test && /tmp/news-test
#include <cassert>
#include <iostream>
#include <string>
#include <ctime>
#include "../news_core.h"
class String {
  std::string s;
 public:
  String() = default;
  String(const char* p): s(p) {}
  String(std::string p): s(p) {}
  char operator[](size_t i) const { return s[i]; }
  void replace(const char* a, const char* b) {
    size_t i=0; while ((i=s.find(a,i))!=std::string::npos) {
      s.replace(i,strlen(a),b); i+=strlen(b);
    }
  }
  String substring(size_t a) const { return s.substr(a); }
  String substring(size_t a,size_t b) const { return s.substr(a,b-a); }
  int lastIndexOf(const char* p) const {
    auto i=s.rfind(p); return i==std::string::npos ? -1 : (int)i;
  }
  void trim() {
    auto a=s.find_first_not_of(" \r\n\t");
    if(a==std::string::npos) {s.clear();return;}
    s=s.substr(a,s.find_last_not_of(" \r\n\t")-a+1);
  }
  void reserve(size_t n) { s.reserve(n); }
  size_t length() const { return s.size(); }
  const char* c_str() const { return s.c_str(); }
  String& operator+=(char c) { s += c; return *this; }
  bool operator==(const char* p) const { return s == p; }
  bool startsWith(const char* p) const { return s.rfind(p, 0) == 0; }
};
class Stream {
 public:
  virtual ~Stream() = default;
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t*, size_t) = 0;
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;
};
String stripEmoji(String s) { return s; }
#include "../news_text.h"
#include "../news_rss.h"
int main() {
  assert(cleanTitle("It’s ‘quoted’ — “news”…") == "It's 'quoted' - \"news\"...");
  assert(cleanTitle("It&#8217;s &#x2018;quoted&#X2019; &mdash; &ldquo;news&rdquo;&hellip;") ==
         "It's 'quoted' - \"news\"...");
  assert(cleanTitle("Rock &amp; roll &apos;yes&apos; &#39;ok&#39;") == "Rock & roll 'yes' 'ok'");
  assert(cleanTitle("A&nbsp;B&#160;C&#x202F;D") == "A B C D");
  assert(cleanTitle("日本語 &#x65E5;") == "日本語 日");
  assert(decodeEntities("&#xD800; &#0; &#x110000; &#bad;") == "&#xD800; &#0; &#x110000; &#bad;");

  auto now = newsPubDate("Thu, 22 Oct 2026 12:00:00 GMT");
  assert(now == 1792670400);
  assert(newsPubDate("Thu, 31 Feb 2026 12:00:00 GMT") == 0);
  assert(newsPubDate("Thu, 22 Oct 2026 12:00:00 JST") == 0);
  assert(newsEligible(newsPubDate("Tue, 20 Oct 2026 00:00:00 GMT"), now));
  assert(!newsEligible(newsPubDate("Mon, 19 Oct 2026 23:59:59 GMT"), now));
  assert(!newsEligible(now + 1, now));
  assert(newsHash("A NEW, story!", true) == newsHash("a new story", true));
  NewsArticle articles[NEWS_CAPACITY] = {};
  int count = 0;
  for (int i = 1; i <= 80; ++i) {
    NewsArticle a = {}; a.published = i; a.titleHash = i;
    newsInsert(articles, count, a);
  }
  assert(count == 64 && articles[0].published == 80 && articles[63].published == 17);
  newsInsert(articles, count, articles[0]); assert(count == 64);
  int pos = 0;
  for (int i = 0; i < 5; ++i) { assert(pos == i*2); pos = newsNextPosition(pos, 10); }
  assert(pos == 0 && newsNextPosition(8, 9) == 0 && newsNextPosition(0, 1) == 0);
  std::string utf8(319, 'a'); utf8 += "日本";
  char capped[321]; newsCopyTitle(capped, utf8.c_str()); assert(strlen(capped) == 319);
  // Actual streaming parser, byte by byte, using dates relative to the host GMT clock.
  char date[64]; time_t current = time(nullptr) - 60;
  strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&current));
  std::string xml = "<rss><channel><title>Ignore channel</title>";
  for (int i = 0; i < 80; ++i) {
    xml += "<item><title><![CDATA[Headline " + std::to_string(i) + " <test>]]></title>";
    xml += "<description>" + std::string(20000, 'x') + "</description>";
    xml += "<pubDate>" + std::string(date) + "</pubDate><guid isPermaLink=\"false\">id" + std::to_string(i) + "</guid></item>";
  }
  xml += "<item><title>Old</title><pubDate>Mon, 19 Oct 2020 00:00:00 GMT</pubDate></item>";
  xml += "<item><title>Missing date</title></item></channel></rss>";
  RssNewsSink sink(articles);
  for (unsigned char c : xml) sink.write(c);
  assert(sink.complete && sink.items == 82 && sink.count == 64);
  assert(std::string(articles[0].title) == "Headline 0 <test>");
  assert(articles[0].idHash == newsHash("id0"));
  RssNewsSink duplicates(articles);
  std::string repeated = "<rss>";
  const char* names[] = {"Same headline!", "same HEADLINE", "Different wording", "Another story"};
  const char* ids[] = {"one", "two", "one", "three"};
  for (int i = 0; i < 4; ++i)
    repeated += "<item><title>" + std::string(names[i]) + "</title><pubDate>" + date +
      "</pubDate><guid>" + ids[i] + "</guid></item>";
  repeated += "</rss>";
  for (size_t i = 0; i < repeated.size(); i += 7)
    duplicates.write((const uint8_t*)repeated.data() + i, std::min(size_t(7), repeated.size() - i));
  assert(duplicates.count == 2 && duplicates.complete);
  RssNewsSink punctuation(articles);
  std::string quotes = "<rss><item><title><![CDATA[It’s “news”… &amp; &#x2018;quotes&#8217;]]></title><pubDate>" +
      std::string(date) + "</pubDate></item></rss>";
  punctuation.write((const uint8_t*)quotes.data(), quotes.size());
  assert(punctuation.complete && punctuation.count == 1);
  assert(std::string(articles[0].title) == "It's \"news\"... & 'quotes'");
  RssNewsSink truncated(articles);
  const char* broken = "<rss><item><title>Broken";
  truncated.write((const uint8_t*)broken, strlen(broken));
  assert(!truncated.complete && truncated.count == 0);
  std::cout << "News date, cache, UTF-8, rotation and streaming tests passed; cache bytes=" << sizeof(articles) << '\n';
}
