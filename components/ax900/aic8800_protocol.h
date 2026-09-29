/*
 * Copyright (c) 2026, Canaan Bright Sight Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Fixed-width messages used by the AIC8800 host/firmware interface.
 */
#ifndef __AIC8800_PROTOCOL_H__
#define __AIC8800_PROTOCOL_H__

#include <stdint.h>
#include <stddef.h>

#define AIC_WIRE_TASK_MM                 0U
#define AIC_WIRE_TASK_DBG                1U
#define AIC_WIRE_TASK_SCANU              4U
#define AIC_WIRE_TASK_ME                 5U
#define AIC_WIRE_TASK_SM                 6U
#define AIC_WIRE_TASK_APM                7U
#define AIC_WIRE_DRIVER_TASK           100U

#define AIC_WIRE_MSG(_task, _index) \
    ((uint16_t)(((_task) << 10) | (_index)))

#define AIC_MM_RESET_REQ          AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 0)
#define AIC_MM_RESET_CFM          AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 1)
#define AIC_MM_START_REQ          AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 2)
#define AIC_MM_START_CFM          AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 3)
#define AIC_MM_VERSION_REQ        AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 4)
#define AIC_MM_VERSION_CFM        AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 5)
#define AIC_MM_ADD_IF_REQ         AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 6)
#define AIC_MM_ADD_IF_CFM         AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 7)
#define AIC_MM_REMOVE_IF_REQ      AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 8)
#define AIC_MM_REMOVE_IF_CFM      AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 9)
#define AIC_MM_SET_FILTER_REQ     AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 14)
#define AIC_MM_SET_FILTER_CFM     AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 15)
#define AIC_MM_SET_CHANNEL_REQ    AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 16)
#define AIC_MM_SET_CHANNEL_CFM    AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 17)
#define AIC_MM_KEY_ADD_REQ        AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 36)
#define AIC_MM_KEY_ADD_CFM        AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 37)
#define AIC_MM_KEY_DEL_REQ        AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 38)
#define AIC_MM_KEY_DEL_CFM        AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 39)
/* The firmware serves one channel context at a time and announces the
 * transitions.  A VIF whose context is not the scheduled one must not be given
 * traffic; see aic_channel_context_active(). */
#define AIC_MM_CHANNEL_SWITCH_IND     AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 68)
#define AIC_MM_CHANNEL_PRE_SWITCH_IND AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 69)
/* An associated peer entering or leaving firmware-managed power save. */
#define AIC_MM_PS_CHANGE_IND      AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 73)
/* A sleeping peer has opened a legacy PS-Poll or U-APSD service period. */
#define AIC_MM_TRAFFIC_REQ_IND    AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 74)
#define AIC_MM_SET_COEX_REQ       AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 101)
#define AIC_MM_SET_COEX_CFM       AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 102)
#define AIC_MM_SET_RF_CONFIG_REQ  AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 103)
#define AIC_MM_SET_RF_CONFIG_CFM  AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 104)
#define AIC_MM_SET_RF_CALIB_REQ   AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 105)
#define AIC_MM_SET_RF_CALIB_CFM   AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 106)
#define AIC_MM_GET_MAC_REQ        AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 115)
#define AIC_MM_GET_MAC_CFM        AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 116)
#define AIC_MM_SET_TXPWR_REQ      AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 119)
#define AIC_MM_SET_TXPWR_CFM      AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 120)
#define AIC_MM_SET_TXPWR_OFST_REQ AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 121)
#define AIC_MM_SET_TXPWR_OFST_CFM AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 122)
#define AIC_MM_SET_STACK_START_REQ AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 123)
#define AIC_MM_SET_STACK_START_CFM AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 124)
#define AIC_MM_APM_STALOSS_IND  AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 125)
#define AIC_MM_GET_STA_INFO_REQ   AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 117)
#define AIC_MM_GET_STA_INFO_CFM   AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 118)
#define AIC_MM_GET_FW_VERSION_REQ  AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 128)
#define AIC_MM_GET_FW_VERSION_CFM  AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 129)
#define AIC_MM_SET_TXPWR_ADJ_REQ  AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 137)
#define AIC_MM_SET_TXPWR_ADJ_CFM  AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 138)
#define AIC_MM_FW_PANIC_IND      AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 149)
#define AIC_MM_FW_ASSERT_IND     AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 150)

#define AIC_DBG_MEM_MASK_WRITE_REQ AIC_WIRE_MSG(AIC_WIRE_TASK_DBG, 17)
#define AIC_DBG_MEM_MASK_WRITE_CFM AIC_WIRE_MSG(AIC_WIRE_TASK_DBG, 18)

