# M5PaperS3 Dashboard

An e-ink desk dashboard for the [M5Paper S3](https://shop.m5stack.com/products/m5papers3-esp32-s3-development-kit) (4.7" touch e-paper, ESP32-S3) showing today's Google Calendar, weather, news headlines, a Japanese word of the day, and earthquake alerts — rotating as a slideshow with touch navigation.

## Pages

| Page | Content | Data source |
|---|---|---|
| Calendar | Today's events (past events drop off as the day progresses) | Google Apps Script (your calendar) |
| Weather | Current conditions, H/L, hourly temp curve + rain bars, sunrise/sunset | [Open-Meteo](https://open-meteo.com/) (no key) |
| News | Top 2 headlines; feed rotates by time of day (tech / Japan / world / India) | Google News RSS |
| Kotoba | JLPT N3/N2 word of the day with reading, meaning, and a furigana example sentence | [Jisho](https://jisho.org/) + [Tatoeba](https://tatoeba.org/) APIs, curated fallback list |
| Countdown | Days remaining to upcoming dates | Calendar events titled `CNT: name` on their target date |
| Message | Optional big decorated message | Calendar event titled `MSG: your text` |
| Earthquake | Takes over the screen after a recent M3+ quake in Japan | [P2PQuake](https://www.p2pquake.net/) (JMA reports) |

## Behavior

- **Slideshow**: flips to the next page every `SLIDE_MIN` minutes.
- **Touch**: tap right half = next page, tap left half = previous page,
  **long-press (~1.5 s) = re-fetch everything now**. Hold taps ~¼ s (the
  device polls between light-sleep naps).
- **Data refresh**: every `REFRESH_MIN` minutes over Wi-Fi (calendar +
  weather each time; news hourly, never repeating recent headlines; word
  of the day once per day).
- **Quiet hours** (default 00:00–06:00): the device deep-sleeps for the
  night — the screen holds its last page, touch is inactive, and at the
  end hour it reboots and refreshes everything fresh.
- **Battery**: light sleep between touch polls, deep sleep at night;
  roughly 4–7 days per charge with default settings. Battery % is shown
  top-right (voltage-based, so approximate — reads ~100% on USB).

## Setup

### 1. Arduino IDE

- Add the M5Stack boards URL in *Settings → Additional boards manager URLs*:
  `https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json`
- *Boards Manager* → install **M5Stack**, then select board **M5PaperS3**.
- Install libraries: **M5GFX**, **ArduinoJson** (v7).

### 2. secrets.h (not in the repo)

Create `secrets.h` next to the .ino:

```cpp
#pragma once
#define WIFI_SSID_SECRET  "your-wifi-ssid"        // 2.4 GHz only
#define WIFI_PASS_SECRET  "your-wifi-password"
#define SCRIPT_URL_SECRET "https://script.google.com/macros/s/XXXX/exec"
```

### 3. Google Apps Script (calendar feed)

Deploy a Script as a web app (access: *Anyone*) that returns JSON shaped
like this for today's events:

```json
{
  "date": "Thu, Jul 17",
  "fetched": "09:46",
  "events": [
    { "title": "Standup", "start": "10:00", "end": "10:15",
      "location": "Meet", "cal": "Work", "allDay": false }
  ],
  "countdowns": [
    { "name": "India trip", "date": "2026-08-10" }
  ]
}
```

Put the deployment URL (ends in `/exec`) in `secrets.h`. The
`countdowns` array comes from
[apps_script_countdowns.gs](apps_script_countdowns.gs) — add its
function to your script and call it in the payload.

### 4. Configure

Everything tunable lives in [config.h](config.h): location + city for
weather, news feeds and their time slots, refresh/slideshow intervals,
quiet hours, and the earthquake alert threshold.

## Special calendar events

Create events **today** in Google Calendar with these title prefixes —
they are hidden from the event list and control extra pages. No
re-flashing needed; changes appear within one refresh.

- `MSG: You can do it!!` (an event today) — decorated message page.
- `CNT: India trip` — countdown page. Create it as an all-day event **on
  the target date itself** (lookahead window set by `COUNTDOWN_DAYS` in
  the Apps Script, default 30 days); requires the
  [apps_script_countdowns.gs](apps_script_countdowns.gs) addition. Up to
  4 countdowns, sorted soonest-first, gone once the date passes. (Legacy
  form also works without the script change: an event *today* titled
  `CNT: name / YYYY-MM-DD`.)

Extras derived from weather data: an umbrella icon appears in every
page's header when rain probability reaches 50% before evening, and the
weather page shows a laundry-drying verdict.

## File layout

```
m5papers3_calendar.ino     setup + main loop (touch, slideshow, sleep)
config.h                   user settings
util.h                     shared globals, drawing/text helpers
net.h                      WiFi + all data fetching/parsing
page_*.h                   one file per page
word_data.h                fallback N3/N2 word list
apps_script_countdowns.gs  Apps Script addition for the countdown page
secrets.h                  credentials (gitignored — create your own)
```

To add a page: write `page_foo.h` with a `renderFoo()`, include it in the
.ino, add an enum entry, and register it in `buildPageList()`.

## Notes & limitations

- Fonts cover Latin + Japanese. Emoji are stripped from all displayed
  text (no emoji glyphs on e-ink).
- Earthquake alerts poll at the refresh interval — this is recent quake
  info, **not** an early warning system.
- Re-flashing: if an upload fails to start, press the reset button as the
  upload begins (light sleep can make the USB port drowsy).
