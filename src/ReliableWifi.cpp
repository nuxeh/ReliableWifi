#include "ReliableWifi.h"

ReliableWiFi::ReliableWiFi(uint8_t ledPin)
  : ledPin(ledPin),
    networkCount(0),
    currentNetworkIndex(-1),
    useLED(true),
    lastConnectAttempt(0),
    lastSuccessfulConnect(0),
    connectTimeout(15000),
    reconnectBackoff(30000),
    refreshInterval(3600000),
    checkInternet(true),
    internetCheckHost("8.8.8.8"),
    internetCheckPort(53),
    internetCheckTimeout(5000),
    useAggressiveScan(false) {
  pinMode(ledPin, OUTPUT);
  setLED(false);
}

bool ReliableWiFi::addNetwork(const char* ssid, const char* password) {
  if (networkCount >= MAX_NETWORKS) {
    Serial.println("Error: Maximum number of networks reached");
    return false;
  }

  if (strlen(ssid) > MAX_SSID_LEN || strlen(password) > MAX_PASSWORD_LEN) {
    Serial.println("Error: SSID or password too long");
    return false;
  }

  strncpy(networks[networkCount].ssid, ssid, MAX_SSID_LEN);
  networks[networkCount].ssid[MAX_SSID_LEN] = '\0';
  strncpy(networks[networkCount].password, password, MAX_PASSWORD_LEN);
  networks[networkCount].password[MAX_PASSWORD_LEN] = '\0';
  networkCount++;

  Serial.printf("Added network: %s (total: %d)\r\n", ssid, networkCount);
  return true;
}

int ReliableWiFi::findStrongestNetwork() {
  if (networkCount == 0) {
    Serial.println("Error: No networks configured");
    return -1;
  }

  if (useLED) analogWrite(ledPin, 127);

  Serial.println("\r\nScanning for WiFi networks...");

  int numScannedNetworks;

#ifdef ESP32
  // ESP32 supports more scan parameters
  if (useAggressiveScan) {
    numScannedNetworks = WiFi.scanNetworks(false, false, false, 300);
  } else {
    numScannedNetworks = WiFi.scanNetworks();
  }
#else
  // ESP8266 only supports basic scanning
  numScannedNetworks = WiFi.scanNetworks();
#endif

  Serial.printf("Scan complete. Found %d networks:\r\n", numScannedNetworks);

  if (numScannedNetworks == 0) {
    if (useLED) setLED(false);
    return -1;
  }

  int bestRSSI = -1000;
  int bestNetworkIndex = -1;

  for (int i = 0; i < numScannedNetworks; i++) {
    String scannedSSID = WiFi.SSID(i);
    int scannedRSSI = WiFi.RSSI(i);

    Serial.printf("  %s (RSSI: %d)\r\n", scannedSSID.c_str(), scannedRSSI);

    for (int j = 0; j < networkCount; j++) {
      if (strcmp(scannedSSID.c_str(), networks[j].ssid) == 0) {
        if (scannedRSSI > bestRSSI) {
          bestRSSI = scannedRSSI;
          bestNetworkIndex = j;
        }
        break;
      }
    }
  }

  if (bestNetworkIndex != -1) {
    Serial.printf("Best network: %s (RSSI: %d)\r\n",
                  networks[bestNetworkIndex].ssid, bestRSSI);
  } else {
    Serial.println("No configured networks found in scan");
  }

  if (useLED) {
    analogWrite(ledPin, 40);
    delay(500);
    pinMode(ledPin, OUTPUT);
    setLED(false);
  }

  return bestNetworkIndex;
}

bool ReliableWiFi::hasInternetConnectivity() {
  if (!checkInternet) {
    return true; // Assume connected if check is disabled
  }

  Serial.printf("Checking internet connectivity (%s:%d)...\r\n",
                internetCheckHost, internetCheckPort);

  WiFiClient client;

#ifdef ESP8266
  // ESP8266 uses setTimeout
  client.setTimeout(internetCheckTimeout);
  bool connected = client.connect(internetCheckHost, internetCheckPort);
#else
  // ESP32 supports timeout parameter directly
  bool connected = client.connect(internetCheckHost, internetCheckPort, internetCheckTimeout);
#endif

  if (connected) {
    Serial.println("Internet connectivity: OK");
    client.stop();
    return true;
  } else {
    Serial.println("Internet connectivity: FAILED");
    client.stop();
    return false;
  }
}

