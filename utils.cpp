// some utilities

#include "utils.h"

char* html_escape(const char* src) {
  //Primitive html attribute escape, should handle most common issues.
  int len = strlen(src);
  //Every char in the string + a null
  int esc_len = len + 1;
  //printf("escape before: %s\n",src);

  for (int i = 0; i < len; i++) {
    if (src[i] == '\\' || src[i] == '\'' || src[i] == '\"' || src[i] == '&' || src[i] == '#' || src[i] == ';') {
      //Will be replaced with a 5 char sequence
      esc_len += 4;
    }
  }

  char* res = (char *)malloc(sizeof(char) * esc_len);

  int j = 0;
  for (int i = 0; i < len; i++) {
    if (src[i] == '\\' || src[i] == '\'' || src[i] == '\"' || src[i] == '&' || src[i] == '#' || src[i] == ';') {
      res[j++] = '&';
      res[j++] = '#';
      res[j++] = '0' + (src[i] / 10);
      res[j++] = '0' + (src[i] % 10);
      res[j++] = ';';
    }
    else {
      res[j++] = src[i];
    }
  }
  res[j] = '\0';
  //printf("escape after: %s\n",res);
  return res;
}

void preprocess_string(char* str)
{
  char *p, *q;

  for (p = q = str; *p != 0; p++)
  {
    if (*(p) == '%' && *(p + 1) != 0 && *(p + 2) != 0)
    {
      // quoted hex
      uint8_t a;
      p++;
      if (*p <= '9')
        a = *p - '0';
      else
        a = toupper((unsigned char) * p) - 'A' + 10;
      a <<= 4;
      p++;
      if (*p <= '9')
        a += *p - '0';
      else
        a += toupper((unsigned char) * p) - 'A' + 10;
      *q++ = a;
    }
    else if (*(p) == '+') {
      *q++ = ' ';
    } else {
      *q++ = *p;
    }
  }
  *q = '\0';
}

int set_config_param_str(const char* name, const char* param, char* target)
{
  esp_err_t err;
  nvs_handle nvs;

  //printf("pre before: %s\n",param);
  //preprocess_string(param);
  //printf("pre after: %s\n",param);
  err = nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs);
  if (err != ESP_OK) {
    return err;
  }

  err = nvs_set_str(nvs, name, param);
  if (err == ESP_OK) {
    printf("nvs %s=%s stored.\n", name, param);
  }
  nvs_close(nvs);
  strncpy(target, param, PARAMLEN);
  return err;
}

esp_err_t get_config_param_str(const char* name, char* param)
{
  nvs_handle nvs;

  esp_err_t err = nvs_open(PARAM_NAMESPACE, NVS_READONLY, &nvs);
  if (err == ESP_OK) {
    size_t len;
    if ( (err = nvs_get_str(nvs, name, NULL, &len)) == ESP_OK) {
      //      param = (char *)malloc(len);
      err = nvs_get_str(nvs, name, param, &len);
      printf("nvs: %s=%s\n", name, param);
    } else {
      return err;
    }
    nvs_close(nvs);
  } else {
    return err;
  }
  return ESP_OK;
}

void param_set_default (const char* def_val, char* goal)
{
  strncpy(goal, def_val, 50);
}

esp_err_t query_key_value(const char *in, const char* pattern, char* target, int size)
{
  char *p;
  if ((p = strstr(in, pattern)))
  {
    if ((p = strstr(p, "=")))
    {
      p++;
      int i;
      for (i = 0; i < size - 1; i++)
      {
        if (*p == 0 || *p == '&' || *p == ' ') break;
        target[i] = *p++;
      }
      target[i] = 0;
      return ESP_OK;
    }
  }
  return -1;
}

void initialize_nvs(void)
{
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK( nvs_flash_erase() );
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
}
