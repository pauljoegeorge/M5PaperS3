/**
 * Add to your existing Apps Script (the one serving the calendar JSON).
 *
 * Scans the next 90 days for events titled "CNT: name" and returns them
 * as [{name, date}] so the device can show countdowns. Create such events
 * on their ACTUAL date (all-day, no repeat, no date in the title needed).
 */
function getCountdowns() {
  var now = new Date();
  var until = new Date(now.getTime() + 90 * 24 * 60 * 60 * 1000);
  var out = [];
  CalendarApp.getDefaultCalendar().getEvents(now, until).forEach(function (ev) {
    var title = ev.getTitle();
    if (title.indexOf('CNT:') !== 0 || out.length >= 4) return;
    out.push({
      name: title.substring(4).trim(),
      date: Utilities.formatDate(ev.getStartTime(),
                                 Session.getScriptTimeZone(), 'yyyy-MM-dd')
    });
  });
  return out;
}

/*
 * Then, in your doGet() where you build the JSON payload, add one line:
 *
 *   payload.countdowns = getCountdowns();
 *
 * so the response looks like:
 *   { "date": "...", "fetched": "...", "events": [...],
 *     "countdowns": [ { "name": "India trip", "date": "2026-08-10" } ] }
 *
 * After editing: Deploy -> Manage deployments -> Edit -> New version.
 * (The /exec URL stays the same.)
 */