/* AIC firmware ships with two incompatible MM_VERSION feature layouts.  The
 * full layout appears in the vendor USB driver, while current D80 firmware and
 * the SDIO driver use the compact layout.  Decode either layout into the
 * transport-independent AIC_FW_CAP_* masks before consuming it. */
#define AIC_MM_FULL_FEATURE_PS_BIT           6U
#define AIC_MM_FULL_FEATURE_DPSM_BIT         8U
#define AIC_MM_FULL_FEATURE_CHNL_CTXT_BIT   11U
#define AIC_MM_FULL_FEATURE_UMAC_BIT        15U
#define AIC_MM_FULL_FEATURE_VHT_BIT         16U
#define AIC_MM_FULL_FEATURE_BFMEE_BIT       17U
#define AIC_MM_FULL_FEATURE_MFP_BIT         20U
#define AIC_MM_FULL_FEATURE_MU_MIMO_RX_BIT  21U
#define AIC_MM_FULL_FEATURE_ANT_DIV_BIT     25U
#define AIC_MM_FULL_FEATURE_MON_DATA_BIT    29U
#define AIC_MM_FULL_FEATURE_HE_BIT          30U

#define AIC_MM_COMPACT_FEATURE_PS_BIT        2U
#define AIC_MM_COMPACT_FEATURE_UMAC_BIT      8U
#define AIC_MM_COMPACT_FEATURE_VHT_BIT       9U
#define AIC_MM_COMPACT_FEATURE_BFMEE_BIT    10U
#define AIC_MM_COMPACT_FEATURE_MFP_BIT      13U
#define AIC_MM_COMPACT_FEATURE_MU_MIMO_RX_BIT 14U
#define AIC_MM_COMPACT_FEATURE_ANT_DIV_BIT  18U
#define AIC_MM_COMPACT_FEATURE_MON_DATA_BIT 22U
#define AIC_MM_COMPACT_FEATURE_HE_BIT       23U

#define AIC_FW_CAP_PS                        (1UL << 0)
#define AIC_FW_CAP_DPSM                      (1UL << 1)
#define AIC_FW_CAP_CHNL_CTXT                 (1UL << 2)
#define AIC_FW_CAP_VHT                       (1UL << 3)
#define AIC_FW_CAP_MFP                       (1UL << 4)
#define AIC_FW_CAP_ANT_DIV                   (1UL << 5)
#define AIC_FW_CAP_MON_DATA                  (1UL << 6)
#define AIC_FW_CAP_HE                        (1UL << 7)
#define AIC_FW_CAP_BFMEE                     (1UL << 8)
#define AIC_FW_CAP_MU_MIMO_RX                (1UL << 9)

/* RX filter bits from the vendor NXMAC_ACCEPT_* register. */
#define AIC_RX_FILTER_MULTICAST       (1UL << 2)
#define AIC_RX_FILTER_BROADCAST       (1UL << 3)
#define AIC_RX_FILTER_OTHER_BSSID     (1UL << 4)
#define AIC_RX_FILTER_UNICAST         (1UL << 6)
#define AIC_RX_FILTER_MY_UNICAST      (1UL << 7)
#define AIC_RX_FILTER_PROBE_REQ       (1UL << 8)
#define AIC_RX_FILTER_PROBE_RESP      (1UL << 9)
#define AIC_RX_FILTER_BEACON          (1UL << 10)
#define AIC_RX_FILTER_OTHER_MGMT      (1UL << 15)
#define AIC_RX_FILTER_BAR             (1UL << 16)
#define AIC_RX_FILTER_BA              (1UL << 17)
#define AIC_RX_FILTER_PS_POLL         (1UL << 18)
#define AIC_RX_FILTER_OTHER_CONTROL   (1UL << 23)
#define AIC_RX_FILTER_DATA            (1UL << 24)
#define AIC_RX_FILTER_Q_DATA          (1UL << 26)
#define AIC_RX_FILTER_QOS_NULL        (1UL << 28)
#define AIC_RX_FILTER_OTHER_DATA      (1UL << 29)

#define AIC_RX_FILTER_FIXED (AIC_RX_FILTER_QOS_NULL | \
                             AIC_RX_FILTER_Q_DATA | \
                             AIC_RX_FILTER_DATA | \
                             AIC_RX_FILTER_OTHER_MGMT | \
                             AIC_RX_FILTER_MY_UNICAST | \
                             AIC_RX_FILTER_BROADCAST | \
                             AIC_RX_FILTER_BEACON | \
                             AIC_RX_FILTER_PROBE_RESP)
