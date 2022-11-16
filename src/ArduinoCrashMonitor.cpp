/**
 * ArduinoCrashMonitor.cpp
 * Version 1.4
 * Author
 *  Cyrus Brunner
 *
 * This library provides a means of monitoring for and capturing firmware
 * crashes and stores the crash reports in EEPROM.
 */

#include "ArduinoCrashMonitor.h"
#include <avr/eeprom.h>

using namespace Watchdog;

// Init static vars
int CrashMonitor::_nBaseAddress = 500;
int CrashMonitor::_nMaxEntries = DEFAULT_ENTRIES;
CCrashReport CrashMonitor::_crashReport;
STATICFUNC CrashMonitor::userCrashHandler = NULL;

void CrashMonitor::begin(int baseAddress, int maxEntries) {
  CrashMonitor::_nBaseAddress = baseAddress;
  CrashMonitor::_nMaxEntries = maxEntries;
  CrashMonitor::_crashReport.uData = 0;
}

void CrashMonitor::enableWatchdog(CrashMonitor::ETimeout timeout) {
  wdt_enable(timeout);
  WDTCSR |= _BV(WDIE);
}

void CrashMonitor::disableWatchdog() {
  wdt_disable();
}

void CrashMonitor::iAmAlive() {
  wdt_reset();
}

void CrashMonitor::readBlock(int baseAddress, void *pData, uint8_t uSize) {
  uint8_t *puData = (uint8_t *)pData;
  while (uSize--) {
    *puData++ = eeprom_read_byte((const uint8_t *)baseAddress++);
  }
}

void CrashMonitor::writeBlock(int baseAddress, const void *pData, uint8_t uSize) {
  const uint8_t *puData = (const uint8_t *)pData;
  while (uSize--) {
    eeprom_write_byte((uint8_t *)baseAddress++, *puData++);
  }
}

void CrashMonitor::loadHeader(CCrashMonitorHeader &reportHeader) {
  CrashMonitor::readBlock(CrashMonitor::_nBaseAddress, &reportHeader, sizeof(reportHeader));

  // Ensure the report structure is valid.
  if (reportHeader.savedReports == 0xff) {
    // EEPROM is 0xff when unintialized.
    reportHeader.savedReports = 0;
  }
  else if (reportHeader.savedReports > CrashMonitor::_nMaxEntries) {
    reportHeader.savedReports = CrashMonitor::_nMaxEntries;
  }

  if (reportHeader.uNextReport >= CrashMonitor::_nMaxEntries) {
    reportHeader.uNextReport = 0;
  }
}

void CrashMonitor::saveHeader(const CCrashMonitorHeader &reportHeader) {
  CrashMonitor::writeBlock(CrashMonitor::_nBaseAddress, &reportHeader, sizeof(reportHeader));
}

int CrashMonitor::getAddressForReport(int report) {
  int address = CrashMonitor::_nBaseAddress + sizeof(CCrashMonitorHeader);
  if (report < CrashMonitor::_nMaxEntries) {
    address += report * sizeof(CrashMonitor::_crashReport);
  }
  return address;
}

void CrashMonitor::saveCurrentReport(int reportSlot) {
  int addr = CrashMonitor::getAddressForReport(reportSlot);
  CrashMonitor::writeBlock(addr, &CrashMonitor::_crashReport, sizeof(CrashMonitor::_crashReport));
}

void CrashMonitor::loadReport(int report, CCrashReport &state) {
  CrashMonitor::readBlock(CrashMonitor::getAddressForReport(report), &state, sizeof(state));

  // The return address is reversed when we read if off the stack. Correct that
  // by reversing the byte order. Assuming PROGRAM_COUNTER_SIZE is 2 or 3.
  uint8_t temp = state.auAddress[0];
  state.auAddress[0] = state.auAddress[PROGRAM_COUNTER_SIZE - 1];
  state.auAddress[PROGRAM_COUNTER_SIZE - 1] = temp;
}

void CrashMonitor::printValue(Print &destination, const __FlashStringHelper *pLabel,
                                uint32_t uValue, uint8_t uRadix, bool newLine) {
  destination.print(pLabel);
  destination.print(uValue, uRadix);
  if (newLine) {
    destination.println();
  }
}