bool ReliableWiFi::connectToNetwork(int networkIndex) {
  if (networkIndex < 0 || networkIndex >= networkCount) {
    Serial.println("Error: Invalid network index");
    return false;
  }

#ifdef ESP8266
  // ESP8266 needs non-const char* for SSID
  char* ssid = networks[networkIndex].ssid;
  char* password = networks[networkIndex].password;
#else
  // ESP32 accepts const char*
  const char* ssid = networks[networkIndex].ssid;
  const char* password = networks[networkIndex].password;
#endif

  lastConnectAttempt = millis();

  Serial.printf("Connecting to: %s\r\n", ssid);
  WiFi.begin(ssid, password);

#ifdef ESP32
  // Optional: Set TX power on ESP32 if needed
  // WiFi.setTxPower(WIFI_POWER_19_5dBm);
#endif

  int attempts = 0;
  int maxAttempts = connectTimeout / 500;

  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    if (useLED) setLED(!digitalRead(ledPin));
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    if (useLED) setLED(true);
    Serial.println("WiFi connected!");

#ifdef ESP8266
    // ESP8266 WiFi.SSID() returns char*
    Serial.printf("  SSID: %s\r\n", WiFi.SSID());
#else
    // ESP32 WiFi.SSID() returns String
    Serial.printf("  SSID: %s\r\n", WiFi.SSID().c_str());
#endif

    Serial.printf("  IP: %s\r\n", WiFi.localIP().toString().c_str());
    Serial.printf("  RSSI: %d dBm\r\n", WiFi.RSSI());

    // Check internet connectivity
    if (checkInternet && !hasInternetConnectivity()) {
      Serial.println("WiFi connected but no internet access");
      WiFi.disconnect();
      if (useLED) setLED(false);
      return false;
    }

    flash(5);
    lastSuccessfulConnect = millis();
    currentNetworkIndex = networkIndex;
    return true;
  } else {
    if (useLED) setLED(false);
    Serial.printf("Failed to connect to %s\r\n", ssid);
    flash(3);
    return false;
  }
}

bool ReliableWiFi::begin() {
  if (networkCount == 0) {
    Serial.println("Error: No networks configured. Use addNetwork() first.");
    return false;
  }

  Serial.println("ReliableWiFi: Starting...");

#ifdef ESP32
  Serial.println("Platform: ESP32");
#else
  Serial.println("Platform: ESP8266");
#endif

  // Try to connect to the strongest available network
  int networkIndex = findStrongestNetwork();

  if (networkIndex == -1) {
    Serial.println("No configured networks found");
    return false;
  }

  return connectToNetwork(networkIndex);
}

bool ReliableWiFi::reconnect() {
  Serial.println("ReliableWiFi: Forcing reconnection...");
  WiFi.disconnect();
  delay(100);
  return begin();
}

void ReliableWiFi::maintain() {
  uint32_t currentMillis = millis();

  // Check if we need to attempt reconnection
  if (currentMillis - lastConnectAttempt > reconnectBackoff) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, attempting reconnection...");
      flash(5);

      // Try current network first if we have one
      if (currentNetworkIndex >= 0) {
        if (connectToNetwork(currentNetworkIndex)) {
          return;
        }
      }

      // If that fails, scan for the strongest network
      int networkIndex = findStrongestNetwork();
      if (networkIndex >= 0) {
        connectToNetwork(networkIndex);
      }
    }
    // Periodic refresh to potentially switch to a stronger network
    else if (currentMillis - lastSuccessfulConnect > refreshInterval) {
      Serial.println("Refreshing WiFi connection...");
      flash(10);

      // Check if we still have internet
      if (checkInternet && !hasInternetConnectivity()) {
        Serial.println("Lost internet connectivity, switching networks...");
        WiFi.disconnect();
        delay(100);

        int networkIndex = findStrongestNetwork();
        if (networkIndex >= 0) {
          connectToNetwork(networkIndex);
        }
      } else {
        // Just refresh the current connection
        WiFi.disconnect();
        delay(100);
        int networkIndex = findStrongestNetwork();
        if (networkIndex >= 0) {
          connectToNetwork(networkIndex);
        }
      }
    }
    // Check internet connectivity periodically even when connected
    else if (checkInternet && (currentMillis - lastSuccessfulConnect > 60000)) {
      if (!hasInternetConnectivity()) {
        Serial.println("Internet connectivity lost, switching networks...");
        WiFi.disconnect();
        delay(100);

        int networkIndex = findStrongestNetwork();
        if (networkIndex >= 0 && networkIndex != currentNetworkIndex) {
          connectToNetwork(networkIndex);
        }
      }
    }
  }
}

bool ReliableWiFi::isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

String ReliableWiFi::getCurrentSSID() {
  if (isConnected()) {
#ifdef ESP8266
    return String(WiFi.SSID());
#else
    return WiFi.SSID();
#endif
  }
  return "";
}

void ReliableWiFi::flash(int count) {
  if (!useLED) return;

  bool oldState = digitalRead(ledPin);
  for (int i = 0; i < count; i++) {
    digitalWrite(ledPin, HIGH);
    delay(100);
    digitalWrite(ledPin, LOW);
    delay(100);
  }
  digitalWrite(ledPin, oldState);
}

void ReliableWiFi::setLED(bool state) {
  if (useLED) {
    digitalWrite(ledPin, state);
  }
}
