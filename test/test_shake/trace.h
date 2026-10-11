#pragma once
#include <stdio.h>
#include <stdlib.h>

#include "ShakeDetector.h"

struct TraceResult {
  int  rows    = 0;
  int  started = 0;
  int  ended   = 0;
  bool loaded  = false;
};

// Формат CSV из MARACA_TRACE (Task 7):
// t_ms,ax,ay,az,gx,gy,gz,excess,state,ev
// Первая строка — заголовок. Трассы хранятся в test/traces/.
inline TraceResult replayTrace(const char* path) {
  TraceResult r;
  FILE* f = fopen(path, "r");
  if (!f) return r;
  r.loaded = true;

  char line[256];
  if (!fgets(line, sizeof(line), f)) { fclose(f); return r; }

  ShakeDetector d;
  d.reset();
  uint32_t prevT = 0;
  bool first = true;
  while (fgets(line, sizeof(line), f)) {
    unsigned long t = 0;
    float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
    if (sscanf(line, "%lu,%f,%f,%f,%f,%f,%f", &t, &ax, &ay, &az, &gx, &gy, &gz) != 7) {
      continue;
    }
    uint32_t dt = first ? 10u : (uint32_t)(t - prevT);
    if (dt == 0) dt = 1;
    if (dt > 100) dt = 100;
    first = false;
    prevT = (uint32_t)t;

    d.update(ax, ay, az, gx, gy, gz, dt);
    ++r.rows;
    if (d.shakeStarted()) ++r.started;
    if (d.shakeEnded())   ++r.ended;
  }
  fclose(f);
  return r;
}