void CrashMonitor::dump(Print &destination, bool onlyIfPresent) {
  CCrashMonitorHeader header;
  CrashMonitor::loadHeader(header);
  if ((!onlyIfPresent) || (header.savedReports != 0)) {
    CCrashReport report;
    uint32_t uAddress;

    destination.println(F("Crash Monitor"));
    destination.println(F("-------------"));
    CrashMonitor::printValue(destination, F("Saved reports: "), header.savedReports, DEC, true);
    CrashMonitor::printValue(destination, F("Next report: "), header.uNextReport, DEC, true);
    for (uint8_t uReport = 0; uReport < header.savedReports; ++uReport) {
      CrashMonitor::loadReport(uReport, report);

      destination.print(uReport);
      uAddress = 0;
      memcpy(&uAddress, report.auAddress, PROGRAM_COUNTER_SIZE);
      CrashMonitor::printValue(destination, F(": word-address=0x"), uAddress, HEX, false);
      CrashMonitor::printValue(destination, F(": byte-address=0x"), uAddress * 2, HEX, false);
      CrashMonitor::printValue(destination, F(", data=0x"), report.uData, HEX, true);
    }
  }
}

void CrashMonitor::clear() {
  // Load the report header so we can determine how many reports we need to clear.
  CCrashMonitorHeader header;
  CrashMonitor::loadHeader(header);
  if (header.savedReports != 0) {
    // We have at least one report.
    uint32_t uAddress;
    int sz = 0;
    CCrashReport report;

    // Loop through the report addresses and load each report.
    for (uint8_t uReport = 0; uReport < header.savedReports; ++uReport) {
      CrashMonitor::loadReport(uReport, report);

      // Determine the size of the report (number of bytes to delete), then
      // starting at the address of the report, write zeros for the length of
      // the report.
      uAddress = 0;
      memcpy(&uAddress, report.auAddress, PROGRAM_COUNTER_SIZE);
      sz = sizeof(report);
      while (sz--) {
        eeprom_write_byte((uint8_t *)uAddress++, 0);
      }
    }
  }

  // Now clear out the header.
  header.savedReports = 0;
  header.uNextReport = 0;
  CrashMonitor::saveHeader(header);
}

bool CrashMonitor::isFull() {
  CCrashMonitorHeader header;
  CrashMonitor::loadHeader(header);
  return (header.savedReports >= CrashMonitor::_nMaxEntries);
}

void CrashMonitor::setUserCrashHandler(void (*onUserCrashEvent)()) {
  CrashMonitor::userCrashHandler = onUserCrashEvent;
}

void CrashMonitor::watchDogInterruptHandler(uint8_t *puProgramAddress) {
  CCrashMonitorHeader header;

  CrashMonitor::loadHeader(header);
  memcpy(CrashMonitor::_crashReport.auAddress, puProgramAddress, PROGRAM_COUNTER_SIZE);
  CrashMonitor::saveCurrentReport(header.uNextReport);

  // Update header for next time.
  ++header.uNextReport;
  if (header.uNextReport >= CrashMonitor::_nMaxEntries) {
    header.uNextReport = 0;
  }
  else {
    ++header.savedReports;
  }

  CrashMonitor::saveHeader(header);

  // Wait for next watchdog timeout to reset the system. If the watchdog timeout
  // is too short, it doesn't give the program much time to reset it before the
  // next timeout. So we can be a bit generous here.
  wdt_enable(WDTO_120MS);
  if (CrashMonitor::userCrashHandler != NULL) {
    CrashMonitor::userCrashHandler();
  }
  else {
    while (true) {
      ;
    }
  }
}

/**
 * @brief Interrupt Service Request. This function is called when the watchdog
 * interrupt fires. The function is naked so that we don't get program states
 * pushed onto the stack. Consequently, the top two values on the stack will be
 * the program counter when the interrupt fired. We're going to save that in the
 * EEPROM then let the second watchdog event reset the MCU. We never return from
 * this function.
 */
ISR(WDT_vect) {
  // Setup a pointer to the program counter. It goes in a register so we don't
  // mess up the stack.
  register uint8_t *upStack;
  upStack = (uint8_t*)SP;

  // The stack pointer on the AVR MCU points to the next available location so
  // we want to go back one location to get the first byte of the address pushed
  // onto the stack when the interrupt was triggered. There will be
  // PROGRAM_COUNTER_SIZE bytes there.
  ++upStack;
  Watchdog::CrashMonitor::watchDogInterruptHandler(upStack);
}
