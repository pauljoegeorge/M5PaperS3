"""Host checks of the actual sanitizer and renderer, using a small display shim."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
util = (root / 'util.h').read_text()
sanitizer = util[util.index('bool isEmojiCp('):util.index('#include "emoji_text.h"')]
harness = r'''
#include <string>
#include <cstdint>
#include <cassert>
#include <iostream>
#include <algorithm>
class String {
  std::string s;
public:
  String(const char* p=""): s(p) {}
  String(std::string p): s(p) {}
  unsigned length() const { return s.size(); }
  char operator[](int i) const { return s[i]; }
  void reserve(size_t n) { s.reserve(n); }
  String& operator+=(char c) { s+=c; return *this; }
  String operator+(const char* p) const { return s+p; }
  bool operator==(const char* p) const { return s==p; }
  void replace(const char* a,const char* b) {
    size_t i=0; while((i=s.find(a,i))!=std::string::npos) {
      s.replace(i,std::string(a).size(),b); i+=std::string(b).size();
    }
  }
  void trim() {
    auto a=s.find_first_not_of(" \t\r\n");
    if(a==std::string::npos) {s.clear();return;}
    s=s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);
  }
  void remove(int i) { s.erase(i); }
  String substring(int a, int b) const { return s.substr(a,b-a); }
  String substring(int a) const { return s.substr(a); }
};
constexpr uint32_t TFT_WHITE=0xffffff, TFT_BLACK=0;
struct Display {
  int icons=0, height=40;
  int fontHeight() {return height;}
  int textWidth(const String& s) {
    int n=0; for(int i=0;i<(int)s.length();++i)
      if(((uint8_t)s[i]&0xc0)!=0x80) ++n;
    return n*20;
  }
  void drawString(const String&,int,int) {}
  void fillRect(int,int,int w,int h,uint32_t) {assert(w>0&&h>0);++icons;}
  void fillTriangle(int,int,int,int,int,int,uint32_t) {++icons;}
  void drawLine(int,int,int,int,uint32_t) {++icons;}
  void fillCircle(int,int,int r,uint32_t) {assert(r>0);++icons;}
} d;
'''
tests = r'''
int main() {
  const char* source[] = {"🎂", "✈️", "❤️", "🎉", "🏠", "😄"};
  for(int id=1;id<=6;++id) {
    String s=emojiText(source[id-1]);
    assert(s.length()==3 && emojiMarker(s,0)==id);
    assert(emojiTextWidth(s)==emojiSize()+4);
    int before=d.icons; drawEmojiText(s,0,0); assert(d.icons>before);
    emojiPop(s); assert(s.length()==0);
  }
  assert(emojiText("Unsupported 😀")=="Unsupported");
  assert(emojiText("✈")=="\xEE\x80\x82");
  assert(emojiText("❤")=="\xEE\x80\x83");
  String mixed=emojiText("Happy 🎂 Birthday ❤️");
  assert(emojiTextWidth(mixed)>d.textWidth("Happy  Birthday "));
  String truncated=fitEmojiText(mixed,100);
  assert(emojiTextWidth(truncated)<=100);
  assert(fitText("日本語日本語", 100)=="日本...");
  String japanese="日本語"; emojiPop(japanese); assert(japanese=="日本");
  for(int h: {18,40,56,72}) {
    d.height=h;
    for(int id=1;id<=6;++id) drawEmojiIcon(id,0,0,emojiSize(),TFT_BLACK);
  }
  std::cout << "Emoji sanitizer, width, drawing, and UTF-8 truncation checks passed\n";
}
'''
with tempfile.TemporaryDirectory() as tmp:
    source = Path(tmp) / 'emoji_test.cpp'
    fit_text = util[util.index('String fitText('):util.index('// Word-wrap msg')]
    source.write_text(harness + sanitizer + '\n#include "' + str(root / 'emoji_text.h') + '"\n' + fit_text + tests)
    exe = Path(tmp) / 'emoji-test'
    subprocess.run(['c++', '-std=c++17', str(source), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
