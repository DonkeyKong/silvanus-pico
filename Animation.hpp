#pragma once

#include "PioProgram.hpp"

#include <map>
#include <pico/multicore.h>
#include <pico/lock_core.h>

template <typename T>
class ScopedLock
{
public:
  ScopedLock(T* mtx) : mtx_(mtx)
  {
    mutex_enter_blocking(mtx_);
  }
  ~ScopedLock()
  {
    mutex_exit(mtx_);
  }
private:
  T* mtx_;
};

enum class AnimationState
{
  Stopped,
  Starting,
  Playing,
};

class Animation
{
public:
  void play(int loops = 1)
  {
    state_ = AnimationState::Starting;
    loops_ = loops;
    playStart_ = get_absolute_time();
    lastUpdate_ = playStart_;
  }
  void stop()
  {
    state_ = AnimationState::Stopped;
  }
  void update(LEDBuffer& buffer)
  {
    auto t = get_absolute_time();
    updateInternal(buffer, (float)absolute_time_diff_us(lastUpdate_, t) / 1000000.0f );
    if (state_ == AnimationState::Starting)
    {
      state_ = AnimationState::Playing;
    }
    if (loops_ == 0)
    {
      state_ = AnimationState::Stopped;
    }
    lastUpdate_ = t;
  }
  void parameter(float t)
  {
    t_ = t;
  }
  AnimationState state()
  {
    return state_;
  }
protected:
  virtual void updateInternal(LEDBuffer& buffer, float deltaT) = 0;
  float t_;
  int loops_;
  absolute_time_t playStart_;
  absolute_time_t lastUpdate_;
  AnimationState state_;
};

class BlankAnimation : public Animation
{
  virtual void updateInternal(LEDBuffer& buffer, float deltaT) override
  {
    for (int i=0; i < buffer.size(); ++i)
    {
      buffer[i] = {};
    }
  }
};

class SolidAnimation : public Animation
{
public:
  SolidAnimation(RGBColor color) : color_(color) {}
protected:
  virtual void updateInternal(LEDBuffer& buffer, float deltaT) override
  {
    for (int i=0; i < buffer.size(); ++i)
    {
      buffer[i] = color_;
    }
  }
  RGBColor color_;
};

class FlashAnimation : public Animation
{
public:
  FlashAnimation(RGBColor flashColor) : flashColor_(flashColor) {}
protected:
  virtual void updateInternal(LEDBuffer& buffer, float deltaT) override
  {
    if (state_ == AnimationState::Starting)
    {
      t_ = 0.0f;
    }

    // Animate
    t_ += deltaT / flashPeriodSecs_;

    float loopLength = loops_ > 1 ? 1.0f : 2.0f;
    while (t_ > loopLength)
    {
      t_ -= 1.0f;
      if (loops_ > 0)
      {
        --loops_;
      }
    }

    // Draw
    RGBColor color;
    if (t_ < flashDutyCycle_) color = flashColor_;
    for (int i=0; i < buffer.size(); ++i)
    {
      buffer[i] = color;
    }
  }
  RGBColor flashColor_;
  float flashDutyCycle_ = 0.666f;
  float flashPeriodSecs_ = 0.3f;
};

class WaveAnimation : public Animation
{
  virtual void updateInternal(LEDBuffer& buffer, float deltaT) override
  {
    if (state_ == AnimationState::Starting)
    {
      t_ = 0.0f;
    }

    // Animate
    t_ += deltaT / 16.0f;

    while (t_ > 1.0f)
    {
      t_ -= 1.0f;
      if (loops_ > 0)
      {
        --loops_;
      }
    }

    // Draw
    float mean = t_ * 16.0f - 4.0f;
    for (int i=0; i < buffer.size(); ++i)
    {
      float v = 0.4f * expf(-0.5 * powf((((float)i - mean)/2.0f), 2.0f));
      buffer[i] = HSVColor{147.0f, 0.8f, v}.toRGB();
    }
  }
};

class PulseAnimation : public Animation
{
public:
  PulseAnimation(RGBColor color) : color_(color) {}
protected:
  virtual void updateInternal(LEDBuffer& buffer, float deltaT) override
  {
    if (state_ == AnimationState::Starting)
    {
      t_ = 0.0f;
    }

    // Animate
    t_ += deltaT / 16.0f;

    while (t_ > 1.0f)
    {
      t_ -= 1.0f;
      if (loops_ > 0)
      {
        --loops_;
      }
    }

    // Draw
    float v;
    if (t_ < 0.3f)
    {
      v = t_ / 0.3f;
    }
    else if (t_ >= 0.3f && t_ < 0.7)
    {
      v = 1.0f;
    }
    else
    {
      v = 1.0f - (t_ - 0.7f) / 0.3f;
    }
    float brightness = v * 0.333f + 0.1f;
    RGBColor color = color_ * brightness;
    for (int i=0; i < buffer.size(); ++i)
    {
      buffer[i] = color;
    }
  }
  RGBColor color_;
};

