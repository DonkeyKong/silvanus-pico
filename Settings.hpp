#pragma once

#include <stdint.h>

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
  bool reserved; // not used, only here for backwards compatibility
  char wifiSsid[256];
  char wifiPassword[256]; // only wpa2-psk auth supported
  float offsetFromUtc; // in hours
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

  // Print all the settings to cout
  void print();
};
