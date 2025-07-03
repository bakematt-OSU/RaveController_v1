// File: Debugger.h
#ifndef DEBUGGER_H
#define DEBUGGER_H

#include <Arduino.h>
#include <string.h>

/**
 * @file Debugger.h
 * @brief Singleton debug utility for Arduino with per-section verbosity levels and runtime control via serial commands.
 */

/// Default list of all possible debug sections (comma-separated)
static const char *DEFAULT_SECTIONS = "Accel,Microphone,LED_Control";
/// Maximum number of sections supported
static const uint8_t MAX_SECTIONS = 5;

/**
 * @class Debugger
 * @brief Manages debug output with per-section logging levels and serial command interface.
 */
class Debugger
{
public:
    /**
     * @brief Get the singleton instance.
     * @return Reference to Debugger singleton.
     */
    static Debugger &instance()
    {
        static Debugger inst;
        return inst;
    }

    /**
     * @brief Initialize Serial port for debug commands and output.
     * @param baud Baud rate to initialize (default: 115200).
     */
    void begin(unsigned long baud = 115200)
    {
        if (_initialized)
            return;
        Serial.begin(baud);
        if (Serial)
            while (!Serial)
                ;
        _initialized = true;
    }

    /**
     * @brief Set the global default verbosity level.
     * @param level New default level; messages <= this level will print if no section override.
     */
    void setDefaultLevel(uint8_t level)
    {
        _defaultLevel = level;
        Serial.print("Default debug level set to: ");
        Serial.println(level);
    }

    /**
     * @brief Get current default verbosity level.
     * @return Default debug level.
     */
    uint8_t getDefaultLevel() const
    {
        return _defaultLevel;
    }

    /**
     * @brief Set verbosity level for a specific section.
     * @param section Section name.
     * @param level New level for the section.
     */
    void setSectionLevel(const char *section, uint8_t level)
    {
        int idx = findSectionIndex(section);
        if (idx >= 0)
        {
            _sectionLevels[idx] = level;
            Serial.print("Section '");
            Serial.print(section);
            Serial.print("' level set to: ");
            Serial.println(level);
        }
    }

    /**
     * @brief Get verbosity level for a section.
     * @param section Section name.
     * @return Section level if found; otherwise default level.
     */
    uint8_t getSectionLevel(const char *section) const
    {
        int idx = findSectionIndex(section);
        return (idx >= 0) ? _sectionLevels[idx] : _defaultLevel;
    }

    /**
     * @brief Enable sections by comma-separated list (or 'all').
     * @param csv Sections CSV; default is DEFAULT_SECTIONS.
     */
    void setSections(const char *csv = DEFAULT_SECTIONS)
    {
        strncpy(_sectionsBuf, csv, sizeof(_sectionsBuf));
        _sectionsBuf[sizeof(_sectionsBuf) - 1] = '\0';
        Serial.print("Debug sections set to: ");
        Serial.println(_sectionsBuf);
        parseSections(_sectionsBuf);
    }

    /**
     * @brief Get the raw CSV of enabled sections.
     * @return Current sections CSV string.
     */
    const char *getSections() const
    {
        return _sectionsBuf;
    }

    /**
     * @brief Print a debug message using default section level.
     * @param section Section name.
     * @param msg Null-terminated message.
     */
    void print(const char *section, const char *msg) const
    {
        uint8_t lvl = getSectionLevel(section);
        print(section, lvl, msg);
    }

    /**
     * @brief Print a debug message if allowed by section and level.
     * @param section Section name.
     * @param level Verbosity level of this message.
     * @param msg Null-terminated message.
     */
    void print(const char *section, uint8_t level, const char *msg) const
    {
        uint8_t thresh = getSectionLevel(section);
        if (level > thresh)
            return;
        Serial.print("[");
        Serial.print(section);
        Serial.print("] ");
        Serial.println(msg);
    }