#define AIC_RX_FILTER_DEFAULT (AIC_RX_FILTER_FIXED | \
                               AIC_RX_FILTER_BA | \
                               AIC_RX_FILTER_BAR | \
                               AIC_RX_FILTER_OTHER_DATA | \
                               AIC_RX_FILTER_PROBE_REQ | \
                               AIC_RX_FILTER_PS_POLL)
#define AIC_RX_FILTER_PROMISCUOUS (AIC_RX_FILTER_DEFAULT | \
                                   AIC_RX_FILTER_OTHER_BSSID | \
                                   AIC_RX_FILTER_UNICAST | \
                                   AIC_RX_FILTER_MULTICAST | \
                                   AIC_RX_FILTER_OTHER_CONTROL)

#define AIC_SCANU_START_REQ       AIC_WIRE_MSG(AIC_WIRE_TASK_SCANU, 0)
#define AIC_SCANU_START_CFM       AIC_WIRE_MSG(AIC_WIRE_TASK_SCANU, 1)
#define AIC_SCANU_RESULT_IND      AIC_WIRE_MSG(AIC_WIRE_TASK_SCANU, 4)
#define AIC_SCANU_VENDOR_IE_REQ   AIC_WIRE_MSG(AIC_WIRE_TASK_SCANU, 7)
#define AIC_SCANU_VENDOR_IE_CFM   AIC_WIRE_MSG(AIC_WIRE_TASK_SCANU, 8)
#define AIC_SCANU_START_ACCEPTED  AIC_WIRE_MSG(AIC_WIRE_TASK_SCANU, 9)
#define AIC_SCANU_CANCEL_REQ      AIC_WIRE_MSG(AIC_WIRE_TASK_SCANU, 10)
#define AIC_SCANU_CANCEL_CFM      AIC_WIRE_MSG(AIC_WIRE_TASK_SCANU, 11)

#define AIC_ME_CONFIG_REQ         AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 0)
#define AIC_ME_CONFIG_CFM         AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 1)
#define AIC_ME_CHAN_CONFIG_REQ    AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 2)
#define AIC_ME_CHAN_CONFIG_CFM    AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 3)
#define AIC_ME_CONTROL_PORT_REQ   AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 4)
#define AIC_ME_CONTROL_PORT_CFM   AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 5)
#define AIC_ME_TKIP_MIC_FAILURE_IND AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 6)
#define AIC_ME_STA_ADD_REQ        AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 7)
#define AIC_ME_STA_ADD_CFM        AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 8)
#define AIC_ME_STA_DEL_REQ        AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 9)
#define AIC_ME_STA_DEL_CFM        AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 10)
/* Firmware-reported transmit credit offsets.  This generation's vendor driver
 * leaves enforcement disabled, so the values are collected for diagnostics. */
#define AIC_ME_TX_CREDITS_UPDATE_IND AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 11)
#define AIC_ME_TRAFFIC_IND_REQ    AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 12)
#define AIC_ME_TRAFFIC_IND_CFM    AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 13)
#define AIC_ME_RC_STATS_REQ       AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 14)
#define AIC_ME_RC_STATS_CFM       AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 15)
#define AIC_ME_SET_PS_MODE_REQ    AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 19)
#define AIC_ME_SET_PS_MODE_CFM    AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 20)

#define AIC_ME_PS_MODE_OFF        0U
#define AIC_ME_PS_MODE_ON         1U
#define AIC_ME_PS_MODE_ON_DYN     2U

#define AIC_SM_CONNECT_REQ        AIC_WIRE_MSG(AIC_WIRE_TASK_SM, 0)
#define AIC_SM_CONNECT_CFM        AIC_WIRE_MSG(AIC_WIRE_TASK_SM, 1)
#define AIC_SM_CONNECT_IND        AIC_WIRE_MSG(AIC_WIRE_TASK_SM, 2)
#define AIC_SM_DISCONNECT_REQ     AIC_WIRE_MSG(AIC_WIRE_TASK_SM, 3)
#define AIC_SM_DISCONNECT_CFM     AIC_WIRE_MSG(AIC_WIRE_TASK_SM, 4)
#define AIC_SM_DISCONNECT_IND     AIC_WIRE_MSG(AIC_WIRE_TASK_SM, 5)
#define AIC_SM_EXTERNAL_AUTH_REQUIRED_IND \
                                    AIC_WIRE_MSG(AIC_WIRE_TASK_SM, 6)
#define AIC_SM_EXTERNAL_AUTH_REQUIRED_RSP \
                                    AIC_WIRE_MSG(AIC_WIRE_TASK_SM, 7)
#define AIC_SM_EXTERNAL_AUTH_REQUIRED_RSP_CFM \
                                    AIC_WIRE_MSG(AIC_WIRE_TASK_SM, 12)

