/**
 * CrashMonitor.cpp
 * Version 1.0a
 * Author
 *  Cyrus Brunner
 *
 * This library provides a means of monitoring for and capturing firmware
 * crashes and stores the crash reports in EEPROM.
 */

#include "ArduinoCrashMonitor.h"
#include <avr/eeprom.h>

using namespace Watchdog;

/**
 * Interrupt Service Request. This function is called when the watchdog
 * interrupt fires. The function is naked so that we don't get program states
 * pushed onto the stack. Consequently, the top two values on the stack will be
 * the program counter when the interrupt fired. We're going to save that in the
 * EEPROM then let the second watchdog event reset the MCU. We never return from
 * this function.
 */
ISR(WDT_vect, ISR_NAKED) {
  // Setup a pointer to the program counter. It goes in a register so we don't
  // mess up the stack.
  register uint8_t *upStack;
  upStack = (uint8_t*)SP;

  // The stack pointer on the AVR MCU points to the next available location so
  // we want to go back one location to get the first byte of the address pushed
  // onto the stack when the interrupt was triggered. There will be
  // PROGRAM_COUNTER_SIZE bytes there.
  ++upStack;
  CrashMonitor.watchDogInterruptHandler(upStack);
}

CCrashMonitor::CCrashMonitor(int baseAddress, int maxEntries)
  : _nBaseAddress(baseAddress), _nMaxEntries(maxEntries) {
  this->_crashReport.uData = 0;
}

void CCrashMonitor::enableWatchdog(CCrashMonitor::ETimeout timeout) {
  wdt_enable(timeout);
  WDTCSR |= _BV(WDIE);
}

void CCrashMonitor::disableWatchdog() {
  wdt_disable();
}

void CCrashMonitor::iAmAlive() const {
  wdt_reset();
}

void CCrashMonitor::readBlock(int baseAddress, void *pData, uint8_t uSize) const {
  uint8_t *puData = (uint8_t *)pData;
  while (uSize--) {
    *puData++ = eeprom_read_byte((const uint8_t *)baseAddress++);
  }
}

void CCrashMonitor::writeBlock(int baseAddress, const void *pData, uint8_t uSize) const {
  const uint8_t *puData = (const uint8_t *)pData;
  while (uSize--) {
    eeprom_write_byte((uint8_t *)baseAddress++, *puData++);
  }
}

void CCrashMonitor::loadHeader(CCrashMonitorHeader &reportHeader) const {
  this->readBlock(_nBaseAddress, &reportHeader, sizeof(reportHeader));

  // Ensure the report structure is valid.
  if (reportHeader.savedReports == 0xff) {
    // EEPROM is 0xff when unintialized.
    reportHeader.savedReports = 0;
  }
  else if (reportHeader.savedReports > _nMaxEntries) {
    reportHeader.savedReports = _nMaxEntries;
  }

  if (reportHeader.uNextReport >= _nMaxEntries) {
    reportHeader.uNextReport = 0;
  }
}

void CCrashMonitor::saveHeader(const CCrashMonitorHeader &reportHeader) const {
  this->writeBlock(_nBaseAddress, &reportHeader, sizeof(reportHeader));
}

int CCrashMonitor::getAddressForReport(int report) const {
  int address = _nBaseAddress + sizeof(CCrashMonitorHeader);
  if (report < _nMaxEntries) {
    address += report * sizeof(_crashReport);
  }
  return address;
}

void CCrashMonitor::saveCurrentReport(int reportSlot) const {
  this->writeBlock(this->getAddressForReport(reportSlot), &_crashReport, sizeof(_crashReport));
}

void CCrashMonitor::loadReport(int report, CCrashReport &state) const {
  this->readBlock(this->getAddressForReport(report), &state, sizeof(state));

  // The return address is reversed when we read if off the stack. Correct that
  // by reversing the byte order. Assuming PROGRAM_COUNTER_SIZE is 2 or 3.
  uint8_t temp = state.auAddress[0];
  state.auAddress[0] = state.auAddress[PROGRAM_COUNTER_SIZE - 1];
  state.auAddress[PROGRAM_COUNTER_SIZE - 1] = temp;
}

void CCrashMonitor::printValue(Print &destination, const __FlashStringHelper *pLabel,
                                uint32_t uValue, uint8_t uRadix, bool newLine) const {
  destination.print(pLabel);
  destination.print(uValue, uRadix);
  if (newLine) {
    destination.println();
  }
}

void CCrashMonitor::dump(Print &destination, bool onlyIfPresent) const {
  CCrashMonitorHeader header;
  this->loadHeader(header);
  if ((!onlyIfPresent) || (header.savedReports != 0)) {
    CCrashReport report;
    uint32_t uAddress;

    destination.println(F("Crash Monitor"));
    destination.println(F("-------------"));
    this->printValue(destination, F("Saved reports: "), header.savedReports, DEC, true);
    this->printValue(destination, F("Next report: "), header.uNextReport, DEC, true);
    for (uint8_t uReport = 0; uReport < header.savedReports; ++uReport) {
      this->loadReport(uReport, report);

      destination.print(uReport);
      uAddress = 0;
      memcpy(&uAddress, report.auAddress, PROGRAM_COUNTER_SIZE);
      this->printValue(destination, F(": word-address=0x"), uAddress, HEX, false);
      this->printValue(destination, F(": byte-address=0x"), uAddress * 2, HEX, false);
      this->printValue(destination, F(", data=0x"), report.uData, HEX, true);
    }
  }
}

void CCrashMonitor::clear() {
  // Load the report header so we can determine how many reports we need to clear.
  CCrashMonitorHeader header;
  this->loadHeader(header);
  if (header.savedReports != 0) {
    // We have at least one report.
    uint32_t uAddress;
    int sz = 0;
    CCrashReport report;

    // Loop through the report addresses and load each report.
    for (uint8_t uReport = 0; uReport < header.savedReports; ++uReport) {
      this->loadReport(uReport, report);

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
  this->saveHeader(header);
}

bool CCrashMonitor::isFull() {
  CCrashMonitorHeader header;
  this->loadHeader(header);
  return (header.savedReports >= _nMaxEntries);
}

void CCrashMonitor::setUserCrashHandler(void (*onUserCrashEvent)()) {
  this->userCrashHandler = onUserCrashEvent;
}

void CCrashMonitor::watchDogInterruptHandler(uint8_t *puProgramAddress) {
  CCrashMonitorHeader header;

  this->loadHeader(header);
  memcpy(_crashReport.auAddress, puProgramAddress, PROGRAM_COUNTER_SIZE);
  this->saveCurrentReport(header.uNextReport);

  // Update header for next time.
  ++header.uNextReport;
  if (header.uNextReport >= _nMaxEntries) {
    header.uNextReport = 0;
  }
  else {
    ++header.savedReports;
  }

  this->saveHeader(header);

  // Wait for next watchdog timeout to reset the system. If the watchdog timeout
  // is too short, it doesn't give the program much time to reset it before the
  // next timeout. So we can be a bit generous here.
  wdt_enable(WDTO_120MS);
  if (this->userCrashHandler != NULL) {
    this->userCrashHandler();
  }
  else {
    while (true) {
      ;
    }
  }
}

CCrashMonitor CrashMonitor;
