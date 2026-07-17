# M5PaperS3 Dashboard

An e-ink desk dashboard for the [M5Paper S3](https://shop.m5stack.com/products/m5papers3-esp32-s3-development-kit) (4.7" touch e-paper, ESP32-S3) showing today's Google Calendar, weather, news headlines, a Japanese word of the day, and earthquake alerts — rotating as a slideshow with touch navigation.

## Pages

| Page | Content | Data source |
|---|---|---|
| Calendar | Today's events (past events drop off as the day progresses) | Google Apps Script (your calendar) |
| Weather | Current conditions, H/L, hourly temp curve + rain bars, sunrise/sunset | [Open-Meteo](https://open-meteo.com/) (no key) |
| News | Top 2 headlines; feed rotates by time of day (tech / Japan / world / India) | Google News RSS |
| Kotoba | JLPT N3/N2 word of the day with reading, meaning, and a furigana example sentence | [Jisho](https://jisho.org/) + [Tatoeba](https://tatoeba.org/) APIs, curated fallback list |
| Message | Optional big decorated message | Calendar event titled `MSG: your text` |
| Earthquake | Takes over the screen after a recent M3+ quake in Japan | [P2PQuake](https://www.p2pquake.net/) (JMA reports) |

## Behavior

- **Slideshow**: flips to the next page every `SLIDE_MIN` minutes.
- **Touch**: tap right half = next page, tap left half = previous page,
  **long-press (~1.5 s) = re-fetch everything now**. Hold taps ~¼ s (the
  device polls between light-sleep naps).
- **Data refresh**: every `REFRESH_MIN` minutes over Wi-Fi (calendar +
  weather each time; news hourly; word of the day once per day).
- **Quiet hours**: no automatic page flips 23:00–06:00 (configurable);
  touch and data refreshes still work.
- **Battery**: light sleep between touch polls; roughly 1–2 weeks per
  charge with default settings. Battery % is shown top-right.

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
  ]
}
```

Put the deployment URL (ends in `/exec`) in `secrets.h`.

### 4. Configure

Everything tunable lives in [config.h](config.h): location + city for
weather, news feeds and their time slots, refresh/slideshow intervals,
quiet hours, and the earthquake alert threshold.

## Special message page

Create a calendar event **today** titled `MSG: You can do it!!` — within
one refresh a decorated message page joins the rotation. Delete the event
to remove it. No re-flashing needed.

## File layout

```
m5papers3_calendar.ino   setup + main loop (touch, slideshow, sleep)
config.h                 user settings
util.h                   shared globals, drawing/text helpers
net.h                    WiFi + all data fetching/parsing
page_*.h                 one file per page
word_data.h              fallback N3/N2 word list
secrets.h                credentials (gitignored — create your own)
```

To add a page: write `page_foo.h` with a `renderFoo()`, include it in the
.ino, add an enum entry, and register it in `buildPageList()`.

## Notes & limitations

- Fonts cover Latin + Japanese. Emoji in event titles are stripped (no
  emoji glyphs on e-ink).
- Earthquake alerts poll at the refresh interval — this is recent quake
  info, **not** an early warning system.
- Re-flashing: if an upload fails to start, press the reset button as the
  upload begins (light sleep can make the USB port drowsy).
