// Host test: c++ -std=c++17 tests/place_names_test.cpp -o /tmp/place-names-test && /tmp/place-names-test
#include <cassert>
#include <iostream>
#include "../place_names.h"

int main() {
  assert(strcmp(quakePlaceEnglish("沖縄本島近海"), "Adjacent Sea of Okinawa Main Island") == 0);
  assert(strcmp(quakePlaceEnglish("三陸沖"), "Off the Coast of Sanriku") == 0);
  assert(strcmp(quakePlaceEnglish("石川県能登地方"), "Noto, Ishikawa Prefecture") == 0);
  assert(strcmp(quakePlaceEnglish("東京湾"), "Tokyo Bay") == 0);
  assert(strcmp(quakePlaceEnglish("日向灘"), "Hyuganada Sea") == 0);
  assert(strcmp(quakePlaceEnglish("伊豆大島近海"), "Adjacent Sea of Izu-Oshima Island") == 0);
  // Unknown and partial names must never acquire a guessed translation.
  for (const char* unknown : {"", "未知の震源", "岩手県沖の未知地域", "沖縄本島近海北部"}) {
    assert(!*quakePlaceEnglish(unknown));
  }
  assert(!*quakePlaceEnglish(nullptr));
  for (const auto& name : QUAKE_PLACE_NAMES) {
    assert(*name.jp && *name.en);
    assert(strcmp(quakePlaceEnglish(name.jp), name.en) == 0);
    assert(strstr(name.en, "\xe2\x80\x8b") == nullptr);
  }
  std::cout << "Earthquake place-name checks passed ("
            << sizeof(QUAKE_PLACE_NAMES) / sizeof(QUAKE_PLACE_NAMES[0]) << " names)\n";
}
