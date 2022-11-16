/**
 * ArduinoCrashMonitor.h
 * Version 1.4
 * Author
 *  Cyrus Brunner
 *
 * This library provides a means of monitoring for and capturing firmware
 * crashes and stores the crash reports in EEPROM.
 */

#ifndef ArduinoCrashMonitor_h
#define ArduinoCrashMonitor_h

#include <Arduino.h>
#ifndef __AVR__
  #error "This library only compatible with the Arduino/Atmel AVR-based controllers."
#endif

#include <avr/wdt.h>

namespace Watchdog
{
  #ifdef __AVR_ATmega2560__
    #define PROGRAM_COUNTER_SIZE 3
  #else
    #define PROGRAM_COUNTER_SIZE 2
  #endif

  typedef void (*STATICFUNC)();

  /**
   * @brief Crash monitor header.
   */
  struct CCrashMonitorHeader
  {
    /**
     * @brief The number of reports saved in the EEPROM.
     */
    uint8_t savedReports;

    /**
     * @brief The location for the next report to be saved.
     */
    uint8_t uNextReport;
  } __attribute__((__packed__));

  /**
   * @brief Crash report info.
   */
  struct CCrashReport
  {
    /**
     * @brief The address of code executing when the watchdog interrupt fired. On the
     * 328 & 644, this is just a word pointer. For the Mega, we need 3 bytes. We
     * can use an array to make it easy.
     */
    uint8_t auAddress[PROGRAM_COUNTER_SIZE];

    /**
     * @brief User data.
     */
    uint32_t uData;
  } __attribute__((__packed__));

  /**
   * @brief The crash monitor class. A crash monitor that implements a watchdog that
   * fires an interrupt if it defects a possible program hang. You set a timeout
   * value and the in your sketch's loop() method, you call
   * CCrashMonitor::iAmAlive() to reset the watchdog timer and indicate that
   * everything is still operating normally. If the crash monitor does not get
   * the iAmAlive signal before the end of the timeout, it will build and store
   * a crash report in EEPROM and then reset the MCU.
   */
  class CrashMonitor
  {
    // The address in the EEPROM where crash data is saved. The first byte is
    // the number of records saved, followed by the location for the next report
    // to be saved, followed by the individual CCrashReport records.
    static int _nBaseAddress;

    // The maximum number of crash entries stored in the EEPROM.
    static int _nMaxEntries;
    enum EConstants { DEFAULT_ENTRIES = 10 };
    static CCrashReport _crashReport;

  public:
    /**
     * @brief Initializes the crash monitor.
     * @param baseAddress The address in the EEPROM where crash data should be stored.
     * @param maxEntries The maximum number of crash entries that should be stored
     * in the EEPROM. Storage of EEPROM data will take up sizeof(CCrashMonitorHeader) +
     * _nMaxEntries * sizeof(CCrashReport) bytes in the EEPROM.
     */
    static void begin(int baseAddress = 500, int maxEntries = DEFAULT_ENTRIES);

    /**
     * @brief Dumps data to the specified destination.
     * @param destination   Any destination object of type Print (ie. Serial).
     * @param onlyIfPresent Only attempt to dump if the specified destination is
     * present.
     */
    static void dump(Print &destination, bool onlyIfPresent = true);

    /**
     * @brief Possible timeout values.
     */
    enum ETimeout
    {
      Timeout_15ms = WDTO_15MS,
      Timeout_30ms = WDTO_30MS,
      Timeout_60ms = WDTO_60MS,
      Timeout_120ms = WDTO_120MS,
      Timeout_250ms = WDTO_250MS,
      Timeout_500ms = WDTO_500MS,
      Timeout_1s = WDTO_1S,
      Timeout_2s = WDTO_2S,
    #ifdef WDTO_4S
      Timeout_4s = WDTO_4S,
    #endif
    #ifdef WDTO_8S
      Timeout_8s = WDTO_8S
    #endif
    };

