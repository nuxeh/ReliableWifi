#include <Arduino.h>
#include "ReliableWifi.h"

// Create WiFi manager instance
ReliableWiFi wifiManager(5);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\r\n=== ReliableWiFi Async Example ===\r\n");
  
  // Configure the WiFi manager (optional - these are the defaults)
  wifiManager.setConnectTimeout(15000);        // 15 seconds to connect
  wifiManager.setReconnectBackoff(30000);      // Wait 30s between reconnection attempts
  wifiManager.setRefreshInterval(3600000);     // Refresh connection every hour
  wifiManager.setInternetCheckEnabled(true);   // Enable internet connectivity check
  wifiManager.setInternetCheckHost("8.8.8.8"); // Google DNS
  wifiManager.setInternetCheckPort(53);        // DNS port
  wifiManager.setInternetCheckTimeout(5000);   // 5 second timeout
  wifiManager.setAggressiveScan(false);        // Use gentle scanning
  wifiManager.setLEDEnabled(true);             // Enable LED feedback
  
  // Add your networks (strongest signal will be chosen automatically)
  wifiManager.addNetwork("Yard", "PASS1234");
  wifiManager.addNetwork("Garage", "PASS1234");
  wifiManager.addNetwork("Grain Store", "PASS1234");
  
  // Start async connection process (non-blocking)
  wifiManager.begin();
  
  Serial.println("WiFi connection started (async)...");
  Serial.println("maintain() will handle the connection process");
}

void loop() {
  // IMPORTANT: Call maintain() regularly - this is required for async operation!
  // This handles scanning, connecting, and reconnecting without blocking
  wifiManager.maintain();
  
  // Your application code here - runs even while WiFi is connecting!
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 5000) {
    
    // Check current state
    switch(wifiManager.getState()) {
      case WIFI_STATE_IDLE:
        Serial.println("State: IDLE");
        break;
      case WIFI_STATE_SCANNING:
        Serial.println("State: SCANNING (async, non-blocking)");
        break;
      case WIFI_STATE_CONNECTING:
        Serial.println("State: CONNECTING...");
        break;
      case WIFI_STATE_CHECKING_INTERNET:
        Serial.println("State: CHECKING INTERNET...");
        break;
      case WIFI_STATE_CONNECTED:
        Serial.printf("State: CONNECTED to %s (RSSI: %d dBm)\r\n", 
                      wifiManager.getCurrentSSID().c_str(), 
                      WiFi.RSSI());
        break;
      case WIFI_STATE_DISCONNECTED:
        Serial.println("State: DISCONNECTED (will retry after backoff)");
        break;
      case WIFI_STATE_INTERNET_CHECK_FAILED:
        Serial.println("State: NO INTERNET (will try different network)");
        break;
      default:
        Serial.println("State: UNKNOWN");
        break;
    }
    
    lastPrint = millis();
  }
  
  // Example: Do work that requires WiFi
  if (wifiManager.isConnected()) {
    // Your WiFi-dependent code here
    // For example: MQTT, HTTP requests, etc.
  }
  
  // Example: Other tasks that don't require WiFi
  // Sensor reading, local processing, etc.
  
  // Small delay to prevent tight loop (optional)
  delay(100);
}

// Example: Force reconnection on demand (e.g., from a button press)
void forceReconnect() {
  Serial.println("Forcing WiFi reconnection...");
  wifiManager.reconnect();
}
