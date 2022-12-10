/*
   SPDX-FileCopyrightText: 2018-2021 Espressif Systems (Shanghai) CO LTD

   SPDX-License-Identifier: Apache-2.0
*/

#include <string.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <esp_log.h>
#include <esp_err.h>
#include <assert.h>

#include <esp_http_server.h>
#include "esp_httpd_priv.h"

typedef struct {
  fd_set *fdset;
  struct httpd_data *hd;
} process_session_context_t;

static const char *TAG = "httpd";
#define IPAddress unsigned long
IPAddress ip;

static esp_err_t httpd_accept_conn(struct httpd_data *hd, int listen_fd, struct sock_db **newsession)
{
  //printf("new TLS connection\n");
  /* If no space is available for new session, close the least recently used one */
  if (hd->config.lru_purge_enable == true) {
    if (!httpd_is_sess_available(hd)) {
      /* Queue asynchronous closure of the least recently used session */
      return httpd_sess_close_lru(hd);
      /* Returning from this allowes the main server thread to process
         the queued asynchronous control message for closing LRU session.
         Since connection request hasn't been addressed yet using accept()
         therefore httpd_accept_conn() will be called again, but this time
         with space available for one session
      */
    }
  }

  struct sockaddr_in addr_from;
  socklen_t addr_from_len = sizeof(addr_from);
  int new_fd = accept(listen_fd, (struct sockaddr *)&addr_from, &addr_from_len);
  if (new_fd < 0) {
    ESP_LOGW(TAG, LOG_FMT("error in accept (%d)"), errno);
    return ESP_FAIL;
  }
  ESP_LOGD(TAG, LOG_FMT("newfd = %d"), new_fd);

  struct timeval tv;
  /* Set recv timeout of this fd as per config ???*/
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  setsockopt(new_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));

  /* Set send timeout of this fd as per config */
  tv.tv_sec = hd->config.send_wait_timeout;
  tv.tv_usec = 0;
  int val = 1;
  setsockopt(new_fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));
  setsockopt(new_fd, SOL_SOCKET, SO_KEEPALIVE, (char*)&val, sizeof(int));
  int status = fcntl(new_fd, F_SETFL, fcntl(new_fd, F_GETFL, 0) | O_NONBLOCK); // set nonblocking
  if (ESP_OK != httpd_sess_new(hd, new_fd, newsession)) {
    printf("session creation failed\n");
    close(new_fd);
    *newsession = NULL;
    return ESP_FAIL;
  }
  printf("TLS session accepted\n");
  return ESP_OK;
}

esp_err_t httpd_queue_work(httpd_handle_t handle, httpd_work_fn_t work, void *arg)
{
  return ESP_OK;
}