#define AIC_APM_START_REQ         AIC_WIRE_MSG(AIC_WIRE_TASK_APM, 0)
#define AIC_APM_START_CFM         AIC_WIRE_MSG(AIC_WIRE_TASK_APM, 1)
#define AIC_APM_STOP_REQ          AIC_WIRE_MSG(AIC_WIRE_TASK_APM, 2)
#define AIC_APM_STOP_CFM          AIC_WIRE_MSG(AIC_WIRE_TASK_APM, 3)
#define AIC_APM_SET_BEACON_IE_REQ AIC_WIRE_MSG(AIC_WIRE_TASK_APM, 8)
#define AIC_APM_SET_BEACON_IE_CFM AIC_WIRE_MSG(AIC_WIRE_TASK_APM, 9)

#define AIC_USB_TYPE_DATA                  0x00U
#define AIC_USB_TYPE_DATA_TX               0x01U
#define AIC_USB_TYPE_CONFIG                0x10U
#define AIC_USB_TYPE_COMMAND               0x11U
#define AIC_USB_TYPE_DATA_CONFIRM          0x12U
#define AIC_USB_TYPE_PRINT                 0x13U
#define AIC_USB_LENGTH_MASK              0x0fffU

#define AIC_WIRE_E2A_PATTERN       0xaddeDe2aUL
#define AIC_WIRE_MSG_API_VERSION_USB         15U
#define AIC_WIRE_MSG_API_VERSION_SDIO        33U
#define AIC_WIRE_SCAN_CHANNEL_COUNT          42U
#define AIC_WIRE_SCAN_SSID_COUNT              3U
#define AIC_WIRE_SCAN_IE_MAX                200U

struct aic_wire_mac_addr
{
    uint16_t array[3];
};

struct aic_wire_mac_ssid
{
    uint8_t length;
    uint8_t array[32];
};

struct aic_wire_mac_channel
{
    uint16_t frequency;
    uint8_t band;
    uint8_t flags;
    int8_t tx_power;
};

struct aic_wire_ht_capability
{
    uint16_t capability;
    uint8_t ampdu_parameters;
    uint8_t mcs_rate[16];
    uint16_t extended_capability;
    uint32_t beamforming_capability;
    uint8_t antenna_selection_capability;
};

struct aic_wire_vht_capability
{
    uint32_t capability;
    uint16_t rx_mcs_map;
    uint16_t rx_highest;
    uint16_t tx_mcs_map;
    uint16_t tx_highest;
};

struct aic_wire_he_mcs_support
{
    uint16_t rx_mcs_80;
    uint16_t tx_mcs_80;
    uint16_t rx_mcs_160;
    uint16_t tx_mcs_160;
    uint16_t rx_mcs_80p80;
    uint16_t tx_mcs_80p80;
};

struct aic_wire_he_capability
{
    uint8_t mac_capability[6];
    uint8_t phy_capability[11];
    struct aic_wire_he_mcs_support mcs;
    uint8_t ppe_thresholds[25];
};

struct aic_wire_mm_start_req
{
    uint32_t phy_parameters[16];
    uint32_t uapsd_timeout;
    uint16_t lp_clock_accuracy;
};

struct aic_wire_mm_set_coex_req
{
    uint8_t bt_on;
    uint8_t disable_coexnull;
    uint8_t enable_nullcts;
    uint8_t enable_periodic_timer;
    uint8_t coex_timeslot_set;
    uint8_t reserved[3];
    uint32_t coex_timeslot[2];
};

struct aic_wire_mm_version_cfm
{
    uint32_t lmac_version;
    uint32_t mac_version1;
    uint32_t mac_version2;
    uint32_t phy_version1;
    uint32_t phy_version2;
    uint32_t features;
    uint16_t max_stations;
    uint8_t max_vifs;
};

struct aic_wire_mm_set_stack_start_req
{
    uint8_t start;
    uint8_t efuse_valid;
    uint8_t vendor_info;
    uint8_t firmware_trace_redirect;
};

struct aic_wire_mm_set_stack_start_cfm
{
    uint8_t supports_5ghz;
    uint8_t vendor_info;
};

struct aic_wire_mm_get_sta_info_req
{
    uint8_t station_index;
};

struct aic_wire_mm_get_sta_info_compat_req
{
    uint8_t station_index;
    uint8_t pattern[3];
};

struct aic_wire_mm_set_filter_req
{
    uint32_t filter;
};

struct aic_wire_mm_get_sta_info_cfm
{
    uint32_t rate_info;
    uint32_t tx_failed;
    int8_t rssi;
    uint8_t reserved[3];
    uint32_t channel_time;
    uint32_t channel_busy_time;
    uint32_t ack_failed;
    uint32_t ack_succeeded;
    uint32_t channel_tx_busy_time;
};

