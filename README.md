==========================================================================
# ArduinoCrashMonitor :: A crash monitor library that stores crash reports to EEPROM.
[![Build Status](https://travis-ci.org/cyrusbuilt/ArduinoCrashMonitor.svg?branch=master)](https://travis-ci.org/cyrusbuilt/ArduinoCrashMonitor)
==========================================================================

## Description

This library takes advantage of the Arduino's watchdog capability to trigger an
interrupt in the event that the Arduino sketch (firmware) becomes unresponsive
due to a crash (divide by zero, Ethernet shield fails to initialize, etc). The
general idea behind this is: during each iteration of loop(), CrashMonitor's
iAmAlive() method should be called to signal to CrashMonitor that the sketch is
still running. If CrashMonitor does not receive the iAmAlive() signal before the
end of the timeout, then an interrupt is triggered. The interrupt will call a
routine that will generate a small report that contains the address of the last
instruction the Arduino attempt to execute (and optionally some user data) and
write the report to EEPROM. You can also have the interrupt call a
user-specified callback. Once the report is written to EEPROM, the MCU will be
restarted or will hang indefinitely until manually reset. You can then use the
dump() method to load the crash reports and dump them to a Serial port.

## Crash Analysis

Having the address at which the crash occurred isn't very useful, unless you
know where in the code that address matches up to. To find out what line in your
sketch is the culprit, you will need to disassemble the ELF your sketch gets
compiled into. To do this, you will need to use the disassembler that comes with
your Arduino software installation. You can find the disassembler in one of
these locations:

Windows:
%programfiles(x86)%\\Arduino\\hardware\\tools\\avr\\bin

MacOS X:
/Applications/Arduino.app/Contents/Java/hardware/tools/avr/bin

Linux:
-- Typically already included in PATH.

And now the command:
```
avr-objdump -d -S -j .text CrashMonitorBasicExample.elf > disassembly.txt
```

The above command will take the compiled ELF file for the basic example sketch
and dump it out to a text file called 'disassembly.txt'. This will only work if
you've ran a build on the sketch first. Make sure you provide the correct path
to the ELF. Each crash report will be a line like this:
```
0: word-address=0x3AE: byte-address=0x75C, data=0x0
```

What we are interested in is the byte-address without the '0x' prefix. So using
the example above, you would search the 'disassembly.txt' file for '75C', which
will take you to the line where the crash occurred. NOTE: The disassembly text
contains the C/C++ code with the Assembly code interleaved.

## How to use

Copy the entire folder containing this library to the "libraries" folder
of your Arduino installation. Then include CrashMonitor.h in your sketch.  See
example below:

```cpp
#include <Arduino.h>
#include "CrashMonitor.h"

using namespace Watchdog;

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    delay(10);
  }

  CrashMonitor::begin();

  // Dump any crash reports to the serial port.
  CrashMonitor::dump(Serial);
  if (CrashMonitor::isFull()) {
    // We've stored as many crash reports as we can. Clear out the old ones so
    // we can have room for new ones.
    Serial.println(F("CrashMonitor report storage full."));
    Serial.println(F("Clearing crash reports from EEPROM ..."));
    CrashMonitor::clear();
  }

  // Enable the watchdog timer with a timeout of 2 seconds.
  CrashMonitor::enableWatchdog(Watchdog::CrashMonitor::Timeout_2s);
}

void loop() {
  // Let the watchdog know that we are still ok.
  CrashMonitor::iAmAlive();
}
```

## How to install
For PlatformIO:
```
pio lib install ArduinoCrashMonitor
```

For Arduino IDE:
See https://www.arduino.cc/en/Guide/Libraries
