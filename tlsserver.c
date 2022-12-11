/* Simple TCP + SSL server based on stripped doen esp_https_server example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
/*
   NOTE: there are breaking changes in espidf 5.0! This is for 4.4.3
*/

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_http_server.h"
#include <esp_https_server.h>
#include "esp_tls.h"
#include "sdkconfig.h"

#include "cert.h"
#include "private_key.h"

#include "tlsserver.h"

#define IPAddress unsigned long
extern IPAddress ip;

#define CONFIG_EXAMPLE_ENABLE_HTTPS_USER_CALLBACK 1
#define CONFIG_ESP_TLS_USING_MBEDTLS 1

/* A simple server listening and providing the connections to upstream
*/

static const char *TAG = "TLS server";

#if CONFIG_EXAMPLE_ENABLE_HTTPS_USER_CALLBACK
/**
   Example callback function to get the certificate of connected clients,
   whenever a new SSL connection is created

   Can also be used to other information like Socket FD, Connection state, etc.

   NOTE: This callback will not be able to obtain the client certificate if the
   following config `Set minimum Certificate Verification mode to Optional` is
   not enabled (enabled by default in this example).

   The config option is found here - Component config → ESP-TLS

*/
void tls_server_user_callback(esp_https_server_user_cb_arg_t *user_cb)
{
  debug("Session Created! Socket fd: %d\n", user_cb->tls->sockfd);
  return;
  /*
    const mbedtls_x509_crt *cert;
    const size_t buf_size = 1024;
    char *buf = calloc(buf_size, sizeof(char));
    if (buf == NULL) {
     printf("Out of memory - Callback execution failed!\n");
     return;
    }

    mbedtls_x509_crt_info((char *) buf, buf_size - 1, "    ", &user_cb->tls->servercert);
    printf("Server certificate info:\n%s", buf);
    memset(buf, 0x00, buf_size);

    cert = mbedtls_ssl_get_peer_cert(&user_cb->tls->ssl);
    if (cert != NULL) {
     mbedtls_x509_crt_info((char *) buf, buf_size - 1, "    ", cert);
     printf("Peer certificate info:\n%s\n", buf);
    } else {
     printf("Could not obtain the peer certificate!\n");
    }

    free(buf);*/
}
#endif

tls_handle_t start_webserver(IPAddress ipaddr)
{
  unsigned long x = ipaddr;
  ip = x;

  tls_handle_t server = NULL;

  // Start the httpd server
  printf("Starting TLS server\n");

  httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();

  conf.cacert_pem = example_crt_DER;
  conf.cacert_len = sizeof(example_crt_DER);

  conf.prvtkey_pem = example_key_DER;
  conf.prvtkey_len = sizeof(example_key_DER);
  conf.port_secure = 8883;
#if CONFIG_EXAMPLE_ENABLE_HTTPS_USER_CALLBACK
  conf.user_cb = tls_server_user_callback;
#endif
  esp_err_t ret = httpd_ssl_start(&server, &conf);
  if (ESP_OK != ret) {
    printf("Error starting server!\n");
    return NULL;
  }
  return server;
}

static esp_err_t stop_webserver(tls_handle_t server)
{
  // Stop the httpd server
  httpd_ssl_stop(server);
  return;
}