/* The operating-channel ABI is shared by MM_SET_CHANNEL and several ME
 * messages in the vendor firmware.  Keep the field order byte-for-byte
 * compatible with struct mac_chan_op. */
struct aic_wire_mac_chan_op
{
    uint8_t band;
    uint8_t type;
    uint16_t primary_frequency;
    uint16_t center_frequency1;
    uint16_t center_frequency2;
    int8_t tx_power;
    uint8_t flags;
};

struct aic_wire_mm_set_channel_req
{
    struct aic_wire_mac_chan_op channel;
    uint8_t index;
};

struct aic_wire_mm_set_channel_cfm
{
    uint8_t radio_index;
    int8_t power;
};

struct aic_wire_me_set_ps_mode_req
{
    uint8_t state;
};

struct aic_wire_mm_ps_change_ind
{
    uint8_t station_index;
    uint8_t power_save;      /* 0: awake, 1: sleeping */
};

struct aic_wire_mm_traffic_req_ind
{
    uint8_t station_index;
    uint8_t packet_count;    /* 0: all buffered, 255: U-APSD interrupted */
    uint8_t uapsd;
};

struct aic_wire_mm_apm_staloss_ind
{
    uint8_t station_index;
    uint8_t vif_index;
    uint8_t address[6];
};

struct aic_wire_mm_firmware_fault_ind
{
    uint32_t length;
    uint8_t info[384];
};

struct aic_wire_mm_channel_switch_ind
{
    uint8_t channel_index;
    uint8_t remain_on_channel;
    uint8_t vif_index;
    uint8_t remain_on_channel_tdls;
};

struct aic_wire_mm_channel_pre_switch_ind
{
    uint8_t channel_index;
};

struct aic_wire_mm_get_fw_version_cfm
{
    uint8_t length;
    uint8_t version[63];
};

struct aic_wire_mm_add_if_req
{
    uint8_t type;
    struct aic_wire_mac_addr address;
    uint8_t p2p;
};

struct aic_wire_security_key
{
    uint8_t length;
    uint32_t array[8];
};

struct aic_wire_mm_key_add_req
{
    uint8_t key_index;
    uint8_t station_index;
    struct aic_wire_security_key key;
    uint8_t cipher;
    uint8_t vif_index;
    uint8_t spp;
    uint8_t pairwise;
};

struct aic_wire_mm_key_add_cfm
{
    uint8_t status;
    uint8_t hardware_key_index;
    uint8_t aligned[2];
};

struct aic_wire_mac_rateset
{
    uint8_t length;
    uint8_t array[12];
};

struct aic_wire_me_sta_add_req
{
    struct aic_wire_mac_addr address;
    struct aic_wire_mac_rateset rates;
    struct aic_wire_ht_capability ht;
    struct aic_wire_vht_capability vht;
    struct aic_wire_he_capability he;
    uint32_t flags;
    uint16_t aid;
    uint8_t uapsd_queues;
    uint8_t max_sp_length;
    uint8_t opmode;
    uint8_t vif_index;
    uint8_t tdls_station;
    uint8_t tdls_initiator;
    uint8_t tdls_channel_switch;
};

struct aic_wire_me_sta_add_cfm
{
    uint8_t station_index;
    uint8_t status;
    uint8_t power_state;
    uint8_t aligned;
};

struct aic_wire_me_sta_del_req
{
    uint8_t station_index;
    uint8_t tdls_station;
};

struct aic_wire_apm_start_req
{
    struct aic_wire_mac_rateset basic_rates;
    struct aic_wire_mac_channel channel;
    uint32_t center_frequency1;
    uint32_t center_frequency2;
    uint8_t channel_width;
    uint32_t beacon_address;
    uint16_t beacon_length;
    uint16_t tim_offset;
    uint16_t beacon_interval;
    uint32_t flags;
    uint16_t control_port_ethertype;
    uint8_t tim_length;
    uint8_t vif_index;
};

struct aic_wire_apm_start_cfm
{
    uint8_t status;
    uint8_t vif_index;
    uint8_t channel_index;
    uint8_t broadcast_station_index;
};

struct aic_wire_apm_set_beacon_ie_req
{
    uint8_t vif_index;
    uint16_t beacon_length;
    uint8_t beacon[512];
};

struct aic_wire_me_config_req
{
    struct aic_wire_ht_capability ht;
    struct aic_wire_vht_capability vht;
    struct aic_wire_he_capability he;
    uint16_t tx_lifetime;
    uint8_t max_bandwidth;
    uint8_t ht_supported;
    uint8_t vht_supported;
    uint8_t he_supported;
    uint8_t he_uplink_enabled;
    uint8_t power_save_enabled;
    uint8_t antenna_diversity_enabled;
    uint8_t dynamic_power_save;
};

