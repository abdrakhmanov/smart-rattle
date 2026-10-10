#include "Accel.h"

#include <M5Unified.h>

#include "Config.h"

void Accel::begin() {
  _enabled = M5.Imu.isEnabled();
  _det.reset();
  _primed = false;
  _lastUpdateMs = 0;
  _lastMotionMs = 0;

#if MARACA_TRACE
  Serial.println("t_ms,ax,ay,az,gx,gy,gz,excess,state,ev");
#endif
}

void Accel::update(uint32_t nowMs, uint32_t dtMs) {
  if (!_enabled) return;

  if (!_primed) {
    _primed = true;
    _lastMotionMs = nowMs;
  }
  if (dtMs == 0) dtMs = 1;
  _lastUpdateMs = nowMs;

  float ax = 0.0f, ay = 0.0f, az = 0.0f;
  float gx = 0.0f, gy = 0.0f, gz = 0.0f;
  M5.Imu.getAccel(&ax, &ay, &az);   // g
  M5.Imu.getGyro(&gx, &gy, &gz);    // °/с

  _det.update(ax, ay, az, gx, gy, gz, dtMs);

  if (_det.motionSeen()) _lastMotionMs = nowMs;

#if MARACA_TRACE
  const int ev = _det.shakeStarted() ? 1 : (_det.shakeEnded() ? 2 : 0);
  Serial.printf("%lu,%.4f,%.4f,%.4f,%.2f,%.2f,%.2f,%.4f,%u,%d\n",
                (unsigned long)nowMs, ax, ay, az, gx, gy, gz,
                _det.excess(), (unsigned)_det.stateCode(), ev);
#endif
}