esp_err_t httpd_get_client_list(httpd_handle_t handle, size_t *fds, int *client_fds)
{
  struct httpd_data *hd = (struct httpd_data *) handle;
  if (hd == NULL || fds == NULL || *fds == 0 || client_fds == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  size_t max_fds = *fds;
  *fds = 0;
  for (int i = 0; i < hd->config.max_open_sockets; ++i) {
    if (hd->hd_sd[i].fd != -1) {
      if (*fds < max_fds) {
        client_fds[(*fds)++] = hd->hd_sd[i].fd;
      } else {
        return ESP_ERR_INVALID_ARG;
      }
    }
  }
  return ESP_OK;
}

void *httpd_get_global_user_ctx(httpd_handle_t handle)
{
  return ((struct httpd_data *)handle)->config.global_user_ctx;
}

void *httpd_get_global_transport_ctx(httpd_handle_t handle)
{
  return ((struct httpd_data *)handle)->config.global_transport_ctx;
}


// Called for each session from httpd_server
static int httpd_process_session(struct sock_db *session, void *context)
{
  if ((!session) || (!context)) {
    return 0;
  }

  if (session->fd < 0) {
    return 1;
  }
  printf("processsession\n");
  process_session_context_t *ctx = (process_session_context_t *)context;
  int fd = session->fd;

  if (FD_ISSET(fd, ctx->fdset) || httpd_sess_pending(ctx->hd, session)) {
    printf("processing socket %d\n", fd);
    if (httpd_sess_process(ctx->hd, session) != ESP_OK) {
      printf("delete session\n");
      httpd_sess_delete(ctx->hd, session); // Delete session
    }
  }
  return 1;
}
//MY functions
void process(struct httpd_data *hd, struct sock_db *session) {
  if (httpd_sess_process(hd, session) != ESP_OK) {
    printf("not OK process\n");
    httpd_sess_delete(hd, session); // Delete session
  }
}
// check if listen socket was connected for new connection
struct sock_db * checkAvail(struct httpd_data *hd)
{
  //printf("avail\n");
  struct sock_db *newsession = NULL;
  fd_set read_set;
  FD_ZERO(&read_set);
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 10000;

  if (hd->config.lru_purge_enable || httpd_is_sess_available(hd)) {
    /* Only listen for new connections if server has capacity to
       handle more (or when LRU purge is enabled, in which case
       older connections will be closed) */
    FD_SET(hd->listen_fd, &read_set);
  }
  int tmp_max_fd;
  httpd_sess_set_descriptors(hd, &read_set, &tmp_max_fd); ////seems to be necessary???
  int maxfd = MAX(hd->listen_fd, tmp_max_fd);
  tmp_max_fd = maxfd;
  //printf("maxfd %d\n",maxfd);

  int active_cnt = select(maxfd + 1, &read_set, NULL, NULL, &timeout);
  if (active_cnt <= 0) return NULL;
  //printf("active=%d max=%d\n", active_cnt, maxfd);

  if (FD_ISSET(hd->listen_fd, &read_set)) {
    //printf("processing listen socket %d\n", hd->listen_fd);
    if (httpd_accept_conn(hd, hd->listen_fd, &newsession) != ESP_OK) {
      printf("error accepting new connection\n");
    }
    else
    {
      printf("new broker connection\n");
      return newsession;
    }
  }
  return NULL;
}

// checks if there are any (TLS) bytes waiting in TLS buffer
bool pendingData(struct sock_db *session)
{
  if (!session) {
    return false;
  }
  struct httpd_data * hd = session->handle;
  if (!hd) {
    return false;
  }

  return httpd_sess_pending(hd, session);
}

// checks if there are any (TLS) bytes waiting on socket
int peekData(struct sock_db *session)
{
  if (!session) {
    return false;
  }
  struct httpd_data * hd = session->handle;
  if (!hd) {
    return false;
  }

  int count = 0;
  ioctl(session->fd, FIONREAD, &count);
  if (count > 0)
    printf("%d socket bytes ready\n", count);
  return count;
}

// reads bytes in TLS buffer for socket
int readData(struct sock_db *session, char* buf, size_t buflen)
{
  if (!session) {
    return false;
  }
  struct httpd_data * hd = session->handle;
  if (!hd) {
    return false;
  }

  int ret = session->recv_fn(session->handle, session->fd, buf, buflen, 0);
  if (ret < 0) {
    printf("my error in readData on socket %d", session->fd);
    httpd_sess_delete(hd, session);
    return -1;
  }
  else
  {
    /*    for (int i = 0; i < ret; i++)
        {
          printf("%02x", buf[i]);
          fflush(stdout);
        }*/
  }
  return ret;
}

// reads bytes in TLS buffer for socket
int writeData(struct sock_db *session, char* buf, size_t buflen)
{
  if (!session) {
    printf("invalid session\n");
    return false;
  }
  struct httpd_data * hd = session->handle;
  if (!hd) {
    printf("invalid hd\n");
    return false;
  }

  int ret = session->send_fn(session->handle, session->fd, buf, buflen, 0);
  if (ret < 0) {
    printf("my error in sendData on socket %d\n", session->fd);
    httpd_sess_delete(hd, session);
    return -1;
  }
  else if (ret == 0) {
    printf("nothing written\n");
  }
  else
  {
    /*    for (int i = 0; i < ret; i++)
        {
          printf("%02x", buf[i]);
          fflush(stdout);
        }*/
  }
  return ret;
}
bool canReadData(struct sock_db * session)
{
  fd_set read_set;
  FD_ZERO(&read_set);
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;

  if (!session) {
    printf("canReadData zero session\n");
    return false;
  }
  struct httpd_data * hd = session->handle;
  if (!hd) {
    printf("canReadData zero hd\n");
    return false;
  }

  if (session->fd < 0) {
    printf("canReadData invalid fd\n");
    return false;
  }
  FD_SET(session->fd, &read_set);
  int active_cnt = select((session->fd) + 1, &read_set, NULL, NULL, &tv);
  //printf("active=%d max=%d\n", active_cnt, maxfd);

  if (FD_ISSET(session->fd, &read_set) || httpd_sess_pending(hd, session)) {
    //printf("data on socket %d\n", session->fd);
    return true;
  }
  return false;
}

/* Manage in-coming connection or data requests */
esp_err_t httpd_server(struct httpd_data * hd)
{
  struct sock_db *newsession = NULL;

  fd_set read_set;
  FD_ZERO(&read_set);
  if (hd->config.lru_purge_enable || httpd_is_sess_available(hd)) {
    /* Only listen for new connections if server has capacity to
       handle more (or when LRU purge is enabled, in which case
       older connections will be closed) */
    FD_SET(hd->listen_fd, &read_set);
  }

  int tmp_max_fd;
  httpd_sess_set_descriptors(hd, &read_set, &tmp_max_fd);
  int maxfd = MAX(hd->listen_fd, tmp_max_fd);
  tmp_max_fd = maxfd;
  //printf("server select\n");
  ESP_LOGD(TAG, LOG_FMT("doing select maxfd+1 = %d"), maxfd + 1);
  int active_cnt = select(maxfd + 1, &read_set, NULL, NULL, NULL);
  //printf("server select after\n");
  if (active_cnt < 0) {
    printf("error in select (%d)\n", errno);
    httpd_sess_delete_invalid(hd);
    return ESP_OK;
  }

  /* Case0: Do we have a control message? */

  //newsession = checkAvail(hd);

  /* Case1: Do we have any activity on the current data
     sessions? */
  //sess_enum(hd);
  process_session_context_t context = {
    .fdset = &read_set,
    .hd = hd
  };
  httpd_sess_enum(hd, httpd_process_session, &context);
  /* Case2: Do we have any incoming connection requests to
     process? */
  /*if (FD_ISSET(hd->listen_fd, &read_set)) {
    printf("processing listen socket %d\n", hd->listen_fd);
    if (httpd_accept_conn(hd, hd->listen_fd, &newsession) != ESP_OK) {
      printf("error accepting new connection\n");
    }
    else printf("new connection\n");
    }*/

  return ESP_OK;
}

/* The main HTTPD thread */
static void httpd_thread(void *arg)
{
  int ret;
  struct httpd_data *hd = (struct httpd_data *) arg;
  hd->hd_td.status = THREAD_RUNNING;

  //printf("web server thread started\n");
  while (1) {
    vTaskDelay(500);
    /*ret = httpd_server(hd);
      if (ret != ESP_OK) {
      break;
      }*/
  }

  ESP_LOGD(TAG, LOG_FMT("web server exiting"));
  close(hd->msg_fd);
  httpd_sess_close_all(hd);
  close(hd->listen_fd);
  hd->hd_td.status = THREAD_STOPPED;
  httpd_os_thread_delete();
}

static esp_err_t httpd_server_init(struct httpd_data * hd)
{
  int fd = socket(PF_INET, SOCK_STREAM, 0);

  if (fd < 0) {
    printf("error in initial socket (%d)\n", errno);
    return ESP_FAIL;
  }

  struct sockaddr_in serv_addr = {
    .sin_family   = PF_INET,
    .sin_addr     = {
      .s_addr = ip
    },
    .sin_port     = htons(hd->config.server_port)
  };


  /* Enable SO_REUSEADDR to allow binding to the same
     address and port when restarting the server */
  int enable = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
    /* This will fail if CONFIG_LWIP_SO_REUSE is not enabled. But
       it does not affect the normal working of the HTTP Server */
    ESP_LOGW(TAG, LOG_FMT("error enabling SO_REUSEADDR (%d)"), errno);
  }

  int ret = bind(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  if (ret < 0) {
    ESP_LOGE(TAG, LOG_FMT("error in bind (%d)"), errno);
    close(fd);
    return ESP_FAIL;
  }

  ret = listen(fd, hd->config.backlog_conn);
  if (ret < 0) {
    ESP_LOGE(TAG, LOG_FMT("error in listen (%d)"), errno);
    close(fd);
    return ESP_FAIL;
  }

  int msg_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (msg_fd < 0) {
    ESP_LOGE(TAG, LOG_FMT("error in creating msg socket (%d)"), errno);
    close(fd);
    return ESP_FAIL;
  }

  hd->listen_fd = fd;
  hd->msg_fd  = msg_fd;
  return ESP_OK;
}

static struct httpd_data *httpd_create(const httpd_config_t *config)
{
  /* Allocate memory for httpd instance data */
  struct httpd_data *hd = calloc(1, sizeof(struct httpd_data));
  if (!hd) {
    ESP_LOGE(TAG, LOG_FMT("Failed to allocate memory for HTTP server instance"));
    return NULL;
  }
  hd->hd_calls = calloc(config->max_uri_handlers, sizeof(httpd_uri_t *));
  if (!hd->hd_calls) {
    ESP_LOGE(TAG, LOG_FMT("Failed to allocate memory for HTTP URI handlers"));
    free(hd);
    return NULL;
  }
  hd->hd_sd = calloc(config->max_open_sockets, sizeof(struct sock_db));
  if (!hd->hd_sd) {
    ESP_LOGE(TAG, LOG_FMT("Failed to allocate memory for HTTP session data"));
    free(hd->hd_calls);
    free(hd);
    return NULL;
  }
  struct httpd_req_aux *ra = &hd->hd_req_aux;
  ra->resp_hdrs = calloc(config->max_resp_headers, sizeof(struct resp_hdr));
  if (!ra->resp_hdrs) {
    ESP_LOGE(TAG, LOG_FMT("Failed to allocate memory for HTTP response headers"));
    free(hd->hd_sd);
    free(hd->hd_calls);
    free(hd);
    return NULL;
  }
  hd->err_handler_fns = calloc(HTTPD_ERR_CODE_MAX, sizeof(httpd_err_handler_func_t));
  if (!hd->err_handler_fns) {
    ESP_LOGE(TAG, LOG_FMT("Failed to allocate memory for HTTP error handlers"));
    free(ra->resp_hdrs);
    free(hd->hd_sd);
    free(hd->hd_calls);
    free(hd);
    return NULL;
  }
  /* Save the configuration for this instance */
  hd->config = *config;
  return hd;
}

static void httpd_delete(struct httpd_data * hd)
{
  struct httpd_req_aux *ra = &hd->hd_req_aux;
  /* Free memory of httpd instance data */
  free(hd->err_handler_fns);
  free(ra->resp_hdrs);
  free(hd->hd_sd);

  /* Free registered URI handlers */
  free(hd->hd_calls);
  free(hd);
}

esp_err_t httpd_start(httpd_handle_t *handle, const httpd_config_t *config)
{
  //printf("starting httpd\n");
  if (handle == NULL || config == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  /* Sanity check about whether LWIP is configured for providing the
     maximum number of open sockets sufficient for the server. Though,
     this check doesn't guarantee that many sockets will actually be
     available at runtime as other processes may use up some sockets.
     Note that server also uses 3 sockets for its internal use :
         1) listening for new TCP connections
         2) for sending control messages over UDP
         3) for receiving control messages over UDP
     So the total number of required sockets is max_open_sockets + 3
  */
  if (CONFIG_LWIP_MAX_SOCKETS < config->max_open_sockets + 3) {
    ESP_LOGE(TAG, "Configuration option max_open_sockets is too large (max allowed %d)\n\t"
             "Either decrease this or configure LWIP_MAX_SOCKETS to a larger value",
             CONFIG_LWIP_MAX_SOCKETS - 3);
    return ESP_ERR_INVALID_ARG;
  }

  struct httpd_data *hd = httpd_create(config);
  if (hd == NULL) {
    /* Failed to allocate memory */
    return ESP_ERR_HTTPD_ALLOC_MEM;
  }

  if (httpd_server_init(hd) != ESP_OK) {
    httpd_delete(hd);
    return ESP_FAIL;
  }

  httpd_sess_init(hd);
  if (httpd_os_thread_create(&hd->hd_td.handle, "httpd",
                             hd->config.stack_size,
                             hd->config.task_priority,
                             httpd_thread, hd,
                             hd->config.core_id) != ESP_OK) {
    //* Failed to launch task
    httpd_delete(hd);
    return ESP_ERR_HTTPD_TASK;
  }

  *handle = (httpd_handle_t *)hd;
  return ESP_OK;
}

esp_err_t httpd_stop(httpd_handle_t handle)
{
  struct httpd_data *hd = (struct httpd_data *) handle;
  if (hd == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  ESP_LOGD(TAG, LOG_FMT("sent control msg to stop server"));
  while (hd->hd_td.status != THREAD_STOPPED) {
    httpd_os_thread_sleep(100);
  }

  /* Release global user context, if not NULL */
  if (hd->config.global_user_ctx) {
    if (hd->config.global_user_ctx_free_fn) {
      hd->config.global_user_ctx_free_fn(hd->config.global_user_ctx);
    } else {
      free(hd->config.global_user_ctx);
    }
    hd->config.global_user_ctx = NULL;
  }

  /* Release global transport context, if not NULL */
  if (hd->config.global_transport_ctx) {
    if (hd->config.global_transport_ctx_free_fn) {
      hd->config.global_transport_ctx_free_fn(hd->config.global_transport_ctx);
    } else {
      free(hd->config.global_transport_ctx);
    }
    hd->config.global_transport_ctx = NULL;
  }

  ESP_LOGD(TAG, LOG_FMT("server stopped"));
  httpd_delete(hd);
  return ESP_OK;
}

// loop over all sessions and read data
void sess_enum(struct httpd_data * hd)
{
  char buf[128];
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;

  if ((!hd) || (!hd->hd_sd) || (!hd->config.max_open_sockets)) {
    return;
  }
  fd_set read_set;
  FD_ZERO(&read_set);

  process_session_context_t context = {
    .fdset = &read_set,
    .hd = hd
  };
  struct sock_db *current = hd->hd_sd;
  struct sock_db *end = hd->hd_sd + hd->config.max_open_sockets - 1;

  while (current <= end) {
    //bool data = canReadData(current);
    //    bool x = httpd_sess_pending(hd, current);
    //    if (x) printf("---pending \n");

    while (canReadData(current))
    {
      int ret = current->recv_fn(current->handle, current->fd, buf, 128, 0);
      if (ret < 0) {
        printf("my error in recv_fn %d", current->fd);
        httpd_sess_delete(hd, current);
        return;
      }
      else
      {
        printf("%d bytes read:\n", ret);
        for (int i = 0; i < ret; i++)
        {
          printf("%c", buf[i]);
        }
        printf("\n");
      }
      if (ret <= 0) break;
    }
    current++;
  }
  httpd_sess_delete_invalid(hd);
}
// loop over all sessions and read data
void xsess_enum(struct httpd_data * hd)
{
  char buf[128];
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;

  if ((!hd) || (!hd->hd_sd) || (!hd->config.max_open_sockets)) {
    return;
  }
  struct sock_db *current = hd->hd_sd;
  struct sock_db *end = hd->hd_sd + hd->config.max_open_sockets - 1;

  while (current <= end) {
    //bool data = canReadData(current);
    //    bool x = httpd_sess_pending(hd, current);
    //    if (x) printf("---pending \n");

    while ((peekData(current) > 0) || pendingData(current))
    {
      int ret = readData(current, buf, 128);
      if (ret < 0) {
        printf("my error in recv_fn %d", current->fd);
        httpd_sess_delete(hd, current);
        return;
      }
      else
      {
        printf("%d bytes read:\n", ret);
        for (int i = 0; i < ret; i++)
        {
          printf("%c", buf[i]);
        }
        printf("\n");
      }
      if (ret <= 0) break;
    }
    current++;
  }
  httpd_sess_delete_invalid(hd);
}