struct aic_wire_me_tx_credits_update_ind
{
    uint8_t station_index;
    uint8_t tid;
    int8_t credits;          /* Offset to apply, may be negative. */
};

#define AIC_WIRE_RC_SAMPLE_COUNT 10U

struct aic_wire_me_rc_stats_req
{
    uint8_t station_index;
};

struct aic_wire_rc_rate_stats
{
    uint16_t attempts;
    uint16_t success;
    uint16_t probability;
    uint16_t rate_config;
    union
    {
        struct
        {
            uint8_t sample_skipped;
            uint8_t old_probability_available;
            uint8_t rate_allowed;
        } sample;
        uint16_t ru_and_length;
    } detail;
};

struct aic_wire_me_rc_stats_cfm
{
    uint8_t station_index;
    uint16_t sample_count;
    uint16_t ampdu_length;
    uint16_t ampdu_packets;
    uint32_t average_ampdu_length;
    uint8_t software_retry_step;
    uint8_t sample_wait;
    uint16_t retry_step_index[4];
    struct aic_wire_rc_rate_stats rates[AIC_WIRE_RC_SAMPLE_COUNT + 1U];
    uint32_t throughput[AIC_WIRE_RC_SAMPLE_COUNT + 1U];
};

struct aic_wire_me_channel_config_req
{
    struct aic_wire_mac_channel channels_2ghz[14];
    struct aic_wire_mac_channel channels_5ghz[28];
    uint8_t count_2ghz;
    uint8_t count_5ghz;
};

struct aic_wire_scanu_start_req
{
    struct aic_wire_mac_channel channels[AIC_WIRE_SCAN_CHANNEL_COUNT];
    struct aic_wire_mac_ssid ssids[AIC_WIRE_SCAN_SSID_COUNT];
    struct aic_wire_mac_addr bssid;
    uint32_t additional_ies;
    uint16_t additional_ie_length;
    uint8_t vif_index;
    uint8_t channel_count;
    uint8_t ssid_count;
    uint8_t no_cck;
    uint32_t duration_us;
};

struct aic_wire_sm_connect_req
{
    struct aic_wire_mac_ssid ssid;
    struct aic_wire_mac_addr bssid;
    struct aic_wire_mac_channel channel;
    uint32_t flags;
    uint16_t control_port_ethertype;
    uint16_t ie_length;
    uint16_t listen_interval;
    uint8_t dont_wait_bcmc;
    uint8_t auth_type;
    uint8_t uapsd_queues;
    uint8_t vif_index;
    uint32_t ie_buffer[64];
};

struct aic_wire_sm_external_auth_required_ind
{
    uint8_t vif_index;
    struct aic_wire_mac_ssid ssid;
    struct aic_wire_mac_addr bssid;
    uint32_t akm;
};

struct aic_wire_sm_external_auth_required_rsp
{
    uint8_t vif_index;
    uint8_t reserved;
    uint16_t status;
};

struct aic_wire_tx_host_descriptor
{
    uint16_t packet_length;
    uint16_t extended_flags;
    uint32_t status_descriptor;
    struct aic_wire_mac_addr destination;
    struct aic_wire_mac_addr source;
    uint16_t ethertype;
    uint8_t access_category;
    uint8_t tid;
    uint8_t vif_index;
    uint8_t station_index;
    uint16_t flags;
};

struct aic_wire_me_traffic_ind_req
{
    uint8_t station_index;
    uint8_t tx_available;
    uint8_t uapsd;
};

/* u64_l is naturally aligned at offset 8 in the vendor ABI. */
struct aic_wire_me_tkip_mic_failure_ind
{
    uint8_t address[6];
    uint8_t reserved[2];
    uint8_t tsc[8];
    uint8_t group_addressed;
    uint8_t key_index;
    uint8_t vif_index;
};

struct aic_wire_tx_power_index
{
    int8_t enable;
    int8_t dsss;
    int8_t ofdm_low_2ghz;
    int8_t ofdm_64qam_2ghz;
    int8_t ofdm_256qam_2ghz;
    int8_t ofdm_1024qam_2ghz;
    int8_t ofdm_low_5ghz;
    int8_t ofdm_64qam_5ghz;
    int8_t ofdm_256qam_5ghz;
    int8_t ofdm_1024qam_5ghz;
};

struct aic_wire_tx_power_v2
{
    uint8_t enable;
    int8_t legacy_2ghz[12];
    int8_t ht_vht_2ghz[10];
    int8_t he_2ghz[12];
};

