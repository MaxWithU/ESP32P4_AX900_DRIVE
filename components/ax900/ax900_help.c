// SPDX-License-Identifier: Apache-2.0
#include "ax900_issue.h"
#include <stdatomic.h>
#include <errno.h>
static atomic_int connection_issue;
void ax_issue_set(ax900_issue_t issue){atomic_store(&connection_issue,issue);}
void ax_issue_if_clear(ax900_issue_t issue){int expected=AX900_ISSUE_NONE;atomic_compare_exchange_strong(&connection_issue,&expected,issue);}
ax900_issue_t ax900_get_connection_issue(void){return atomic_load(&connection_issue);}
#define HELP(title,action) ((ax900_help_t){title,action})
ax900_help_t ax900_issue_help(ax900_issue_t issue){
    switch(issue){
    case AX900_ISSUE_NONE:return HELP("","");
    case AX900_ISSUE_ASSOCIATION:return HELP("Access point not reached","Move closer, scan again and select the network.");
    case AX900_ISSUE_AUTHENTICATION:return HELP("Authentication rejected","Check login and network policy, then reconnect. The cause is not confirmed.");
    case AX900_ISSUE_AUTH_TIMEOUT:return HELP("Authentication timed out","Check signal and router availability, then reconnect.");
    case AX900_ISSUE_DHCP:return HELP("No IP address assigned","Check the router's DHCP service and free addresses, then reconnect.");
    case AX900_ISSUE_CLOCK:return HELP("Device time is not set","Set accurate date/time before using server certificate verification.");
    case AX900_ISSUE_TLS_CONFIG:return HELP("Certificate checks unavailable","Install firmware with certificate date checks enabled.");
    case AX900_ISSUE_CA:return HELP("CA certificate cannot be read","Choose a valid PEM CA on the SD card and enter the server name.");
    case AX900_ISSUE_CERTIFICATE:return HELP("Server certificate not trusted","Check the CA and ask the network administrator to verify the server certificate.");
    case AX900_ISSUE_CERT_DATE:return HELP("Certificate date check failed","Check device date/time and the server certificate's validity dates.");
    case AX900_ISSUE_CERT_NAME:return HELP("Server name does not match","Confirm the expected server DNS name with the network administrator.");
    case AX900_ISSUE_TLS:return HELP("Secure handshake failed","Check enterprise network availability and TLS settings with the administrator.");
    case AX900_ISSUE_MEMORY:return HELP("Not enough memory","Close other apps and retry; restart Tab5 if the problem continues.");
    case AX900_ISSUE_CONNECTION:return HELP("Connection could not start","Wait for the adapter to become ready, then scan and select the network again.");
    default:return HELP("Connection failed","Reconnect and retry. If it persists, check local diagnostics.");
    }
}
ax900_help_t ax900_connection_help(const ax900_link_status_t *s){
    if(!s)return ax900_issue_help(AX900_ISSUE_NONE);
    if(!s->present)return HELP("AX900 is not attached","Plug AX900 into Tab5's USB-A port and wait for initialization.");
    if(!s->ready && s->transport_error)return HELP("USB adapter needs recovery","Wait for recovery; if it stops, replug AX900 or restart Tab5.");
    if(!s->ready)return HELP("Initializing AX900","Wait for the adapter to become ready before selecting a network.");
    if(!s->has_ip){
        ax900_help_t h=ax900_issue_help(ax900_get_connection_issue());
        if(h.title[0])return h;
        if(s->authenticated)return HELP("Waiting for an IP address","If this persists, check the router's DHCP service.");
    }
    if(s->profile_error)return HELP("Network settings were not saved","Connection may still work. Retry saving; check local storage diagnostics if it repeats.");
    return ax900_issue_help(AX900_ISSUE_NONE);
}
ax900_help_t ax900_request_help(esp_err_t error){
    switch(error){
    case ESP_OK:return HELP("","");
    case ESP_ERR_NO_MEM:return ax900_issue_help(AX900_ISSUE_MEMORY);
    case ESP_ERR_INVALID_STATE:return HELP("Adapter is busy or offline","Wait for scanning to finish. Disconnect before choosing another network.");
    case ESP_ERR_INVALID_ARG:return HELP("Connection settings incomplete","Check the login fields and certificate settings, then try again.");
    case ESP_ERR_NOT_SUPPORTED:return HELP("Network settings not supported","Use WPA2/CCMP without required PMF; personal passwords need 8-63 characters or 64 hex digits.");
    case ESP_ERR_NOT_FOUND:return HELP("Saved login is unavailable","Enter the network credentials again and connect to save them.");
    default:return HELP("Request could not start","Retry after the adapter is ready. Check local diagnostics if it repeats.");
    }
}
ax900_help_t ax900_test_help(const ax900_test_result_t *r,esp_err_t start){
    esp_err_t error=start?start:r?r->error:ESP_OK;
    if(error==ESP_ERR_NO_MEM)return ax900_issue_help(AX900_ISSUE_MEMORY);
    if(start==ESP_ERR_INVALID_ARG)return HELP("Test target is invalid","Open Target; enter a host, port 1-65535 and an HTTP path starting with /.");
    if(start==ESP_ERR_INVALID_STATE)return HELP("Test is busy or not connected","Wait for an IP address and for the previous test to finish, then retry.");
    if(start==ESP_ERR_NOT_SUPPORTED || start==ESP_ERR_NOT_FOUND)return HELP("DNS server is unavailable","Open Target and enter a DNS IPv4 reachable from this Wi-Fi network.");
    if(!r)return HELP("Test could not start","Reconnect and retry; check local diagnostics if the error repeats.");
    if(r->state==AX900_TEST_STALE)return HELP("Connection changed","Run a new test on the current connection.");
    if(r->state==AX900_TEST_CANCELLED)return HELP("Test cancelled","Select Run test when ready.");
    if(r->state!=AX900_TEST_FAILED && !start)return HELP("","");
    if(r->stage==AX900_TEST_STAGE_DNS)return HELP("DNS lookup failed","Check the host spelling and DNS address in Target; verify the DNS server is reachable.");
    if(r->stage==AX900_TEST_STAGE_INTERFACE)return HELP("Network is not ready","Wait for an IP address; reconnect if no address is assigned.");
    if(r->socket_error==ECONNREFUSED)return HELP("Target refused the connection","Check the port and start the service on the target computer.");
    if(r->stage==AX900_TEST_STAGE_CONNECT)return HELP("Target could not be reached","Check host, port, firewall and Wi-Fi client isolation, then retry.");
    if(r->kind==AX900_TEST_HTTP && r->http_status>=400)return HELP("HTTP server returned an error","Check the HTTP status below and the target path or server configuration.");
    if(r->kind==AX900_TEST_HTTP && r->stage==AX900_TEST_STAGE_RESPONSE)return HELP("No complete HTTP response","Use a plain HTTP service and port; this test does not support HTTPS.");
    if(r->kind==AX900_TEST_UDP)return HELP("UDP replies were lost","Check signal, firewall and the LAN test peer; retry on a stable connection.");
    if(r->stage==AX900_TEST_STAGE_TRANSFER || r->stage==AX900_TEST_STAGE_RESPONSE)
        return HELP(error==ESP_ERR_TIMEOUT?"Data transfer timed out":"Data transfer failed","Start the matching LAN test peer, check signal and retry.");
    return HELP("Test socket is unavailable","Close other network tasks and retry; restart Tab5 if it persists.");
}
