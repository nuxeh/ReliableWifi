#include <Arduino.h>
#include <ReliableWifi.h>

// Create WiFi manager instance
ReliableWiFi wifiManager(5);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== ReliableWiFi Example ===\n");
  
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
  wifiManager.addNetwork("Yard", "PASSWORD");
  wifiManager.addNetwork("Garage", "PASSWORD");
  wifiManager.addNetwork("House", "PASSWORD");
  
  // Connect to the strongest available network
  if (wifiManager.begin()) {
    Serial.println("\nSuccessfully connected!");
    Serial.printf("Connected to: %s\n", wifiManager.getCurrentSSID().c_str());
  } else {
    Serial.println("\nFailed to connect to any network");
  }
}

void loop() {
  // Call maintain() regularly to handle reconnections and network switching
  wifiManager.maintain();
  
  // Your application code here
  if (wifiManager.isConnected()) {
    // Do something that requires WiFi
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 10000) {
      Serial.printf("Still connected to: %s (RSSI: %d dBm)\n", 
                    wifiManager.getCurrentSSID().c_str(), 
                    WiFi.RSSI());
      lastPrint = millis();
    }
  } else {
    Serial.println("WiFi not connected, waiting for reconnection...");
  }
  
  delay(100);
}

// Example: Force reconnection on demand
void forceReconnect() {
  Serial.println("Forcing WiFi reconnection...");
  wifiManager.reconnect();
}