struct aic_wire_tx_power_v3
{
    uint8_t enable;
    int8_t legacy_2ghz[12];
    int8_t ht_vht_2ghz[10];
    int8_t he_2ghz[12];
    int8_t legacy_5ghz[12];
    int8_t ht_vht_5ghz[10];
    int8_t he_5ghz[12];
};

struct aic_wire_tx_power_v4
{
    uint8_t enable;
    int8_t legacy_2ghz[12];
    int8_t ht_vht_2ghz[10];
    int8_t he_2ghz[12];
    int8_t legacy_5ghz[8];
    int8_t ht_vht_5ghz[10];
    int8_t he_5ghz[12];
    int8_t legacy_6ghz[8];
    int8_t ht_vht_6ghz[10];
    int8_t he_6ghz[12];
};

struct aic_wire_mm_set_tx_power_req
{
    union
    {
        struct aic_wire_tx_power_index index;
        struct aic_wire_tx_power_v2 v2;
        struct aic_wire_tx_power_v3 v3;
        struct aic_wire_tx_power_v4 v4;
    } configuration;
};

struct aic_wire_tx_power_offset
{
    int8_t enable;
    int8_t channels_1_4;
    int8_t channels_5_9;
    int8_t channels_10_13;
    int8_t channels_36_64;
    int8_t channels_100_120;
    int8_t channels_122_140;
    int8_t channels_142_165;
};

struct aic_wire_tx_power_offset_2x
{
    uint8_t enable;
    int8_t offsets_2ghz[3][3];
    int8_t offsets_5ghz[3][6];
};

struct aic_wire_tx_power_offset_2x_v2
{
    uint8_t enable;
    uint8_t flags;
    int8_t offsets_2ghz_ant0[3][3];
    int8_t offsets_2ghz_ant1[3][3];
    int8_t offsets_5ghz_ant0[6][3];
    int8_t offsets_5ghz_ant1[6][3];
    int8_t offsets_6ghz_ant0[15];
    int8_t offsets_6ghz_ant1[15];
};

struct aic_wire_mm_set_tx_power_offset_req
{
    union
    {
        struct aic_wire_tx_power_offset offset;
        struct aic_wire_tx_power_offset_2x offset_2x;
        struct aic_wire_tx_power_offset_2x_v2 offset_2x_v2;
    } configuration;
};

struct aic_wire_tx_power_adjust
{
    uint8_t enable;
    int8_t adjustment_2ghz[3];
    int8_t adjustment_5ghz[6];
};

struct aic_wire_mm_set_tx_power_adjust_req
{
    struct aic_wire_tx_power_adjust adjustment;
};

struct aic_wire_mm_set_rf_config_req
{
    uint8_t table_selector;
    uint8_t table_offset;
    uint8_t table_count;
    uint8_t default_page;
    uint32_t data[64];
};

struct aic_wire_mm_set_rf_calibration_req
{
    uint32_t calibration_2ghz;
    uint32_t calibration_5ghz;
    uint32_t alpha;
    uint32_t bluetooth_enabled;
    uint32_t bluetooth_parameter;
    uint8_t crystal_capacitance;
    uint8_t crystal_capacitance_fine;
};

struct aic_wire_mm_set_rf_calibration_cfm
{
    uint32_t rx_gain_2ghz_address;
    uint32_t rx_gain_5ghz_address;
    uint32_t tx_gain_2ghz_address;
    uint32_t tx_gain_5ghz_address;
};

_Static_assert(sizeof(struct aic_wire_mac_channel) == 6,
               "AIC channel ABI changed");
_Static_assert(sizeof(struct aic_wire_ht_capability) == 32,
               "AIC HT capability ABI changed");
_Static_assert(sizeof(struct aic_wire_he_capability) == 56,
               "AIC HE capability ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_start_req) == 72,
               "AIC start request ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_set_coex_req) == 16,
               "AIC coexistence request ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_version_cfm) == 28,
               "AIC version confirmation ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_set_stack_start_req) == 4,
               "AIC stack-start request ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_set_stack_start_cfm) == 2,
               "AIC stack-start confirmation ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_get_sta_info_req) == 1,
               "AIC station-info request ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_get_sta_info_compat_req) == 4,
               "AIC compatible station-info request ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_set_filter_req) == 4,
               "AIC RX-filter request ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_get_sta_info_cfm) == 32,
               "AIC station-info confirmation ABI changed");
_Static_assert(sizeof(struct aic_wire_mac_chan_op) == 10,
               "AIC operating-channel ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_set_channel_req) == 12,
               "AIC set-channel request ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_set_channel_cfm) == 2,
               "AIC set-channel confirmation ABI changed");
