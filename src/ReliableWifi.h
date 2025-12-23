#ifndef RELIABLE_WIFI_H
#define RELIABLE_WIFI_H

#include <WiFi.h>

#define MAX_SSID_LEN 32
#define MAX_PASSWORD_LEN 64
#define MAX_NETWORKS 10

struct WifiCredentials {
  char ssid[MAX_SSID_LEN + 1];
  char password[MAX_PASSWORD_LEN + 1];
};

class ReliableWiFi {
public:
  // Constructor
  ReliableWiFi(uint8_t ledPin = LED_BUILTIN);
  
  // Add a network to the list
  bool addNetwork(const char* ssid, const char* password);
  
  // Initialize and connect to the strongest available network
  bool begin();
  
  // Check connection and reconnect if necessary
  void maintain();
  
  // Force a reconnection (useful for switching networks)
  bool reconnect();
  
  // Check if WiFi is connected
  bool isConnected();
  
  // Get current SSID
  String getCurrentSSID();
  
  // Configuration
  void setConnectTimeout(uint32_t timeout) { connectTimeout = timeout; }
  void setReconnectBackoff(uint32_t backoff) { reconnectBackoff = backoff; }
  void setRefreshInterval(uint32_t interval) { refreshInterval = interval; }
  void setInternetCheckEnabled(bool enabled) { checkInternet = enabled; }
  void setInternetCheckHost(const char* host) { internetCheckHost = host; }
  void setInternetCheckPort(uint16_t port) { internetCheckPort = port; }
  void setInternetCheckTimeout(uint32_t timeout) { internetCheckTimeout = timeout; }
  void setAggressiveScan(bool aggressive) { useAggressiveScan = aggressive; }
  void setLEDEnabled(bool enabled) { useLED = enabled; }

private:
  // Network management
  WifiCredentials networks[MAX_NETWORKS];
  uint8_t networkCount;
  int currentNetworkIndex;
  
  // LED pin
  uint8_t ledPin;
  bool useLED;
  
  // Timing
  uint32_t lastConnectAttempt;
  uint32_t lastSuccessfulConnect;
  uint32_t connectTimeout;
  uint32_t reconnectBackoff;
  uint32_t refreshInterval;
  
  // Internet connectivity check
  bool checkInternet;
  const char* internetCheckHost;
  uint16_t internetCheckPort;
  uint32_t internetCheckTimeout;
  
  // Scanning
  bool useAggressiveScan;
  
  // Internal methods
  int findStrongestNetwork();
  bool connectToNetwork(int networkIndex);
  bool hasInternetConnectivity();
  void flash(int count);
  void setLED(bool state);
};

#endif // RELIABLE_WIFI_H