    /**
     * @brief Enable the watchdog timer & have it trigger an interrupt before resetting
     * the MCU. When the interrupt fires, we save the program counter to the
     * EEPROM.
     * @param timeout The timeout value to wait before considering the Firmware
     * to be hung. NOTE: On the ESP platform, changing the default timeout is
     * not yet supported and the specified timeout value here has no effect.
     */
    static void enableWatchdog(ETimeout timeout);

    /**
     * @brief Disables the watchdog timer.
     */
    static void disableWatchdog();

    /**
     * @brief Lets the watchdog timer know the program is still alive. Call
     * this before the watchdog timeout elapses to prevent program being aborted.
     */
    static void iAmAlive();

    /**
     * @brief Sets user data to be included in crash report.
     * @param data The data to include.
     */
    static void setData(uint32_t data) { _crashReport.uData = data; }

    /**
     * @brief Gets the user data being included in the crash report.
     * @return The user data being included in the crash report.
     */
    static uint32_t getData() { return _crashReport.uData; }

    /**
     * @brief Set the program address for the watchdog interrupt handler.
     * @param puProgramAddress The program address.
     */
    static void watchDogInterruptHandler(uint8_t *puProgramAddress);

    /**
     * @brief Sets a user crash event handler. If set, this callback will be executed
     * by the interrupt handler after that crash report is generated and stored.
     * @param onUserCrashEvent A user callback to execute. If NULL, then the
     * firmware will simply halt until the board is power-cycled or reset.
     */
    static void setUserCrashHandler(void (*onUserCrashEvent)());

    /**
     * @brief Deletes all saved reports from EEPROM.
     */
    static void clear();

    /**
     * @brief Determines whether or not the maximum number of reports have been saved.
     * @return true if the EEPROM storage for crash reports is full; Otherwise;
     * false.
     */
    static bool isFull();

  private:
    /**
     * @brief Saves the header to EEPROM.
     * @param reportHeader The crash report header.
     */
    static void saveHeader(const CCrashMonitorHeader &reportHeader);

    /**
     * @brief Loads the crash report header from EEPROM
     * @param reportHeader The header to store the read data in.
     */
    static void loadHeader(CCrashMonitorHeader &reportHeader);

    /**
     * @brief Saves the current crash report to EEPROM.
     * @param reportSlot The slot to store the report in.
     */
    static void saveCurrentReport(int reportSlot);

    /**
     * @brief Loads the crash report from EEPROM.
     * @param report The report to load.
     * @param state  The state struct to load the crash report into.
     */
    static void loadReport(int report, CCrashReport &state);

    /**
     * @brief Gets the EEPROM address to store the report in.
     * @param  report The report ID.
     * @return        The address to save the report in.
     */
    static int getAddressForReport(int report);

    /**
     * @brief Reads a block of data from EEPROM.
     * @param baseAddress The base address to start reading from.
     * @param pData       The program data.
     * @param uSize       The size of the block to read.
     */
    static void readBlock(int baseAddress, void *pData, uint8_t uSize);

    /**
     * @brief Writes a block of data to EEPROM.
     * @param baseAddress The base address to start writing at.
     * @param pData       The program data to write.
     * @param uSize       The size of the data block to write.
     */
    static void writeBlock(int baseAddress, const void *pData, uint8_t uSize);

    /**
     * @brief Prints the specified value to the specified target.
     * @param destination The destination target to print the value to (ie. Serial).
     * @param pLabel      A label for the value.
     * @param uValue      The value to print.
     * @param uRadix      A radix (formatter) for the data (ie. DEC, HEX, etc).
     * @param newLine     Set true if the data should terminate with a new line.
     */
    static void printValue(Print &destination, const __FlashStringHelper *pLabel,
      uint32_t uValue, uint8_t uRadix, bool newLine);

    static STATICFUNC userCrashHandler;
  };
}
#endif
