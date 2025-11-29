#include <cpp/Button.hpp>
#include <cpp/Color.hpp>
#include <cpp/DiscreteOut.hpp>
#include <cpp/FlashStorage.hpp>
#include <cpp/WiFi.hpp>

#include "Animation.hpp"
#include "Settings.hpp"

#include <hardware/rtc.h>
#include <hardware/watchdog.h>
#include <pico/stdlib.h>
#include <pico/stdio.h>
#include <pico/util/datetime.h>
#include <pico/bootrom.h>
#include <pico/multicore.h>
#include <pico/unique_id.h>
#include <pico/cyw43_arch.h>
#include <pico/multicore.h>

#include <lwip/dns.h>
#include <lwip/pbuf.h>
#include <lwip/udp.h>

#include <iostream>
#include <istream>
#include <cmath>
#include <memory>
#include <sstream>
#include <cstring>
#include <string>
#include <time.h>

#define NTP_MSG_LEN 48
#define NTP_PORT 123
#define NTP_DELTA 2208988800 // seconds between 1 Jan 1900 and 1 Jan 1970

Animator animator(6, 8);
GPIOButton waterButton(0);
GPIOButton lightButton(1, true);

DiscreteOut pump1(2);
DiscreteOut pump2(3);
DiscreteOut pump3(4);
DiscreteOut pump4(5);
std::vector<DiscreteOut*> pumps {&pump1, &pump2, &pump3, &pump4};

DiscreteOut light1(7, false, true, true);
DiscreteOut light2(8, false, true, true);
std::vector<DiscreteOut*> lights {&light1, &light2};

struct RtcBootTimeSync
{
  static const uint64_t usPerDay = (24ull*60ull*60ull*1000ull*1000ull);
  uint64_t firstMidnightUs;
  
  RtcBootTimeSync()
  {
    datetime_t syncTimeRtc;
    rtc_get_datetime(&syncTimeRtc);
    firstMidnightUs = to_us_since_boot(get_absolute_time());

    uint64_t offsetUs = (((uint64_t)syncTimeRtc.hour * 60ull + (uint64_t)syncTimeRtc.min) * 60ull + (uint64_t)syncTimeRtc.sec) * 1000ull * 1000ull;
    
    while (offsetUs > firstMidnightUs)
    {
      firstMidnightUs += usPerDay;
    }
    firstMidnightUs -= offsetUs;
  }

  absolute_time_t absoluteTimeFromSecondsSinceMidnight(int32_t secondsSinceMidnight, absolute_time_t referenceTime = get_absolute_time())
  {
    uint64_t nowUs = to_us_since_boot(referenceTime);
    if (nowUs < firstMidnightUs)
    {
      int64_t startOfDayUs = (int64_t)firstMidnightUs - (int64_t)usPerDay;
      int64_t usSinceBoot = startOfDayUs + (int64_t)secondsSinceMidnight * 1000000ll;
      if (usSinceBoot < 0)
      {
        usSinceBoot = 0;
      }
      return from_us_since_boot(usSinceBoot);
    }
    else
    {
      uint64_t days = (nowUs - firstMidnightUs) / usPerDay;
      uint64_t startOfDayUs = firstMidnightUs + days * usPerDay;
      uint64_t usSinceBoot = startOfDayUs + (uint64_t)secondsSinceMidnight * 1000000ull;
      return from_us_since_boot(usSinceBoot);
    }
  }
};

struct NtpRequest
{
  std::string url;
  bool dnsOk = false;
  bool reqOk = false;
  bool respOk = false;
  bool error = false;
  ip_addr_t address;
  udp_pcb* ntp_pcb;
  time_t time;

  NtpRequest(const std::string& url) :
    url(url)
  {
    ntp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    error = !ntp_pcb;
  }

  ~NtpRequest()
  {
    if (ntp_pcb)
    {
      udp_remove(ntp_pcb);
      ntp_pcb = nullptr;
    }
  }
};

