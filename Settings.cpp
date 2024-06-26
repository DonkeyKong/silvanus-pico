#include "Settings.hpp"

#include <hardware/flash.h>
#include <pico/stdlib.h>
#include <hardware/sync.h>
#include <pico/multicore.h>

//#include <stdio.h>
#include <vector>
#include <cstring>
#include <iostream>
#include <algorithm>

extern "C" uint64_t crc64(uint64_t crc, const unsigned char* data, uint64_t len);

namespace
{
  bool operator!=(const pico_unique_board_id_t& id1, const pico_unique_board_id_t& id2)
  {
    for (int i=0; i < sizeof(pico_unique_board_id_t::id); ++i)
    {
      if (id1.id[i] != id2.id[i]) return true;
    }
    return false;
  }

  template <typename T>
  bool validate(T& field, T min, T max, T defaultVal)
  {
    if (field < min || field > max)
    {
      field = defaultVal;
      return true;
    }
    return false;
  }

  uint32_t settingsOffsetBytes(int sectorOffset)
  {
    return PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE * (sectorOffset + 1);
  }

  const Settings* flashSettingsPtr(int sectorOffset)
  {
    return (Settings*)(XIP_BASE+settingsOffsetBytes(sectorOffset));
  }
}

void Settings::setDefaults()
{
  size = sizeof(Settings);
  pico_get_unique_board_id(&boardId);
  autosave = false;
  memcpy(wifiSsid, "wifi", 5);
  memcpy(wifiPassword, "password", 9);
  offsetFromUtc = -5.0f; // EST in the US
  pump1 = { true, 1.3f, 80.0f, 8 * 60 * 60 };
  pump2 = { false, 1.3f, 80.0f, 8 * 60 * 60 };
  pump3 = { false, 1.3f, 80.0f, 8 * 60 * 60 };
  pump4 = { false, 1.3f, 80.0f, 8 * 60 * 60 };
  light1 = { true, 8 * 60 * 60, (8 + 12) * 60 * 60};
  light2 = { false, 8 * 60 * 60, (8 + 12) * 60 * 60};
  crc = calculateCrc();
}

bool Settings::validateAll()
{
  // Validate the settings to make sure they are ok after load
  bool failedValidation = false;
  // tbd: actually validate stuff
  return !failedValidation;
}

bool Settings::writeToFlash()
{
  size = sizeof(Settings);
  pico_get_unique_board_id(&boardId);
  crc = calculateCrc();
  bool wrote0 = writeToFlashInternal(0);
  bool wrote1 = writeToFlashInternal(1);
  return wrote0 || wrote1;
}

bool Settings::readFromFlash()
{
  for (int i=0; i < 2; ++i)
  {
    std::cout << "Loading settings from sector " << i << std::endl << std::flush;
    Settings flashSettings = *flashSettingsPtr(i);
    uint64_t crc = flashSettings.calculateCrc();
    if (flashSettings.crc == crc)
    {
      memcpy(this, &flashSettings, flashSettings.structSize());
      return true;
    }
    else
    {
      std::cout << "CRC check failed!" << std::endl << std::flush;
    }
  }
  return false;
}

void PumpConfig::print()
{
  std::cout << "enable: " << (enable ? "1" : "0") << std::endl;
  std::cout << "rate: " << rate << " mL/sec" << std::endl;
  std::cout << "amount: " << amount << " mL" << std::endl;
  std::cout << "activationTime: " << activationTime << " secs after midnight" << std::endl << std::flush;
}

void LightConfig::print()
{
  std::cout << "enable: " << (enable ? "1" : "0") << std::endl;
  std::cout << "onTime: " << onTime << " secs after midnight" << std::endl;
  std::cout << "offTime: " << offTime << " secs after midnight" << std::endl << std::flush;
}

void Settings::print()
{
  std::cout << "-- Silvanus --" << std::endl;
  std::cout << "crc: " << crc << std::endl;
  std::cout << "size: " << size << std::endl;
  std::cout << "boardId: " << *((uint64_t*)boardId.id) << std::endl;
  std::cout << "autosave: " << (autosave ? "1" : "0") << std::endl;
  std::cout << "wifiSsid: " << wifiSsid << std::endl;
  std::cout << "wifiPassword: " << wifiPassword << std::endl;
  std::cout << "offsetFromUtc: " << offsetFromUtc << " hours" << std::endl << std::flush;
  std::cout << "-- Pump 1 --" << std::endl;
  pump1.print();
  std::cout << "-- Pump 2 --" << std::endl;
  pump2.print();
  std::cout << "-- Pump 3 --" << std::endl;
  pump3.print();
  std::cout << "-- Pump 4 --" << std::endl;
  pump4.print();
  std::cout << "-- Light 1 --" << std::endl;
  light1.print();
  std::cout << "-- Light 2 --" << std::endl;
  light2.print();
}

