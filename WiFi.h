#pragma once

#include <string>
#include <iostream>

#include <pico/cyw43_arch.h>

class WiFiClient
{
public:
  WiFiClient(std::string ssid, std::string wpa2Psk) :
    ssid_(ssid), wpa2Psk_(wpa2Psk) {  }
  
  WiFiClient(const WiFiClient&) = delete;

  WiFiClient(WiFiClient&& other)
  {
    shutdown();
    ssid_ = std::move(other.ssid_);
    wpa2Psk_ = std::move(other.wpa2Psk_);
    inited_ = other.inited_;
    connected_ = other.connected_;
    other.inited_ = false;
  }

  ~WiFiClient()
  {
    shutdown();
  }

  bool init(uint32_t timeoutMs = 10000)
  {
    if (cyw43_arch_init()) 
    {
        std::cout << "WiFi failed to init!" << std::endl << std::flush;
        return false;
    }

    inited_ = true;

    cyw43_arch_enable_sta_mode();

    if (cyw43_arch_wifi_connect_timeout_ms(ssid_.c_str(), wpa2Psk_.c_str(), CYW43_AUTH_WPA2_AES_PSK, timeoutMs))
    {
        std::cout << "WiFi failed to connect!" << std::endl << std::flush;
        return false;
    }

    connected_ = true;
    
    return true;
  }

  void shutdown()
  {
    if (inited_)
    {
      cyw43_arch_deinit();
      inited_ = false;
    }
  }

  bool connected()
  {
    return connected_;
  }

  static WiFiClient Init(std::string authName, std::string authPass, uint32_t timeoutMs = 10000)
  {
    WiFiClient client(authName, authPass);
    client.init(timeoutMs);
    return client;
  }

private:
  std::string ssid_;
  std::string wpa2Psk_;
  bool inited_ = false;
  bool connected_ = false;
};