class WiFiConnectAnimation : public Animation
{
  virtual void updateInternal(LEDBuffer& buffer, float deltaT) override
  {
    if (state_ == AnimationState::Starting)
    {
      t_ = 0.0f;
    }

    // Animate
    t_ += deltaT / 2.0f;

    while (t_ > 1.0f)
    {
      t_ -= 1.0f;
      if (loops_ > 0)
      {
        --loops_;
      }
    }

    // Draw
    float loc;
    if (t_ < 0.5f)
    {
      loc = t_ * 14.0f;
    }
    else
    {
      loc = 14.0f - (t_ * 14.0f);
    }

    for (int i=0; i < buffer.size(); ++i)
    {
      float v = std::clamp(1.0f - std::abs(loc - i), 0.0f, 1.0f);
      buffer[i] = HSVColor{200.0f, 0.7f, 0.5f * v}.toRGB();
    }
  }
};

class ProgressAnimation : public Animation
{
public:
  ProgressAnimation(RGBColor color) : color_(color) {}
protected:
  virtual void updateInternal(LEDBuffer& buffer, float deltaT) override
  {
    // End when progress reaches 100%
    if (t_ >= 1.0f)
    {
      loops_ = 0;
    }

    // Draw
    for (int i=0; i < buffer.size(); ++i)
    {
      float dist = t_ * 7.0f - i;
      float v = std::clamp(dist + 1.0f, 0.25f, 1.0f);
      buffer[i] = color_ * v;
    }
  }
  RGBColor color_;
};

// Starts a process on core 1 to animate the LEDs
class Animator
{
private:
  friend void updateThread();
  static constexpr uint64_t TargetFPS = 30;
  static constexpr uint64_t TargetFrameTimeUs = 1000000 / TargetFPS;
  static constexpr float TargetFrameTimeSec = 1.0f / (float)TargetFPS;
  static Animator* ptr;
  Ws2812bOutput leds_;
  absolute_time_t nextFrameTime_;
  std::string baseAnimation_;
  std::string overlayAnimation_;
  std::map<std::string, std::unique_ptr<Animation>> animations_;
  BlankAnimation blank_;
  mutex_t mtx_;

  Animation* currentAnim()
  {
    if (!overlayAnimation_.empty())
    {
      return animations_[overlayAnimation_].get();
    }
    else if (!baseAnimation_.empty())
    {
      return animations_[baseAnimation_].get();
    }
    return &blank_;
  }

  static void updateThread()
  {
    multicore_lockout_victim_init();
    while (1)
    {
      Animator::ptr->update();
    }
  }
public:
  Animator(uint pin, uint numLeds) :
    leds_{ Ws2812bOutput::create(pin, numLeds) },
    nextFrameTime_ { get_absolute_time() }
  {
    mutex_init(&mtx_);
  }

  ~Animator()
  {
    ptr = nullptr;
    multicore_reset_core1();
  }

  void startUpdateThread()
  {
    ptr = this;
    //multicore_reset_core1();
    multicore_lockout_victim_init();
    multicore_launch_core1(updateThread);
  }

  void addAnimation(const std::string name, std::unique_ptr<Animation> anim)
  {
    ScopedLock lock(&mtx_);
    animations_[name] = std::move(anim);
  }

  bool changeBaseAnimation(std::string name)
  {
    ScopedLock lock(&mtx_);
    if (animations_.count(name) > 0)
    {
      baseAnimation_ = name;
      animations_[name]->play(-1);
      return true;
    }
    return false;
  }

  bool playAnimation(std::string name, int loops = 1)
  {
    ScopedLock lock(&mtx_);
    if (animations_.count(name) > 0)
    {
      overlayAnimation_ = name;
      animations_[name]->play(loops);
      return true;
    }
    return false;
  }

  void stopAnimation()
  {
    // lock
    if (!overlayAnimation_.empty())
    {
      animations_[overlayAnimation_]->stop();
    }
  }

  void parameter(float t)
  {
    ScopedLock lock(&mtx_);
    currentAnim()->parameter(t);
  }

  void parameter(std::string animation, float t)
  {
    ScopedLock lock(&mtx_);
    if (animations_.count(animation) > 0)
    {
      animations_[animation]->parameter(t);
    }
  }

  bool waitForAnimationComplete(int timeoutMs = -1)
  {
    absolute_time_t startTime = get_absolute_time();
    while (1)
    {
      {
        ScopedLock lock(&mtx_);
        if (overlayAnimation_.empty())
        {
          return true;
        }
      }
      if (timeoutMs >= 0 && to_ms_since_boot(get_absolute_time()) - to_ms_since_boot(startTime) > timeoutMs)
      {
        return false;
      }
      sleep_ms(10);
    }
  }
  void update()
  {
    // Wait
    sleep_until(nextFrameTime_);
    nextFrameTime_ = make_timeout_time_us(TargetFrameTimeUs);

    // If the overlay animation is stopped, clear it out
    {
      ScopedLock lock(&mtx_);
      if (!overlayAnimation_.empty() && animations_[overlayAnimation_]->state() == AnimationState::Stopped)
      {
        overlayAnimation_.clear();
      }
      currentAnim()->update(leds_.buffer());
    }
    leds_.update();
  }
  Ws2812bOutput& leds()
  {
    return leds_;
  }
};

Animator* Animator::ptr;