PumpConfig& Settings::pump(int i)
{
  switch (i)
  {
    case 0: return pump1;
    case 1: return pump2;
    case 2: return pump3;
    case 3: return pump4;
    default: return pump1;
  }
}

const PumpConfig& Settings::pump(int i) const
{
  switch (i)
  {
    case 0: return pump1;
    case 1: return pump2;
    case 2: return pump3;
    case 3: return pump4;
    default: return pump1;
  }
}

LightConfig& Settings::light(int i)
{
  switch (i)
  {
    case 0: return light1;
    case 1: return light2;
    default: return light1;
  }
}

const LightConfig& Settings::light(int i) const
{
  switch (i)
  {
    case 0: return light1;
    case 1: return light2;
    default: return light1;
  }
}

bool Settings::writeToFlashInternal(int sectorOffset)
{
  // Don't write to the flash if it's already what it needs to be
  if (memcmp(this, flashSettingsPtr(sectorOffset), sizeof(Settings)) == 0)
  {
    return false;
  }

  // Create a block of memory the size of the flash sector
  std::vector<uint8_t> buffer((sizeof(Settings)/FLASH_PAGE_SIZE + (sizeof(Settings)%FLASH_PAGE_SIZE > 0 ? 1 : 0)) * FLASH_PAGE_SIZE);\
  auto* data = buffer.data();
  auto size = buffer.size();
  memcpy(buffer.data(), this, sizeof(Settings));

  // Write to flash!
  multicore_lockout_start_blocking();
  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(settingsOffsetBytes(sectorOffset), FLASH_SECTOR_SIZE);
  flash_range_program(settingsOffsetBytes(sectorOffset), data, size);
  restore_interrupts(ints);
  multicore_lockout_end_blocking();

  return true; 
}

size_t Settings::structSize() const
{
  // Read the struct size from the settings and clamp it between a sane minimum value
  // and the size of the settings as of this code.
  return std::clamp(size, 136u, sizeof(Settings));
}

uint64_t Settings::calculateCrc() const
{
  // Get the data pointer to the first data after the CRC value
  // Exclude the CRC from calculating the CRC
  uint8_t* dataPtr = (uint8_t*)this + sizeof(uint64_t);
  uint64_t dataSize = structSize() - sizeof(uint64_t);

  return crc64(0, dataPtr, dataSize);
}

SettingsManager::SettingsManager()
{
  // Read the current settings
  std::cout << "Loading settings..." << std::endl << std::flush;
  if (!settings.readFromFlash())
  {
    std::cout << "No valid settings found, loading defaults..." << std::endl << std::flush;
    settings.setDefaults();
  }
  //settings.print();
  std::cout << "Load complete!" << std::endl << std::flush;

  // Validate the current settings
  std::cout << "Validating settings..." << std::endl << std::flush;
  if (!settings.validateAll())
  {
    std::cout << "Some settings were invalid and had to be reset." << std::endl<< std::flush;
  }
  //settings.print();
  std::cout << "Validation complete!" << std::endl << std::flush;

  // Set up a next write time for autosave
  nextWriteTime = get_absolute_time();
}

Settings& SettingsManager::getSettings()
{
  return settings;
}

bool SettingsManager::autosave()
{
  absolute_time_t now = get_absolute_time();
  bool wroteToFlash = false;
  if (to_ms_since_boot(now) > to_ms_since_boot(nextWriteTime))
  {
    wroteToFlash = settings.writeToFlash();
    nextWriteTime = make_timeout_time_ms(Minimum_Write_Interval_Ms);
  }
  return wroteToFlash;
}

// Some static asserts to ensure we haven't broken the settings object
static_assert(FLASH_SECTOR_SIZE >= sizeof(Settings), "Settings may not be larger than flash sector size!");
static_assert(std::is_standard_layout<Settings>::value, "Settings must have standard layout.");
static_assert(std::is_trivially_copyable<Settings>::value, "Settings must be trivially copyable.");
