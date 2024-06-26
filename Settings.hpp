#pragma once

#include <pico/unique_id.h>
#include <pico/platform.h>

struct PumpConfig
{
  bool enable;
  float rate; // mL per second
  float amount; // mL
  int32_t activationTime; // seconds since midnight
  void print();
};

struct LightConfig
{
  bool enable;
  int32_t onTime; // seconds since midnight
  int32_t offTime; // seconds since midnight
  void print();
};

struct Settings
{
public:
  uint64_t crc;   // crc of the settings object
  size_t size = sizeof(Settings);  // size of the whole settings object
  pico_unique_board_id_t boardId;
  bool autosave;
  char wifiSsid[256];
  char wifiPassword[256]; // only wpa2-psk auth supported
  float offsetFromUtc; // hours
  PumpConfig pump1;
  PumpConfig pump2;
  PumpConfig pump3;
  PumpConfig pump4;
  LightConfig light1;
  LightConfig light2;

  PumpConfig& pump(int i);
  LightConfig& light(int i);

  const PumpConfig& pump(int i) const;
  const LightConfig& light(int i) const;

  // Set all settings to their default values
  void setDefaults();

  // Returns true if all settings are ok, false if any had to be changed 
  bool validateAll();

  // Write the settings object to the last two flash sectors.
  // Updates the size, board ID, and crc as a side effect.
  // Writing it twice ensures there is always one valid copy
  // in the event of power failure during write.
  // Returns false if flash contents is already correct
  bool writeToFlash();

  // Read the settings from flash memory. Returns false
  // if none of the flash sectors with settings were valid
  bool readFromFlash();

  // Print all the settings to cout
  void print();
private:
  // Return true if the flash is actually written
  bool __no_inline_not_in_flash_func(writeToFlashInternal)(int sectorOffset);
  size_t structSize() const;
  uint64_t calculateCrc() const;
};

class SettingsManager
{
private:
  static const uint32_t Minimum_Write_Interval_Ms = 60000;
  absolute_time_t nextWriteTime;
  Settings settings;

public:
  SettingsManager();
  Settings& getSettings();
  bool autosave();
};