_Static_assert(sizeof(struct aic_wire_me_set_ps_mode_req) == 1,
               "AIC power-save request ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_get_fw_version_cfm) == 64,
               "AIC firmware-version confirmation ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_add_if_req) == 10,
               "AIC add-interface ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_key_add_req) == 44,
               "AIC key request ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_key_add_cfm) == 4,
               "AIC key confirmation ABI changed");
_Static_assert(sizeof(struct aic_wire_mac_rateset) == 13,
               "AIC rate-set ABI changed");
_Static_assert(sizeof(struct aic_wire_me_sta_add_req) == 136,
               "AIC station-add ABI changed");
_Static_assert(sizeof(struct aic_wire_me_sta_add_cfm) == 4,
               "AIC station-add confirmation ABI changed");
_Static_assert(sizeof(struct aic_wire_me_sta_del_req) == 2,
               "AIC station-delete ABI changed");
_Static_assert(sizeof(struct aic_wire_apm_start_req) == 52,
               "AIC AP-start ABI changed");
_Static_assert(sizeof(struct aic_wire_apm_start_cfm) == 4,
               "AIC AP-start confirmation ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_ps_change_ind) == 2,
               "AIC power-save indication ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_traffic_req_ind) == 3,
               "AIC power-save traffic request ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_apm_staloss_ind) == 8,
               "AIC AP station-loss indication ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_firmware_fault_ind) == 388,
               "AIC firmware-fault indication ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_channel_switch_ind) == 4,
               "AIC channel-switch indication ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_channel_pre_switch_ind) == 1,
               "AIC channel-pre-switch indication ABI changed");
_Static_assert(sizeof(struct aic_wire_apm_set_beacon_ie_req) == 516,
               "AIC beacon-upload ABI changed");
_Static_assert(sizeof(struct aic_wire_me_config_req) == 112,
               "AIC ME configuration ABI changed");
_Static_assert(sizeof(struct aic_wire_me_tx_credits_update_ind) == 3,
               "AIC transmit credit indication ABI changed");
_Static_assert(sizeof(struct aic_wire_me_rc_stats_req) == 1,
               "AIC RC statistics request ABI changed");
_Static_assert(sizeof(struct aic_wire_rc_rate_stats) == 12,
               "AIC RC rate statistics ABI changed");
_Static_assert(sizeof(struct aic_wire_me_rc_stats_cfm) == 200,
               "AIC RC statistics confirmation ABI changed");
_Static_assert(sizeof(struct aic_wire_me_channel_config_req) == 254,
               "AIC channel configuration ABI changed");
_Static_assert(sizeof(struct aic_wire_scanu_start_req) == 376,
               "AIC scan request ABI changed");
_Static_assert(sizeof(struct aic_wire_sm_connect_req) == 320,
               "AIC connect request ABI changed");
_Static_assert(sizeof(struct aic_wire_sm_external_auth_required_ind) == 44,
               "AIC external-auth indication ABI changed");
_Static_assert(sizeof(struct aic_wire_sm_external_auth_required_rsp) == 4,
               "AIC external-auth response ABI changed");
_Static_assert(sizeof(struct aic_wire_tx_host_descriptor) == 28,
               "AIC TX descriptor ABI changed");
_Static_assert(sizeof(struct aic_wire_me_traffic_ind_req) == 3,
               "AIC ME traffic indication ABI changed");
_Static_assert(sizeof(struct aic_wire_me_tkip_mic_failure_ind) == 19,
               "AIC TKIP MIC indication ABI changed");
_Static_assert(sizeof(struct aic_wire_tx_power_index) == 10,
               "AIC TX-power index ABI changed");
_Static_assert(sizeof(struct aic_wire_tx_power_v2) == 35,
               "AIC TX-power v2 ABI changed");
_Static_assert(sizeof(struct aic_wire_tx_power_v3) == 69,
               "AIC TX-power v3 ABI changed");
_Static_assert(sizeof(struct aic_wire_tx_power_v4) == 95,
               "AIC TX-power v4 ABI changed");
_Static_assert(sizeof(struct aic_wire_tx_power_offset) == 8,
               "AIC TX-power offset ABI changed");
_Static_assert(sizeof(struct aic_wire_tx_power_offset_2x) == 28,
               "AIC TX-power 2x offset ABI changed");
_Static_assert(sizeof(struct aic_wire_tx_power_offset_2x_v2) == 86,
               "AIC TX-power 2x v2 offset ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_set_tx_power_adjust_req) == 10,
               "AIC TX-power adjustment ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_set_rf_config_req) == 260,
               "AIC RF-config request ABI changed");
_Static_assert(sizeof(struct aic_wire_mm_set_rf_calibration_req) == 24,
               "AIC RF calibration ABI changed");

#endif /* __AIC8800_PROTOCOL_H__ */
