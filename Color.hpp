#pragma once

#include <stdint.h>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <cmath>

struct RGBColor;
struct HSVColor;
struct LabColor;

using LEDBuffer = std::vector<RGBColor>;

#pragma pack(push, 1)

struct Vec3f
{
  float X = 0.0f;
  float Y = 0.0f;
  float Z = 0.0f;

  bool operator==(const Vec3f& other)
  {
    return X == other.X && Y == other.Y && Z == other.Z;
  }

  bool operator!=(const Vec3f& other)
  {
    return X != other.X || Y != other.Y || Z != other.Z;
  }

  Vec3f operator* (float c) const
  {
    return {c*X, c*Y, c*Z};
  }
};

struct HSVColor
{
  float H = 0.0f;
  float S = 0.0f;
  float V = 0.0f;

  RGBColor toRGB();
};

struct RGBColor
{
  uint8_t R = 0;
  uint8_t G = 0;
  uint8_t B = 0;

  RGBColor operator* (float c) const
  {
    return {(uint8_t)(c*R), (uint8_t)(c*G), (uint8_t)(c*B)};
  }

  RGBColor operator* (const Vec3f& c) const
  {
    return {(uint8_t)std::clamp(c.X*R, 0.0f, 255.0f), (uint8_t)std::clamp(c.Y*G, 0.0f, 255.0f), (uint8_t)std::clamp(c.Z*B, 0.0f, 255.0f)};
  }

  void applyGamma(float gamma)
  {
    R = (uint8_t)std::clamp(std::pow((float)R / 255.0f, gamma) * 255.0f, 0.0f, 255.0f);
    G = (uint8_t)std::clamp(std::pow((float)G / 255.0f, gamma) * 255.0f, 0.0f, 255.0f);
    B = (uint8_t)std::clamp(std::pow((float)B / 255.0f, gamma) * 255.0f, 0.0f, 255.0f);
  }

  static RGBColor blend(const RGBColor& a, const RGBColor& b, float t = 0.5f)
  {
    float invT = (1.0f - t);
    float R = (float)a.R * invT + (float) b.R * t;
    float G = (float)a.G * invT + (float) b.G * t;
    float B = (float)a.B * invT + (float) b.B * t;
    return { (uint8_t)R, (uint8_t)G, (uint8_t)B };
  }

  HSVColor toHSV() const;
  LabColor toLab() const;
  uint8_t getBrightestChannel() const;
  uint8_t getDarkestChannel() const;
  uint8_t getGrayValue() const;
};

struct XYZColor
{
  float X = 0;
  float Y = 0;
  float Z = 0;
};

struct LabColor
{
  float L = 0;
  float a = 0;
  float b = 0;

  RGBColor toRGB() const;

  LabColor operator+ (const LabColor& c)  const
  {
    return {L + c.L, a + c.a, b + c.b};
  }

  LabColor operator* (const LabColor& c)  const
  {
    return {L * c.L, a * c.a, b * c.b};
  }

  void operator+= (const LabColor& c)
  {
    L += c.L;
    a += c.a;
    b += c.b;
  }

  LabColor operator- (const LabColor& c) const
  {
    return {L - c.L, a - c.a, b - c.b};
  }

  LabColor operator* (float c) const
  {
    return {c*L, c*a, c*b};
  }

  friend LabColor operator*(float c, const LabColor& rhs)
  {
      return {c*rhs.L, c*rhs.a, c*rhs.b};
  }

  float deltaE(const LabColor& other) const;
};
#pragma pack(pop)

static_assert(std::is_standard_layout<Vec3f>::value, "Vec3f must have standard layout.");
static_assert(std::is_trivially_copyable<Vec3f>::value, "Vec3f must be trivially copyable.");

static_assert(std::is_standard_layout<HSVColor>::value, "HSVColor must have standard layout.");
static_assert(std::is_trivially_copyable<HSVColor>::value, "HSVColor must be trivially copyable.");

static_assert(std::is_standard_layout<RGBColor>::value, "RGBColor must have standard layout.");
static_assert(std::is_trivially_copyable<RGBColor>::value, "RGBColor must be trivially copyable.");

static_assert(std::is_standard_layout<XYZColor>::value, "XYZColor must have standard layout.");
static_assert(std::is_trivially_copyable<XYZColor>::value, "XYZColor must be trivially copyable.");

static_assert(std::is_standard_layout<LabColor>::value, "LabColor must have standard layout.");
static_assert(std::is_trivially_copyable<LabColor>::value, "LabColor must be trivially copyable.");

// Get an RGBColor corresponding to a color teperature in kelvin.
// Works for all float values but returns colors are clamped 
// between 1000k and 12000k
RGBColor GetColorFromTemperature(float tempK);