bool getSecondsSinceEpochNpt(time_t& epoch, uint32_t timeoutMs)
{
  bool success = false;
  absolute_time_t timeout_time = make_timeout_time_ms(timeoutMs);
  NtpRequest ntp("pool.ntp.org");

  // First, setup the UDP recieve callback. Might as well!
  udp_recv(ntp.ntp_pcb, [](void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
  {
    // Parse time out of response
    NtpRequest* ntp = (NtpRequest*)arg;
    uint8_t mode = pbuf_get_at(p, 0) & 0x7;
    uint8_t stratum = pbuf_get_at(p, 1);

    // Check the result
    if (ip_addr_cmp(addr, &ntp->address) && port == NTP_PORT && p->tot_len == NTP_MSG_LEN && mode == 0x4 && stratum != 0) 
    {
      uint8_t seconds_buf[4] = {0};
      pbuf_copy_partial(p, seconds_buf, sizeof(seconds_buf), 40);
      uint32_t seconds_since_1900 = seconds_buf[0] << 24 | seconds_buf[1] << 16 | seconds_buf[2] << 8 | seconds_buf[3];
      uint32_t seconds_since_1970 = seconds_since_1900 - NTP_DELTA;
      ntp->time = seconds_since_1970;
      ntp->respOk = true;
    } 
    else
    {
      std::cout << "Invalid NTP response" << std::endl << std::flush;
      ntp->error = true;
    }
    pbuf_free(p);
  }, &ntp);

  // Next, resolve the NTP server with DNS and chain the request on to that
  int dnsResult = dns_gethostbyname(ntp.url.c_str(), &ntp.address, [](const char* hostname, const ip_addr_t* ipaddr, void* arg)
  {
    NtpRequest* ntp = (NtpRequest*)arg;
    ntp->address = *ipaddr;
    ntp->dnsOk = true;
    std::cout << "DNS resolved!" << std::endl << std::flush;
  }, &ntp);

  if (dnsResult == ERR_OK)
  {
    std::cout << "DNS resolution skipped, value cached" << std::endl << std::flush;
    ntp.dnsOk = true;
  }
  else if (dnsResult == ERR_INPROGRESS)
  {
    std::cout << "DNS resolution in progress..." << std::endl << std::flush;
  }
  else // dnsResult == ERR_ARG
  {
    std::cout << "DNS resolution failed!" << std::endl << std::flush;
    ntp.error = true;
  }
  
  bool timedOut = true;
  while(absolute_time_diff_us(get_absolute_time(), timeout_time) > 0)
  {
    cyw43_arch_poll();
    if (ntp.respOk || ntp.error)
    {
      // If the response is ok or an error was encountered at any point,
      // then bail out of the work loop, we're done
      timedOut = false;
      break;
    } 
    else if (ntp.dnsOk && !ntp.reqOk)
    {
      std::cout << "Sending NTP request..." << std::endl << std::flush;
      // Send the time request
      pbuf* p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
      uint8_t* req = (uint8_t*) p->payload;
      memset(req, 0, NTP_MSG_LEN);
      req[0] = 0x1b;
      udp_sendto(ntp.ntp_pcb, p, &ntp.address, NTP_PORT);
      pbuf_free(p);
      ntp.reqOk = true;
    }
    cyw43_arch_wait_for_work_until(timeout_time);
  }

  if (ntp.respOk)
  {
    epoch = ntp.time;
    return true;
  }
  else if (timedOut)
  {
    std::cout << "Timed out!" << std::endl << std::flush;
    return false;
  }
  
  return false;
}

std::ostream& operator<<( std::ostream& os, const datetime_t& t )
{
  std::cout << (int)t.year << "/" << (int)t.month << "/" << (int)t.day << " " << (int)t.hour << ":" << (int)t.min << ":" << (int)t.sec;
  return os;
}

bool syncRtcWithNtp(Settings& settings, uint32_t timeoutMs = 10000)
{
  animator.playAnimation("wifi", -1);
  auto start = get_absolute_time();
  auto wifi = WiFiClient::Init(settings.wifiSsid, settings.wifiPassword, timeoutMs);
  
  if (!wifi.connected())
  {
    animator.playAnimation("alert", 3);
    animator.changeBaseAnimation("errorIdle");
    return false;
  }

  // Get the time
  time_t secondsSinceEpoch;
  if (!getSecondsSinceEpochNpt(secondsSinceEpoch, timeoutMs))
  {
    animator.playAnimation("alert", 3);
    animator.changeBaseAnimation("errorIdle");
    return false;
  }

  // Adjust to the local time
  secondsSinceEpoch += (settings.offsetFromUtc * 60 * 60);
  
  // Convert to an RTC datetime struct
  tm* local = gmtime(&secondsSinceEpoch);
  datetime_t dt
  {
    (int16_t) (local->tm_year + 1900), // int16_t year;    ///< 0..4095
    (int8_t) (local->tm_mon + 1),      // int8_t month;    ///< 1..12, 1 is January
    (int8_t) (local->tm_mday),         // int8_t day;      ///< 1..28,29,30,31 depending on month
    (int8_t) (local->tm_wday),         // int8_t dotw;     ///< 0..6, 0 is Sunday
    (int8_t) (local->tm_hour),         // int8_t hour;     ///< 0..23
    (int8_t) (local->tm_min),          // int8_t min;      ///< 0..59
    (int8_t) (local->tm_sec),          // int8_t sec;      ///< 0..59
  };

  std::cout << "Setting RTC to " << dt << std::endl << std::flush;

  // Finally push the struct into the RTC hardware
  rtc_set_datetime(&dt);

  // Tell the user sync was successful
  animator.playAnimation("ok", 3);
  animator.changeBaseAnimation("idle");
  animator.waitForAnimationComplete(1200);

  return true;
}

void rebootIntoProgMode()
{
  animator.changeBaseAnimation("blank");
  animator.playAnimation("alert", 3);
  animator.waitForAnimationComplete(1200);
  multicore_reset_core1();
  reset_usb_boot(0,0);
}

template <typename T>
bool setValFromStream(T& val, T min, T max, std::istream& s)
{
  T input;
  s >> input;
  if (s.fail())
  {
    std::cout << "parse error" << std::endl << std::flush;
    return false;
  }
  if (input < min || input > max)
  {
    std::cout << "value out of range error" << std::endl << std::flush;
    return false;
  }
  val = input;
  return true;
}

bool setValFromStream(bool& val, std::istream& s)
{
  int input;
  s >> input;
  if (s.fail())
  {
    std::cout << "parse error" << std::endl << std::flush;
    return false;
  }
  if (input < 0 || input > 1)
  {
    std::cout << "value out of range error" << std::endl << std::flush;
    return false;
  }
  val = (input == 1);
  return true;
}

bool setValFromStream(char* val, size_t len, std::istream& ss)
{
  // Eat any extra whitespace in the stream
  ss >> std::ws;
  // Get the whole rest of the line as a string
  std::string str;
  std::getline(ss, str);

  // Check if the read was clean and if the string will fit
  if (ss.fail())
  {
    std::cout << "parse error" << std::endl << std::flush;
    return false;
  }
  if (str.size() >= len)
  {
    std::cout << "string param too long" << std::endl << std::flush;
    return false;
  }
  // Copy the string to the dest buffer will null terminator
  memcpy(val, str.data(), str.size());
  val[str.size()] = '\0';
  return true;
}

void processCommand(std::string cmdAndArgs, FlashStorage<Settings>& settingsMgr)
{
  Settings& settings = settingsMgr.data;
  std::stringstream ss(cmdAndArgs);
  std::string cmd;
  ss >> cmd;
  
  if (cmd == "wifiSsid")
  {
    setValFromStream(settings.wifiSsid, 256ul, ss);
  }
  else if (cmd == "wifiPassword")
  {
    setValFromStream(settings.wifiPassword, 256ul, ss);
  }
  else if (cmd == "offsetFromUtc")
  {
    setValFromStream(settings.offsetFromUtc, -24.0f, 24.0f , ss);
  }
  else if (cmd == "pump")
  {
    int id;
    if (!setValFromStream(id, 1, 4, ss)) return;
    std::string prop;
    ss >> prop;

    if (prop == "enable")
    {
      setValFromStream(settings.pump(id-1).enable, ss);
    }
    else if (prop == "rate")
    {
      setValFromStream(settings.pump(id-1).rate, 0.0f, 1000.0f, ss);
    }
    else if (prop == "amount")
    {
      setValFromStream(settings.pump(id-1).amount, 0.0f, 1000.0f, ss);
    }
    else if (prop == "activationTime")
    {
      setValFromStream(settings.pump(id-1).activationTime, 0l, (int32_t)24 * 60 * 60, ss);
    }
    else
    {
      std::cout << "unknown property error" << std::endl << std::flush;
    }
  }
  else if (cmd == "light")
  {
    int id;
    if (!setValFromStream(id, 1, 2, ss)) return;
    std::string prop;
    ss >> prop;

    if (prop == "enable")
    {
      setValFromStream(settings.light(id-1).enable, ss);
    }
    else if (prop == "onTime")
    {
      setValFromStream(settings.light(id-1).onTime, 0l, (int32_t)24 * 60 * 60, ss);
    }
    else if (prop == "offTime")
    {
      setValFromStream(settings.light(id-1).offTime, 0l, (int32_t)24 * 60 * 60, ss);
    }
    else
    {
      std::cout << "unknown property error" << std::endl << std::flush;
    }
  }
  else if (cmd == "force")
  {
    std::string prop;
    ss >> prop;

    if (prop == "pump")
    {
      int id;
      if (!setValFromStream(id, 1, 4, ss)) return;
      bool val;
      if (!setValFromStream(val, ss)) return;
      // Force relevant I/O value
      pumps[id-1]->set(val);
    }
    else if (prop == "light")
    {
      int id;
      if (!setValFromStream(id, 1, 2, ss)) return;
      bool val;
      if (!setValFromStream(val, ss)) return;
      // Force relevant I/O value
      lights[id-1]->set(val);
    }
    else
    {
      std::cout << "unknown property error" << std::endl << std::flush;
    }
  }
  else if (cmd == "defaults")
  {
    settings.setDefaults();
  }
  else if (cmd == "flash")
  {
    // Write the settings to flash
    if (settingsMgr.writeToFlash())
      std::cout << "Wrote settings to flash!" << std::endl << std::flush;
    else
      std::cout << "Skipped writing to flash because contents were already correct." << std::endl << std::flush;
  }
  else if (cmd == "info" || cmd == "about")
  {
    std::cout << "silvanus-pico by Donkey Kong" << std::endl;
    std::cout << "https://github.com/DonkeyKong/silvanus-pico" << std::endl;
    std::cout << std::endl;
    settings.print();
    std::cout << std::endl;
    std::cout << "-- Runtime Data --" << std::endl;
    std::cout << "full settings size: " << sizeof(Settings) << std::endl;
    std::cout << std::flush;
  }
  else if (cmd == "reboot")
  {
    // Reboot the system immediately
    std::cout << "ok" << std::endl << std::flush;
    watchdog_reboot(0,0,0);
  }
  else if (cmd == "prog")
  {
    // Reboot into programming mode
    std::cout << "ok" << std::endl << std::flush;
    rebootIntoProgMode();
  }
  else if (cmd == "anim")
  {
    std::string subcmd;
    ss >> subcmd;

    if (subcmd == "play")
    {
      std::string name = "idle";
      int loops = 1;
      ss >> name >> loops;
      animator.playAnimation(name, loops);
    }
    else if (subcmd == "base")
    {
      std::string name = "idle";
      ss >> name;
      animator.changeBaseAnimation(name);
    }
    else if (subcmd == "stop")
    {
      animator.stopAnimation();
    }
    else if (subcmd == "param")
    {
      float t;
      ss >> t;
      animator.parameter(t);
    }
  }
  else if (cmd == "synctime")
  {
    if (!syncRtcWithNtp(settings))
    {
      std::cout << "Error fetching time with NTP!" << std::endl << std::flush;
      return;
    }
  }
  else if (cmd == "time")
  {
    datetime_t time;
    if (rtc_get_datetime(&time))
    {
      std::cout << time << std::endl << std::flush;
    }
    else
    {
      std::cout << "Error: realtime clock is not running! Call synctime at least once." << std::endl << std::flush;
      return;
    }
  }
  else
  {
    std::cout << "unknown command error" << std::endl << std::flush;
    return;
  }

  if (!ss.fail())
  {
    std::cout << "ok" << std::endl << std::flush;
  }
}

void processStdIo(FlashStorage<Settings>& settingsMgr)
{
  static char inBuf[1024];
  static int pos = 0;

  while (true)
  {
    int inchar = getchar_timeout_us(0);
    if (inchar > 31 && inchar < 127 && pos < 1023)
    {
      inBuf[pos++] = (char)inchar;
      std::cout << (char)inchar << std::flush; // echo to client
    }
    else if (inchar == '\n')
    {
      inBuf[pos] = '\0';
      std::cout << std::endl << std::flush; // echo to client
      processCommand(inBuf, settingsMgr);
      pos = 0;
    }
    else
    {
      return;
    }
  }
}

void processStdIoFor(FlashStorage<Settings>& settingsMgr, uint32_t milliseconds)
{
  absolute_time_t exitTime = make_timeout_time_ms(milliseconds);
  while (to_ms_since_boot(exitTime) > to_ms_since_boot(get_absolute_time()))
  {
    processStdIo(settingsMgr);
    sleep_ms(50);
  }
}

bool withinRange(absolute_time_t val, absolute_time_t minEx, absolute_time_t maxInc)
{
  return to_us_since_boot(val) > to_us_since_boot(minEx) && to_us_since_boot(val) <= to_us_since_boot(maxInc);
}

int32_t getRtcSecondsSinceMidnight()
{
  datetime_t t;
  if (rtc_get_datetime(&t))
  {
    return (uint32_t)t.hour * 60ul * 60ul + (uint32_t)t.min * 60ul + (uint32_t)t.sec;
  }
  return -1;
}

void autoLights(const Settings& settings)
{
  int32_t now = getRtcSecondsSinceMidnight();

  for (int i = 0; i < lights.size(); ++i)
  {
    int32_t onTime = settings.light(i).onTime;
    int32_t offTime = settings.light(i).offTime;
    if (settings.light(i).enable)
    {
      if (onTime < offTime)
      {
        lights[i]->set(now < offTime && now >= onTime);
      }
      else
      {
        lights[i]->set(now < offTime || now >= onTime);
      }
    }
    else
    {
      lights[i]->set(false);
    }
  }
}

int main()
{
  // Configure stdio
  stdio_init_all();
  rtc_init();

  // Wait 1 second for remote terminals to connect
  // before doing anything.
  sleep_ms(1000);

  // Init the settings object
  FlashStorage<Settings> settingsMgr;
  Settings& settings = settingsMgr.data;

  // Read the current settings
  std::cout << "Loading settings..." << std::endl << std::flush;
  if (!settingsMgr.readFromFlash())
  {
    std::cout << "No valid settings found, loading defaults..." << std::endl << std::flush;
    settings.setDefaults();
  }
  std::cout << "Load complete!" << std::endl << std::flush;

  // Validate the current settings
  std::cout << "Validating settings..." << std::endl << std::flush;
  if (!settings.validateAll())
  {
    std::cout << "Some settings were invalid and had to be reset." << std::endl<< std::flush;
  }
  std::cout << "Validation complete!" << std::endl << std::flush;

  // Setup the animation system
  animator.addAnimation("idle", std::make_unique<SolidAnimation>(HSVColor{147.0f, 0.8f, 0.15f}.toRGB()));
  animator.addAnimation("errorIdle", std::make_unique<PulseAnimation>(HSVColor{0.0f, 0.8f, 1.0f}.toRGB()));
  animator.addAnimation("blank", std::make_unique<BlankAnimation>());
  animator.addAnimation("wifi", std::make_unique<WiFiConnectAnimation>());
  animator.addAnimation("alert", std::make_unique<FlashAnimation>(RGBColor{128, 0, 0}));
  animator.addAnimation("ok", std::make_unique<FlashAnimation>(HSVColor{200.0f, 0.7f, 0.5f}.toRGB()));
  animator.addAnimation("water-progress", std::make_unique<ProgressAnimation>(RGBColor{0, 0, 255}));
  animator.startUpdateThread();

  lightButton.holdActivationRepeatMs(-1);
  
  // Try like crazy to establish a wifi connection and sync RTC
  bool rtcOk = false;
  uint32_t reconnectTries = 0;
  uint32_t wifiTimeout = 10000;
  while (!rtcOk)
  {
    rtcOk = syncRtcWithNtp(settings, wifiTimeout);
    if (!rtcOk)
    {
      if (reconnectTries < 5)
      {
        processStdIoFor(settingsMgr, 5000);
      }
      else if (reconnectTries < 15)
      {
        wifiTimeout = 15000;
        processStdIoFor(settingsMgr, 15000);
      }
      else
      {
        wifiTimeout = 30000;
        processStdIoFor(settingsMgr, 60000);
      }
      reconnectTries++;
    }
  }

  // With the RTC set, create a type sync object which will allow
  // us to feed in a "secondsFromMidnight" values and get out an
  // absolute_time_t corresponding to that value for today.

  RtcBootTimeSync timeSync;
  absolute_time_t evalTime = get_absolute_time();
  absolute_time_t lastEvalTime = evalTime;
  absolute_time_t nextFrameTime = evalTime;

  std::vector<absolute_time_t> pumpOnTimes(pumps.size());
  std::vector<absolute_time_t> pumpOffTimes(pumps.size());

  bool autoLightsDone = false;
  for (auto& time : pumpOffTimes)
  {
    time = from_us_since_boot(0);
  }

  while (1)
  {
    // Regulate loop speed
    sleep_until(nextFrameTime);
    nextFrameTime = make_timeout_time_ms(50);

    // Take a reading of the system time
    lastEvalTime = evalTime;
    evalTime = get_absolute_time();

    // Watering cycle detection
    bool wateringCycleRunning = false;
    float waterCycleProgress = 1.0f;
    for (int i = 0; i < pumps.size(); ++i)
    {
      if (to_ms_since_boot(evalTime) < to_ms_since_boot(pumpOffTimes[i]) && 
          to_ms_since_boot(pumpOffTimes[i]) > to_ms_since_boot(pumpOnTimes[i]) &&
          settingsMgr.data.pump(i).enable)
      {
        wateringCycleRunning = true;
        float progress = (float)(to_ms_since_boot(evalTime)-to_ms_since_boot(pumpOnTimes[i])) / 
                         (float)(to_ms_since_boot(pumpOffTimes[i]) - to_ms_since_boot(pumpOnTimes[i]));
        if (progress < waterCycleProgress)
        {
          waterCycleProgress = progress;
        }
      }
    }
    animator.parameter("water-progress", waterCycleProgress);
    if (wateringCycleRunning)
    {
      animator.playAnimation("water-progress");
    }
    
    // Determine if between last frame and this frame, a light should have turned on
    // or off
    if (!autoLightsDone)
    {
      autoLights(settings);
      autoLightsDone = true;
    }
    else
    {
      for (int i = 0; i < lights.size(); ++i)
      {
        if (settings.light(i).enable)
        {
          auto onTime = timeSync.absoluteTimeFromSecondsSinceMidnight(settings.light(i).onTime, evalTime);
          auto offTime = timeSync.absoluteTimeFromSecondsSinceMidnight(settings.light(i).offTime, evalTime);
          if (withinRange(onTime, lastEvalTime, evalTime))
          {
            lights[i]->set(true);
          }
          else if (withinRange(offTime, lastEvalTime, evalTime))
          {
            lights[i]->set(false);
          }
        }
      }
    }

    // Determine if between last frame and this frame, a watering event should have been triggered
    for (int i = 0; i < pumps.size(); ++i)
    {
      if (settings.pump(i).enable)
      {
        auto onTime = timeSync.absoluteTimeFromSecondsSinceMidnight(settings.pump(i).activationTime, evalTime);
        if (withinRange(onTime, lastEvalTime, evalTime))
        {
          uint64_t usPumpOnTime = (settings.pump(i).amount / settings.pump(i).rate * 1000000.0f);
          pumpOffTimes[i] = from_us_since_boot(to_us_since_boot(onTime) + usPumpOnTime);
          pumpOnTimes[i] = onTime;
        }
      }
      pumps[i]->set(to_ms_since_boot(evalTime) < to_ms_since_boot(pumpOffTimes[i]));
    }

    // Process input
    processStdIo(settingsMgr);

    waterButton.update();
    if (waterButton.buttonUp())
    {
      if (wateringCycleRunning)
      {
        // Cancel the watering cycle
        for (int i = 0; i < pumps.size(); ++i)
        {
          pumpOffTimes[i] = from_us_since_boot(0);
          pumpOnTimes[i] = from_us_since_boot(0);
          pumps[i]->set(false);
        }
      }
      else
      {
        // Start a watering cycle
        for (int i = 0; i < pumps.size(); ++i)
        {
          if (settings.pump(i).enable)
          {
            uint64_t usPumpOnTime = (settings.pump(i).amount / settings.pump(i).rate * 1000000.0f);
            pumpOffTimes[i] = from_us_since_boot(to_us_since_boot(evalTime) + usPumpOnTime);
            pumpOnTimes[i] = evalTime;
          }
          pumps[i]->set(to_ms_since_boot(evalTime) < to_ms_since_boot(pumpOffTimes[i]));
        }
      }
    }

    lightButton.update();
    if (lightButton.heldActivate())
    {
      std::cout << "Button held, set lights to auto state" << std::endl << std::flush;
      autoLights(settings);
    }
    
    if (lightButton.buttonUp())
    {
      // Toggle the lights
      bool lightState = false;
      for (auto& light : lights)
      {
        lightState = lightState || light->get();
      }
      std::cout << "Button tapped, set lights " << (lightState ? "off" : "on") << std::endl << std::flush;
      for (auto& light : lights)
      {
        light->set(!lightState);
      }
    }
  }
  return 0;
}
