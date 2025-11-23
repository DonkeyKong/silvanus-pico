#include "Settings.hpp"

#include <vector>
#include <cstring>
#include <iostream>
#include <algorithm>

namespace
{
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
}

void Settings::setDefaults()
{
  memcpy(wifiSsid, "wifi", 5);
  memcpy(wifiPassword, "password", 9);
  offsetFromUtc = -5.0f; // EST in the US
  pump1 = { true, 1.3f, 80.0f, 8 * 60 * 60 };
  pump2 = { false, 1.3f, 80.0f, 8 * 60 * 60 };
  pump3 = { false, 1.3f, 80.0f, 8 * 60 * 60 };
  pump4 = { false, 1.3f, 80.0f, 8 * 60 * 60 };
  light1 = { true, 8 * 60 * 60, (8 + 12) * 60 * 60};
  light2 = { false, 8 * 60 * 60, (8 + 12) * 60 * 60};
}

bool Settings::validateAll()
{
  // Validate the settings to make sure they are ok after load
  bool failedValidation = false;
  // tbd: actually validate stuff
  return !failedValidation;
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
  std::cout << "-- Silvanus Pico v1.1 --" << std::endl;
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
