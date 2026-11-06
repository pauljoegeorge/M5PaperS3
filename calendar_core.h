#pragma once
#include <stdint.h>

inline bool calendarShowTomorrow(int nowMin, int cutoffHour, bool timedRemaining, bool tomorrowHasEvents) {
  return nowMin >= cutoffHour * 60 && !timedRemaining && tomorrowHasEvents;
}

inline bool calendarTimedEventEnded(int64_t nowEpoch, int64_t endEpoch,
                                   int nowMin, int startMin, int endMin) {
  if (endEpoch > 0) return endEpoch <= nowEpoch;
  // Legacy HH:MM payloads cannot safely expire overnight events.
  if (startMin >= 0 && endMin < startMin) return false;
  return endMin >= 0 && endMin <= nowMin;
}
