# M5PaperS3 Dashboard

An e-ink desk dashboard for the [M5Paper S3](https://shop.m5stack.com/products/m5papers3-esp32-s3-development-kit) (4.7" touch e-paper, ESP32-S3) showing today's Google Calendar, weather, news headlines, a Japanese word of the day, and earthquake alerts — rotating as a slideshow with touch navigation.

## Pages

| Page | Content | Data source |
|---|---|---|
| Calendar | Today's events (past events drop off as the day progresses) | Google Apps Script (your calendar) |
| Weather | Current conditions, H/L, hourly temp curve + rain bars, sunrise/sunset | [Open-Meteo](https://open-meteo.com/) (no key) |
| News | 2 headlines per batch; 7 scheduled feeds, cached rotation every 15 minutes | Google News RSS |
| Kotoba | JLPT N3/N2 word of the day with reading, meaning, and a furigana example sentence | [Jisho](https://jisho.org/) + [Tatoeba](https://tatoeba.org/) APIs, curated fallback list |
| Countdown | Days remaining to upcoming dates | Calendar events titled `CNT: name` on their target date |
| Message | Optional big decorated message | Calendar event titled `MSG: your text` |
| Earthquake | Takes over the screen after a recent M3+ quake in Japan (shows the biggest by magnitude if several occurred in the window); epicenter name shown in English with the Japanese name below when recognized, else Japanese alone | [P2PQuake](https://www.p2pquake.net/) (JMA reports) |
| Photo | Whatever image is newest in a Drive folder you control — drop in a new photo any time to change it | Google Drive, via the same Apps Script |

## News schedule and cache

All schedule times are JST: Tech until 10:00, Science 10:00–14:00,
Japan 14:00–15:00, Chennai 15:00–17:00, Kerala 17:00–19:00,
India 19:00–20:00, International from 20:00 until quiet hours.
Quiet hours are configured as 23:00–06:00. Japan coverage is in English.

The active RSS feed is streamed hourly and on category changes, independently
of the 30-minute calendar/weather refresh. Long-press forces a news fetch too.
Only articles published today or on the previous two GMT calendar dates qualify;
missing/invalid dates and future timestamps are excluded. Articles are sorted
newest first and deduplicated using normalized title and article identity hashes.
Different headlines about the same event can still appear.

Only the active category is cached: the newest 64 eligible articles, each title
capped at 320 UTF-8 bytes. Two headlines rotate locally every 15 minutes and
loop at the end (an odd final article appears alone). The five-minute dashboard
slideshow and existing headline layout are unchanged; news rotation does not
force navigation to News. Refreshes preserve the current article when possible.
Caches reset on reboot. Failed requests retain eligible news from the same
category and retry after 15 minutes; category changes clear the previous feed.

Each fixed cache uses 22,528 bytes of PSRAM; refresh temporarily keeps two
(45,056 bytes total), plus bounded parser strings and normal HTTP/TLS allocations.
No full XML response or seven-feed cache is retained. Serial logs show internal
heap, largest internal free block and PSRAM before/during/after news downloads.
Runtime heap margin and battery impact still require on-device measurement.

Host checks: `c++ -std=c++17 tests/news_test.cpp -o /tmp/news-test && /tmp/news-test`.
These cover GMT dates, bounded selection, title hashes, UTF-8 truncation,
two-item looping, and streaming parsing including large ignored descriptions.

## Behavior

- **Slideshow**: flips to the next page every `SLIDE_MIN` minutes.
- **Touch**: tap right half = next page, tap left half = previous page,
  **long-press (~1.5 s) = re-fetch everything now**. The device sleeps
  between touches and wakes on the touch controller's interrupt line
  rather than polling it on a timer, so a quick, light flick can be
  missed — hold each tap for a beat rather than a fast tap.
- **Data refresh**: every `REFRESH_MIN` minutes over Wi-Fi (calendar +
  weather each time; active news feed hourly and at category changes, cached
  headlines rotate every 15 minutes and loop; word
  of the day once per day).
- **Quiet hours** (default 23:00–06:00): the device deep-sleeps for the
  night — the screen holds its last page, touch is inactive, and at the
  end hour it reboots and refreshes everything fresh.
- **Battery**: light sleep between touches (woken by a GPIO interrupt on
  the touch controller instead of constant polling), deep sleep at
  night. Actual runtime depends heavily on your Wi-Fi/refresh settings
  and hasn't been independently re-measured since this change — treat any
  "days per charge" figure as unverified until you've tried it yourself.
  Battery % is shown top-right (voltage-based, so approximate — reads
  ~100% on USB).

## Setup

### 1. Arduino IDE

- Add the M5Stack boards URL in *Settings → Additional boards manager URLs*:
  `https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json`
- *Boards Manager* → install **M5Stack**, then select board **M5PaperS3**.
- Install libraries: **M5GFX**, **ArduinoJson** (v7).
- *Tools → PSRAM* → enable it (exact label depends on your board package
  version, e.g. "OPI PSRAM"). Required for the Photo page's decode buffer;
  everything else works fine either way.

### 2. secrets.h (not in the repo)

Create `secrets.h` next to the .ino:

```cpp
#pragma once
#define WIFI_SSID_SECRET  "your-wifi-ssid"        // 2.4 GHz only
#define WIFI_PASS_SECRET  "your-wifi-password"
#define SCRIPT_URL_SECRET "https://script.google.com/macros/s/XXXX/exec"
#define WEATHER_LAT_SECRET 35.6812f               // your location (Open-Meteo)
#define WEATHER_LON_SECRET 139.7671f
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

### 4. Photo page (optional)

Add a `?photo=1` branch to the same `doGet(e)` — it serves whichever
image is newest in one Drive folder, so the device always fetches the
same fixed URL and you never have to update anything to change the
picture, just drop a new file into the folder:

```javascript
const PHOTO_FOLDER_ID = 'YOUR_FOLDER_ID_HERE';

function findNewestPhoto() {
  const folder = DriveApp.getFolderById(PHOTO_FOLDER_ID);
  const files = folder.getFiles();
  let newest = null;
  while (files.hasNext()) {
    const f = files.next();
    if (f.getMimeType().indexOf('image/') !== 0) continue;
    if (!newest || f.getLastUpdated() > newest.getLastUpdated()) newest = f;
  }
  return newest;
}

function servePhotoId() {
  const newest = findNewestPhoto();
  const payload = newest ? { id: newest.getId() } : { id: '' };
  return ContentService.createTextOutput(JSON.stringify(payload)).setMimeType(ContentService.MimeType.JSON);
}

function doGet(e) {
  if (e.parameter.photo) {
    return servePhotoId();
  }
  // ...existing calendar logic...
}
```

Set the folder to **"Anyone with the link — Viewer"** (Share menu) — the
device fetches the actual image bytes straight from Google's CDN
(`lh3.googleusercontent.com`), not through the script, so the file needs
to be link-accessible. Any image format works (JPEG, PNG, HEIC, etc.).

To change the picture: just upload a new file into that folder from your
phone — no redeploy, no ID to copy anywhere. Old files can be left in the
folder or deleted; only the newest one is ever used.

### 5. Configure

Everything tunable lives in [config.h](config.h): location + city for
weather, news feeds and their time slots, refresh/slideshow intervals,
quiet hours, the earthquake alert threshold, and `PHOTO_FILL` (how
aggressively the Photo page crops to fill the screen vs. letterboxing).

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

Every page shows today's date, and an umbrella icon appears in every
page's header when rain probability reaches 50% before evening; the
weather page also shows a laundry-drying verdict.

## File layout

```
m5papers3_calendar.ino     setup + main loop (touch, slideshow, sleep)
config.h                   user settings
util.h                     shared globals, drawing/text helpers
net.h                      WiFi + all data fetching/parsing
page_*.h                   one file per page
word_data.h                fallback N3/N2 word list
place_names.h              JMA hypocenter name -> English lookup (earthquake page)
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
- Epicenter English names are composed from a static table of Japan's 47
  prefectures + common JMA region/suffix names (see `place_names.h`);
  uncommon or compound names fall back to the original Japanese.
- Photo page: decode + dither only happens when the Drive folder's newest
  file actually changes (checked every `REFRESH_MIN`), not on every render,
  so swapping the picture takes up to one refresh cycle (or a long-press)
  to appear. Requires PSRAM enabled in board settings.
- Re-flashing: if an upload fails to start, press the reset button as the
  upload begins (light sleep can make the USB port drowsy).
