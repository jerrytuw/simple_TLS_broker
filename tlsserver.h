#include "esp_http_server.h"
#include "esp_httpd_priv.h"

//#define TLS_DEBUG

#ifdef TLS_DEBUG
#define debug(...) { printf (__VA_ARGS__);}
#else
#define debug(...) {}
#endif

typedef httpd_handle_t tls_handle_t;

#ifdef __cplusplus
extern "C" {
#endif
int writeData(struct sock_db *session, char* buf, size_t buflen);
int readData(struct sock_db *session, char* buf, size_t buflen);
struct sock_db * checkAvail(struct httpd_data *hd);
bool canReadData(struct sock_db * session);
#ifdef __cplusplus
}
#endif
