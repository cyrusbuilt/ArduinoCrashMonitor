#include "Arduino.h"
#include "ArduinoCrashMonitor.h"

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    delay(10);
  }
  
  CrashMonitor.dump(Serial);
  CrashMonitor.enableWatchdog(Watchdog::CCrashMonitor::Timeout_2s);
}

void loop() {
  // We signal once so we can be certain we'll get the full timeout.
  CrashMonitor.iAmAlive();
  while (true) {
    // Now hold here until the timeout elapses and the CrashMonitor considers
    // the firmware to be hung.
    ;
  }
}
