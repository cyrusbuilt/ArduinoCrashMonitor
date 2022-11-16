#include <Arduino.h>
#include "ArduinoCrashMonitor.h"

using namespace Watchdog;

void onCrash() {
  Serial.println(F("OH NO! We crashed!"));
  Serial.println(F("Rebooting MCU ..."));
  asm volatile (" jmp 0");  // Causes a soft reboot to occur (sketch restart).
}

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    delay(10);
  }

  CrashMonitor::begin();

  // Dump any existig crash reports to the console.
  CrashMonitor::dump(Serial);

  // If the crash report storage in EEPROM is full, clear it.
  if (CrashMonitor::isFull()) {
    Serial.println(F("Crash report storage full!"));
    Serial.println(F("Clearing existing crash reports from EEPROM ..."));

    CrashMonitor::clear();
  }

  // Set a custom crash handler.
  CrashMonitor::setUserCrashHandler(onCrash);

  // Enable the watchdog without a timeout of 2 seconds.
  CrashMonitor::enableWatchdog(Watchdog::CrashMonitor::Timeout_2s);
}

void loop() {
  // We signal once so we can be certain we'll get the full timeout.
  CrashMonitor::iAmAlive();
  while (true) {
    // Now hold here until the timeout elapses and the CrashMonitor considers
    // the firmware to be hung.
    ;
  }
}
