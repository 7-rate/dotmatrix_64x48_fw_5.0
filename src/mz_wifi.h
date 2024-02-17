#ifndef MZ_WIFI_H__
#define MZ_WIFI_H__
#include <WiFi.h>
#include <limits.h>
#include <vector>

void wifi_set_clear_setting_flag();

void wifi_setup();
void wifi_check();
void wifi_wps();
WiFiEvent_t wifi_get_wps_status();
void wifi_stop_wps();

void wifi_start();
void wifi_write_settings();


struct ip_addr_settings_t
{
	String ip_addr;
	String ip_gateway;
	String ip_mask;
	String dns1;
	String dns2;

	ip_addr_settings_t();
	void clear();
	void dump(const char * address_zero_comment = nullptr) const;
};

struct wifi_scan_item_t
{
	String SSID;
	wifi_auth_mode_t encryptionType;
	int32_t RSSI;
	uint8_t BSSID[6];
	String BSSIDstr;
	int32_t channel;
};

const String & wifi_get_ap_name();
const String & wifi_get_ap_pass();
ip_addr_settings_t wifi_get_ip_addr_settings(bool use_current_config = false);

void wifi_set_ap_info(const String &_ap_name, const String &_ap_pass);

void wifi_set_ap_info(const String &_ap_name, const String &_ap_pass,
	const ip_addr_settings_t & ip);

void wifi_manual_ip_info(const ip_addr_settings_t & ip);

String wifi_get_connection_info_string();
bool validate_ipv4_address(const String &string_addr);
bool validate_ipv4_netmask(const String  &string_addr);

/**
 * retrieves wifi network list from WiFiScanClass, then sort them by RSSID,
 * then returns them. Scan results stored in WiFiScanClass are cleared.
 * */
std::vector<wifi_scan_item_t>  get_wifi_scan_list(size_t max = SIZE_MAX);

extern const String null_ip_addr; // contains "0.0.0.0"
#endif