    /**
     * @brief Print any value if allowed by section and level.
     * @tparam T Any type supported by Serial.println.
     * @param section Section name.
     * @param level Verbosity level of this message.
     * @param value Value to print.
     */
    template <typename T>
    void print(const char *section, uint8_t level, const T &value) const
    {
        uint8_t thresh = getSectionLevel(section);
        if (level > thresh)
            return;
        Serial.print("[");
        Serial.print(section);
        Serial.print("] ");
        Serial.println(value);
    }

    /**
     * @brief Print a String message if allowed by section and level.
     * @param section Section name.
     * @param level Verbosity level of this message.
     * @param msg String message to print.
     */
    void print(const char *section, uint8_t level, const String &msg) const
    {
        print(section, level, msg.c_str());
    }

    /**
     * @brief Print any value using default section level.
     * @tparam T Any type supported by Serial.println.
     * @param section Section name.
     * @param value Value to print.
     */
    template <typename T>
    void print(const char *section, const T &value) const
    {
        uint8_t lvl = getSectionLevel(section);
        print(section, lvl, value);
    }

    /**
     * @brief Print help menu of serial debug commands.
     */
    void printHelp() const
    {
        Serial.println("Available debug commands:");
        Serial.println("  DEBUG HELP                  Show this help message");
        Serial.println("  DEBUG OFF                   Disable all debug output");
        Serial.println("  DEBUG <sections>            Set enabled sections");
        Serial.println("  DEBUG <sections> <level>    Set sections and default level");
        Serial.println("  DEBUG <section> <level>     Set single section level");
        Serial.println("  DBGLEVEL <n>                Set default debug level");
        Serial.println("  DEBUG LIST SECTIONS         List all possible sections");
        Serial.println("  DEBUG LIST LEVELS           List current section levels");
        Serial.println("  DEBUG LIST ALL              List sections and levels");
    }

    /**
     * @brief Read and parse a line from Serial for debug commands.
     * @return True if a debug command was consumed, false otherwise.
     */
    bool handleCommands()
    {
        if (!Serial.available())
            return false;
        String line = Serial.readStringUntil('\n');
        line.trim();
        return handleCommandLine(line);
    }

    /**
     * @brief Parse and execute a debug command from a given string.
     * @param line Full command line to parse.
     * @return True if the line was recognized as a debug command, false otherwise.
     */
    bool handleCommandLine(const String &line)
    {
        if (line.equalsIgnoreCase("DEBUG HELP"))
        {
            printHelp();
            return true;
        }
        if (line.equalsIgnoreCase("DEBUG OFF"))
        {
            setSections("");
            Serial.println("All debug disabled");
            return true;
        }
        if (line.startsWith("DEBUG "))
        {
            String arg = line.substring(6);
            arg.trim();
            if (arg.equalsIgnoreCase("LIST SECTIONS"))
            {
                listSections();
                return true;
            }
            if (arg.equalsIgnoreCase("LIST LEVELS"))
            {
                listSectionLevels();
                return true;
            }
            if (arg.equalsIgnoreCase("LIST ALL"))
            {
                listSections();
                listSectionLevels();
                return true;
            }
            int sp = arg.indexOf(' ');
            if (sp > 0)
            {
                String first = arg.substring(0, sp);
                String second = arg.substring(sp + 1);
                first.trim();
                second.trim();
                bool secIsNum = second.length() && isdigit(second.charAt(0));
                bool firstIsAll = first.equalsIgnoreCase("all");
                bool firstIsList = firstIsAll || (first.indexOf(',') >= 0);
                if (firstIsList && secIsNum)
                {
                    setSections(first.c_str());
                    setDefaultLevel((uint8_t)second.toInt());
                }
                else if (!firstIsList && secIsNum)
                {
                    setSectionLevel(first.c_str(), (uint8_t)second.toInt());
                }
                else
                {
                    setSections(arg.c_str());
                }
            }
            else
            {
                if (arg.equalsIgnoreCase("all"))
                    setSections(DEFAULT_SECTIONS);
                else
                    setSectionLevel(arg.c_str(), 0);
            }
            return true;
        }
        else if (line.startsWith("DBGLEVEL "))
        {
            setDefaultLevel((uint8_t)line.substring(9).toInt());
            return true;
        }
        return false;
    }

