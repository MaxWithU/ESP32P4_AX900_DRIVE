#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif
// The application owns USB host installation and board power.
typedef enum {AX900_STOPPED,AX900_STARTING,AX900_RUNNING,AX900_STOPPING} ax900_lifecycle_t;
esp_err_t ax900_start(void);
// Asynchronous shutdown. Wait for STOPPED before restarting. A late USB callback
// or application-owned RX pbuf keeps STOPPING; host-owned memory is never freed.
// Close application sockets before stopping. The application owns the USB host.
// Releases network/test buffers but retains the USB client, worker and attached
// adapter identity to observe hotplug and resume without recalibrating firmware.
// This is a reversible stop, not full USB Host deinitialization.
esp_err_t ax900_stop(void);
ax900_lifecycle_t ax900_get_lifecycle(void);
#define AX900_MAX_APS 64
typedef struct {
    char ssid[33];
    uint8_t bssid[6];
    uint16_t frequency;
    int8_t rssi;
    bool secured;
    uint8_t ssid_len;
    uint8_t raw_ssid[32];
    uint16_t rsn_len;
    uint8_t rsn[258];
    uint8_t rsnxe_len, rsnxe[18];
    bool wpa2_psk, enterprise, sae, pmf_required;
} ax900_ap_t;
typedef struct {
    char status[96];
    bool present, ready, supports_5ghz, scanning;
    uint32_t scan_generation;
    size_t ap_count;
    bool connecting, associated, authenticated, has_ip;
    char connected_ssid[33];
    char ip[16];
    uint32_t tx_packets, rx_packets, rx_dropped;
    uint16_t disconnect_reason;
    uint32_t connection_id;
    uint16_t connected_frequency;
    uint8_t saved_networks;
    bool credentials_saved;
    esp_err_t profile_error;
    bool reconnect_enabled, reconnect_pending;
    uint8_t reconnect_attempts;
    uint32_t reconnect_in_ms, usb_errors, usb_recoveries;
    esp_err_t transport_error;
} ax900_link_status_t;
// Legacy full snapshot; prefer the compact status for periodic polling.
typedef struct {
    char status[96];
    bool present, ready, supports_5ghz, scanning;
    uint32_t scan_generation;
    size_t ap_count;
    ax900_ap_t aps[AX900_MAX_APS];
    bool connecting, associated, authenticated, has_ip;
    char connected_ssid[33];
    char ip[16];
    uint32_t tx_packets, rx_packets, rx_dropped;
    uint16_t disconnect_reason;
    uint32_t connection_id;
    uint16_t connected_frequency;
    uint8_t saved_networks;
    bool credentials_saved;
    esp_err_t profile_error;
    bool reconnect_enabled, reconnect_pending;
    uint8_t reconnect_attempts;
    uint32_t reconnect_in_ms, usb_errors, usb_recoveries;
    esp_err_t transport_error;
} ax900_status_t;
void ax900_get_link_status(ax900_link_status_t *out);
typedef struct {char ip[16],gateway[16],netmask[16],dns[16];} ax900_ipv4_info_t;
// Copies DHCP metadata without exposing a netif pointer that stop may destroy.
bool ax900_get_ipv4_info(ax900_ipv4_info_t *out);
// Copies scan entries only when needed; returns total stored entries.
size_t ax900_get_scan_results(ax900_ap_t *out, size_t capacity, uint32_t *generation);
void ax900_get_status(ax900_status_t *out);
// True only while this connection still has an authenticated AX900 DHCP lease.
bool ax900_connection_is_current(uint32_t connection_id);
esp_err_t ax900_request_scan(void);
// Queue connection to a scanned BSSID. Credentials are saved locally after DHCP succeeds.
// No API exports stored passwords; failures never overwrite a working profile.
// Supports open and WPA2-PSK/CCMP networks without mandatory PMF.
esp_err_t ax900_connect(const ax900_ap_t *ap, const char *password);
// All strings are copied. Trust validation requires a PEM CA and exact DNS name.
// Explicit opt-out allows unverified servers; never enable it silently.
typedef struct {
    const char *username;       // 1..128 UTF-8 bytes, optionally DOMAIN\\user
    const char *password;       // 1..128 UTF-8 bytes
    const char *ca_cert_pem;    // optional, at most 8192 bytes
    const char *server_name;    // required with CA, at most 253 bytes
    bool allow_unverified_server;
} ax900_peap_config_t;
esp_err_t ax900_connect_peap(const ax900_ap_t *ap, const ax900_peap_config_t *config);
esp_err_t ax900_connect_saved(const ax900_ap_t *ap);
bool ax900_has_saved(const ax900_ap_t *ap);
// Forget all AX900 profiles; keep the current connection until explicitly disconnected.
esp_err_t ax900_forget_saved(void);
// Diagnostic association only: never sends credentials or opens the data port.
// Automatically disconnects after 10 seconds.
esp_err_t ax900_test_association(const ax900_ap_t *ap);
// Cancels automatic recovery too, including while the USB adapter is absent.
esp_err_t ax900_disconnect(void);
#ifdef __cplusplus
}
#endif
