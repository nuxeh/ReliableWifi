#include "ReliableWifi.h"

ReliableWiFi::ReliableWiFi(uint8_t ledPin)
  : ledPin(ledPin),
    networkCount(0),
    currentNetworkIndex(-1),
    targetNetworkIndex(-1),
    useLED(true),
    lastConnectAttempt(0),
    lastSuccessfulConnect(0),
    connectStartTime(0),
    connectTimeout(15000),
    reconnectBackoff(30000),
    refreshInterval(3600000),
    lastInternetCheck(0),
    checkInternet(true),
    internetCheckHost("8.8.8.8"),
    internetCheckPort(53),
    internetCheckTimeout(5000),
    useAggressiveScan(false),
    scanInProgress(false),
    currentState(WIFI_STATE_IDLE) {
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

void ReliableWiFi::setState(WifiState newState) {
  if (currentState != newState) {
    currentState = newState;

    // Visual feedback for state changes
    switch(newState) {
      case WIFI_STATE_SCANNING:
        if (useLED) analogWrite(ledPin, 127);
        break;
      case WIFI_STATE_CONNECTING:
        // LED will blink in handleConnecting()
        break;
      case WIFI_STATE_CONNECTED:
        if (useLED) setLED(true);
        break;
      case WIFI_STATE_DISCONNECTED:
        if (useLED) setLED(false);
        break;
      default:
        break;
    }
  }
}

void ReliableWiFi::startScan() {
  if (networkCount == 0) {
    Serial.println("Error: No networks configured");
    return;
  }

  if (scanInProgress) {
    return; // Already scanning
  }

  Serial.println("\r\nStarting async WiFi scan...");
  setState(WIFI_STATE_SCANNING);

#ifdef ESP32
  // ESP32 async scan
  if (useAggressiveScan) {
    WiFi.scanNetworks(true, false, false, 300);
  } else {
    WiFi.scanNetworks(true);
  }
#else
  // ESP8266 async scan
  WiFi.scanNetworks(true, false);
#endif

  scanInProgress = true;
}

int ReliableWiFi::processScanResults() {
  int numScannedNetworks = WiFi.scanComplete();

  if (numScannedNetworks == WIFI_SCAN_RUNNING) {
    return -2; // Still scanning
  }

  if (numScannedNetworks == WIFI_SCAN_FAILED) {
    Serial.println("WiFi scan failed");
    scanInProgress = false;
    WiFi.scanDelete();
    return -1;
  }

  // Scan complete
  Serial.printf("Scan complete. Found %d networks:\r\n", numScannedNetworks);
  scanInProgress = false;

  if (numScannedNetworks == 0) {
    WiFi.scanDelete();
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

  WiFi.scanDelete(); // Free memory

  if (bestNetworkIndex != -1) {
    Serial.printf("Best network: %s (RSSI: %d)\r\n",
                  networks[bestNetworkIndex].ssid, bestRSSI);
  } else {
    Serial.println("No configured networks found in scan");
  }

  if (useLED) {
    analogWrite(ledPin, 40);
    delay(100);
    pinMode(ledPin, OUTPUT);
    setLED(false);
  }

  setState(WIFI_STATE_SCAN_COMPLETE);
  return bestNetworkIndex;
}

void ReliableWiFi::startConnection(int networkIndex) {
  if (networkIndex < 0 || networkIndex >= networkCount) {
    Serial.println("Error: Invalid network index");
    setState(WIFI_STATE_DISCONNECTED);
    return;
  }

#ifdef ESP8266
  char* ssid = networks[networkIndex].ssid;
  char* password = networks[networkIndex].password;
#else
  const char* ssid = networks[networkIndex].ssid;
  const char* password = networks[networkIndex].password;
#endif

  targetNetworkIndex = networkIndex;
  lastConnectAttempt = millis();
  connectStartTime = millis();

  Serial.printf("Connecting to: %s\r\n", ssid);
  WiFi.begin(ssid, password);

#ifdef ESP32
  // Optional: Set TX power on ESP32 if needed
  // WiFi.setTxPower(WIFI_POWER_19_5dBm);
#endif

  setState(WIFI_STATE_CONNECTING);
}

void ReliableWiFi::handleConnecting() {
  // Blink LED while connecting
  static uint32_t lastBlink = 0;
  if (useLED && millis() - lastBlink > 500) {
    setLED(!digitalRead(ledPin));
    lastBlink = millis();
  }

  // Feed the watchdog
  yield();

  if (WiFi.status() == WL_CONNECTED) {
    if (useLED) setLED(true);
    Serial.println("WiFi connected!");

    Serial.printf("  SSID: %s\r\n", WiFi.SSID().c_str());

    Serial.printf("  IP: %s\r\n", WiFi.localIP().toString().c_str());
    Serial.printf("  RSSI: %d dBm\r\n", WiFi.RSSI());

    flash(5);
    lastSuccessfulConnect = millis();
    currentNetworkIndex = targetNetworkIndex;

    if (checkInternet) {
      setState(WIFI_STATE_CHECKING_INTERNET);
    } else {
      setState(WIFI_STATE_CONNECTED);
    }
  } else if (millis() - connectStartTime > connectTimeout) {
    // Connection timeout
    if (useLED) setLED(false);
    Serial.printf("Failed to connect to %s (timeout)\r\n", networks[targetNetworkIndex].ssid);
    flash(3);
    WiFi.disconnect();
    setState(WIFI_STATE_DISCONNECTED);
  }
}

void ReliableWiFi::handleInternetCheck() {
  if (hasInternetConnectivity()) {
    setState(WIFI_STATE_CONNECTED);
    lastInternetCheck = millis();
  } else {
    Serial.println("WiFi connected but no internet access");
    WiFi.disconnect();
    if (useLED) setLED(false);
    setState(WIFI_STATE_INTERNET_CHECK_FAILED);
  }
}

bool ReliableWiFi::hasInternetConnectivity() {
  if (!checkInternet) {
    return true;
  }

  Serial.printf("Checking internet connectivity (%s:%d)...\r\n",
                internetCheckHost, internetCheckPort);

  WiFiClient client;

#ifdef ESP8266
  client.setTimeout(internetCheckTimeout);
  bool connected = client.connect(internetCheckHost, internetCheckPort);
#else
  bool connected = client.connect(internetCheckHost, internetCheckPort, internetCheckTimeout);
#endif

  // Feed watchdog during check
  yield();

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

  startScan();
  return true; // Async operation started
}

bool ReliableWiFi::reconnect() {
  Serial.println("ReliableWiFi: Forcing reconnection...");
  WiFi.disconnect();
  delay(100);
  setState(WIFI_STATE_IDLE);
  return begin();
}

void ReliableWiFi::maintain() {
  uint32_t currentMillis = millis();

  // Feed the watchdog
  yield();

  // State machine
  switch (currentState) {
    case WIFI_STATE_IDLE:
      // Check if we should attempt connection
      if (currentMillis - lastConnectAttempt > reconnectBackoff) {
        startScan();
      }
      break;

    case WIFI_STATE_SCANNING:
      // Check if scan is complete
      {
        int networkIndex = processScanResults();
        if (networkIndex >= 0) {
          startConnection(networkIndex);
        } else if (networkIndex == -1) {
          // Scan failed or no networks found
          setState(WIFI_STATE_IDLE);
        }
        // -2 means still scanning, keep waiting
      }
      break;

    case WIFI_STATE_CONNECTING:
      handleConnecting();
      break;

    case WIFI_STATE_CHECKING_INTERNET:
      handleInternetCheck();
      break;

    case WIFI_STATE_CONNECTED:
      // Check if still connected
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected!");
        setState(WIFI_STATE_DISCONNECTED);
        flash(5);
      }
      // Periodic refresh
      else if (currentMillis - lastSuccessfulConnect > refreshInterval) {
        Serial.println("Refreshing WiFi connection...");
        flash(10);
        WiFi.disconnect();
        delay(100);
        setState(WIFI_STATE_IDLE);
      }
      // Periodic internet check
      else if (checkInternet && (currentMillis - lastInternetCheck > 60000)) {
        if (!hasInternetConnectivity()) {
          Serial.println("Internet connectivity lost, switching networks...");
          WiFi.disconnect();
          delay(100);
          setState(WIFI_STATE_IDLE);
        } else {
          lastInternetCheck = currentMillis;
        }
      }
      break;

    case WIFI_STATE_DISCONNECTED:
    case WIFI_STATE_INTERNET_CHECK_FAILED:
      // Wait for backoff period before trying again
      if (currentMillis - lastConnectAttempt > reconnectBackoff) {
        Serial.println("Attempting reconnection after backoff...");

        // Try current network first if we have one
        if (currentNetworkIndex >= 0 && currentState == WIFI_STATE_DISCONNECTED) {
          startConnection(currentNetworkIndex);
        } else {
          // Scan for best network
          startScan();
        }
      }
      break;

    case WIFI_STATE_SCAN_COMPLETE:
      // Transition state, should move to connecting quickly
      setState(WIFI_STATE_IDLE);
      break;
  }
}

bool ReliableWiFi::isConnected() {
  return currentState == WIFI_STATE_CONNECTED && WiFi.status() == WL_CONNECTED;
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
    yield(); // Feed watchdog
  }
  digitalWrite(ledPin, oldState);
}

void ReliableWiFi::setLED(bool state) {
  if (useLED) {
    digitalWrite(ledPin, state);
  }
}