    // List all possible sections
    void listSections() const
    {
        Serial.print("Available sections: ");
        Serial.println(DEFAULT_SECTIONS);
    }

    // List each enabled section and its current level
    void listSectionLevels() const
    {
        Serial.println("Section levels:");
        for (uint8_t i = 0; i < _numSections; ++i)
        {
            Serial.print("  ");
            Serial.print(_sections[i]);
            Serial.print(" = ");
            Serial.println(_sectionLevels[i]);
        }
    }

private:
    Debugger() : _initialized(false), _defaultLevel(2)
    {
        strcpy(_sectionsBuf, DEFAULT_SECTIONS);
        parseSections(_sectionsBuf);
    }
    ~Debugger()
    {
        for (uint8_t i = 0; i < _numSections; ++i)
            free((void *)_sections[i]);
    }

    // Split CSV into section names and init their levels
    void parseSections(const char *csv)
    {
        _numSections = 0;
        char buf[64];
        strncpy(buf, csv, sizeof(buf));
        buf[sizeof(buf) - 1] = '\0';
        char *tok = strtok(buf, ",");
        while (tok && _numSections < MAX_SECTIONS)
        {
            strncpy(_sections[_numSections], tok, sizeof(_sections[_numSections]));
            _sections[_numSections][sizeof(_sections[_numSections]) - 1] = '\0';
            _sectionLevels[_numSections] = _defaultLevel;
            ++_numSections;
            tok = strtok(nullptr, ",");
        }
    }

    // Find index of a section name or return -1
    int findSectionIndex(const char *section) const
    {
        for (uint8_t i = 0; i < _numSections; ++i)
        {
            if (strcmp(_sections[i], section) == 0)
                return i;
        }
        return -1;
    }

    bool _initialized;
    uint8_t _defaultLevel;
    char _sectionsBuf[100];
    char _sections[MAX_SECTIONS][20];
    uint8_t _sectionLevels[MAX_SECTIONS];
    uint8_t _numSections;
};

/// Shortcut macro to access the Debugger singleton
#define DBG Debugger::instance()

#endif // DEBUGGER_H

// ==========================
// Example Usage (ExampleDebug.ino)
// ==========================
/**
 * In your sketch (.ino):
 *
 * #include <Arduino.h>
 * #include "Debugger.h"
 *
 * void setup() {
 *   DBG.begin(9600);
 *   Serial.println("Type 'DEBUG HELP' for commands");
 *   DBG.setDefaultLevel(3);
 *   DBG.setSections();
 *   DBG.setSectionLevel("Accel",1);
 *   DBG.listSections();
 *   DBG.listSectionLevels();
 * }
 *
 * void loop() {
 *   DBG.handleCommands();
 *   DBG.print("Accel", "Accel data (level 1)");
 *   DBG.print("Accel", 2, "Verbose accel (level 2)");
 *   DBG.print("LED_Control", 2, "LED update (level 2)");
 *   DBG.print("LED_Control", 4, "LED verbose (level 4)");
 *   DBG.print("Mode",2, sampleBuffer[i]);
 *   DBG.println("Microphone", "Mic reading ready");
 *   delay(1000);
 * }
 */

// ==============================================
// Example Serial Commands
// ==============================================
/*
// In the Serial Monitor at 9600 baud (or your chosen rate):

// 1. Show help menu
DEBUG HELP

// 2. List all available sections
DEBUG LIST SECTIONS

// 3. List current section levels
DEBUG LIST LEVELS

// 4. Enable only Accel and LED_Control at default level
DEBUG Accel,LED_Control

// 5. Set global default level to 1
DBGLEVEL 1

// 6. Disable 'Mode' section entirely
DEBUG Mode 0

// 7. Enable all sections at level 4
DEBUG all 4
*/
