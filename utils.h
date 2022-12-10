#include <WiFi.h>
#include "nvs.h"
#include "nvs_flash.h"
#define PARAM_NAMESPACE "esp32_nat"
#define PARAMLEN 50

char* html_escape(const char* src);
void preprocess_string(char* str);
int set_config_param_str(const char* name, const char* param, char *target);
esp_err_t get_config_param_str(const char* name, char* param);
void param_set_default (const char* def_val, char* goal);
void initialize_nvs(void);
esp_err_t query_key_value(const char *in, const char* pattern, char* target, int size);
