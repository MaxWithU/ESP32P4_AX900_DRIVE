// SPDX-License-Identifier: Apache-2.0
// Exercise the production protected-storage branch with synthetic NVS keys.
#define main profile_regression_main
#include "profile_test.c"
#undef main
#include "../components/ax900/ax900_profile.c"
static bool flash_enabled, missing_partition;
static esp_partition_t key_partition;
static esp_err_t stats_result=ESP_ERR_NVS_NOT_INITIALIZED, read_result=ESP_OK, init_result=ESP_OK;
static unsigned secure_inits;
bool esp_flash_encryption_enabled(void){return flash_enabled;}
const esp_partition_t *esp_partition_find_first(int type,int sub,const char *name){
    assert(type==ESP_PARTITION_TYPE_DATA && sub==ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS && !strcmp(name,"nvs_keys"));
    return missing_partition?NULL:&key_partition;
}
esp_err_t nvs_get_stats(const char *name,nvs_stats_t *stats){assert(!strcmp(name,"ax900_nvs"));(void)stats;return stats_result;}
esp_err_t nvs_flash_read_security_cfg(const esp_partition_t *part,nvs_sec_cfg_t *config){assert(part==&key_partition);memset(config,0xa5,sizeof(*config));return read_result;}
esp_err_t nvs_flash_secure_init_partition(const char *name,nvs_sec_cfg_t *config){
    assert(!strcmp(name,"ax900_nvs") && config->eky[0]==0xa5);secure_inits++;return init_result;
}
esp_err_t nvs_open_from_partition(const char *part,const char *name,int mode,nvs_handle_t *handle){assert(!strcmp(part,"ax900_nvs"));return nvs_open(name,mode,handle);}
int main(void){
    assert(ax_profile_init()==ESP_ERR_NOT_SUPPORTED && !ax900_profiles_encrypted());
    flash_enabled=true;stats_result=ESP_OK;
    assert(ax_profile_init()==ESP_ERR_INVALID_STATE && !ax900_profiles_encrypted());
    stats_result=ESP_ERR_NVS_NOT_INITIALIZED;missing_partition=true;
    assert(ax_profile_init()==ESP_ERR_NOT_FOUND);
    missing_partition=false;assert(ax_profile_init()==ESP_ERR_NOT_FOUND);
    key_partition.encrypted=true;read_result=ESP_FAIL;
    assert(ax_profile_init()==ESP_FAIL && !secure_inits && !ax900_profiles_encrypted());
    read_result=ESP_OK;init_result=ESP_FAIL;
    assert(ax_profile_init()==ESP_FAIL && !ax900_profiles_encrypted());
    init_result=ESP_OK;assert(ax_profile_init()==ESP_OK && ax900_profiles_encrypted());
    unsigned before=secure_inits;assert(ax_profile_init()==ESP_OK && secure_inits==before);
    profile_regression_main();
    puts("Protected profiles reject unprotected/preopened stores and key/init failures; no fallback");
}
