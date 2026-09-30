// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "ax900_issue.h"
int main(void){
    ax_issue_set(AX900_ISSUE_CLOCK);ax_issue_if_clear(AX900_ISSUE_AUTHENTICATION);
    assert(ax900_get_connection_issue()==AX900_ISSUE_CLOCK);
    ax900_link_status_t s={.present=true,.ready=true};
    assert(strstr(ax900_connection_help(&s).title,"time"));
    ax_issue_set(AX900_ISSUE_NONE);ax_issue_if_clear(AX900_ISSUE_AUTHENTICATION);
    assert(!strstr(ax900_connection_help(&s).title,"password"));
    s.has_ip=true;assert(!ax900_connection_help(&s).title[0]);
    s.profile_error=ESP_FAIL;assert(strstr(ax900_connection_help(&s).title,"saved"));
    s.present=false;assert(strstr(ax900_connection_help(&s).action,"USB-A"));
    assert(strstr(ax900_test_help(NULL,ESP_ERR_NO_MEM).action,"restart"));
    assert(strstr(ax900_test_help(NULL,ESP_ERR_INVALID_ARG).action,"Target"));
    assert(strstr(ax900_test_help(NULL,ESP_ERR_NOT_SUPPORTED).action,"DNS IPv4"));
    ax900_test_result_t t={.state=AX900_TEST_FAILED,.stage=AX900_TEST_STAGE_DNS,.error=ESP_ERR_TIMEOUT};
    assert(strstr(ax900_test_help(&t,ESP_OK).title,"DNS"));
    t.stage=AX900_TEST_STAGE_CONNECT;t.socket_error=ECONNREFUSED;
    assert(strstr(ax900_test_help(&t,ESP_OK).title,"refused"));
    t.socket_error=0;t.kind=AX900_TEST_HTTP;t.stage=AX900_TEST_STAGE_RESPONSE;t.http_status=503;
    assert(strstr(ax900_test_help(&t,ESP_OK).title,"server"));
    t.http_status=0;assert(strstr(ax900_test_help(&t,ESP_OK).action,"HTTPS"));
    t.state=AX900_TEST_STALE;assert(strstr(ax900_test_help(&t,ESP_OK).title,"changed"));
    t.state=AX900_TEST_PASSED;assert(!ax900_test_help(&t,ESP_OK).title[0]);
    for(int i=AX900_ISSUE_ASSOCIATION;i<=AX900_ISSUE_CONNECTION;i++){
        ax900_help_t h=ax900_issue_help(i);assert(h.title[0] && h.action[0] && strlen(h.action)<=140);
    }
    puts("Help: precise causes survive generic failures; actionable network/test advice passed");
}
