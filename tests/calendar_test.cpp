#include "../calendar_core.h"
#include <cassert>
#include <iostream>
int main() {
  assert(!calendarShowTomorrow(17 * 60 + 59, 18, false, true));
  assert(calendarShowTomorrow(18 * 60, 18, false, true));
  // A visible all-day event remains active throughout today and blocks tomorrow.
  assert(!calendarEventEnded(true, 200, 100, 23 * 60 + 59, 0, 0));
  assert(!calendarShowTomorrow(23 * 60 + 59, 18, true, true));
  assert(!calendarShowTomorrow(18 * 60, 18, true, true));
  assert(!calendarShowTomorrow(20 * 60 + 29, 18, true, true));
  assert(calendarShowTomorrow(20 * 60 + 30, 18, false, true));
  assert(!calendarShowTomorrow(0, 18, false, true));
  // Empty tomorrow keeps today even after the cutoff and last event.
  assert(!calendarShowTomorrow(18 * 60, 18, false, false));
  assert(!calendarShowTomorrow(23 * 60, 18, false, false));
  assert(!calendarShowTomorrow(20 * 60, 18, true, false));
  // Future evening events block the switch even between appointments.
  assert(!calendarTimedEventEnded(100, 200, 18*60, 19*60, 20*60));
  assert(calendarTimedEventEnded(200, 200, 20*60, 19*60, 20*60));
  // Epoch end handles midnight-spanning/multi-day events.
  assert(!calendarTimedEventEnded(100, 300, 23*60, 22*60, 60));
  assert(calendarTimedEventEnded(300, 300, 60, 22*60, 60));
  assert(!calendarTimedEventEnded(100, 0, 23*60, 22*60, 60));
  assert(!calendarTimedEventEnded(100, 0, 18*60, -1, -1));
  assert(calendarTimedEventEnded(100, 0, 18*60, 16*60, 17*60));
  std::cout << "Calendar checks passed\n";
}
