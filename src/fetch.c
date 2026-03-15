/* fetch.c — Threaded non-blocking HTTP GET via libcurl + pthread.
 *
 * Each request runs in a joinable thread so shutdown can wait for
 * completion.  A mutex protects the shared status/response fields;
 * the main loop polls fetch_check() each frame and takes ownership
 * of the response when ready. */

#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "fetch.h"

/* Growable buffer for accumulating curl response data. */
typedef struct {
    char   *data;
    size_t  len;
    size_t  cap;
} Buffer;

/* libcurl write callback: appends received data to the growable buffer. */
static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    Buffer *buf = userdata;
    if (size != 0 && nmemb > (size_t)-1 / size) return 0;
    size_t total = size * nmemb;
    if (total > (size_t)-1 - buf->len - 1) return 0;
    if (buf->len + total + 1 > buf->cap) {
        size_t newcap = buf->cap + total + 1;
        if (newcap > (size_t)-1 / 2) return 0;
        newcap *= 2;
        char *p = realloc(buf->data, newcap);
        if (!p) return 0;   /* signal error to curl */
        buf->data = p;
        buf->cap = newcap;
    }
    memcpy(buf->data + buf->len, ptr, total);
    buf->len += total;
    buf->data[buf->len] = '\0';
    return total;
}

/* Thread entry point: performs the HTTP GET and stores result under mutex. */
static void *fetch_thread(void *arg)
{
    FetchRequest *req = arg;
    CURL *curl = curl_easy_init();
    if (!curl) {
        pthread_mutex_lock(&req->mutex);
        req->status = -1;
        pthread_mutex_unlock(&req->mutex);
        return NULL;
    }

    Buffer buf = { .data = malloc(4096), .len = 0, .cap = 4096 };
    if (!buf.data) {
        curl_easy_cleanup(curl);
        pthread_mutex_lock(&req->mutex);
        req->status = -1;
        pthread_mutex_unlock(&req->mutex);
        return NULL;
    }
    buf.data[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, req->url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "azMap/1.0");
    /* Enforce TLS verification explicitly */
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    /* Restrict to HTTP/HTTPS only */
#if LIBCURL_VERSION_NUM >= 0x075500  /* 7.85.0 */
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    pthread_mutex_lock(&req->mutex);
    if (res == CURLE_OK) {
        req->response = buf.data;
        req->response_len = buf.len;
        req->status = 1;
    } else {
        free(buf.data);
        req->status = -1;
    }
    pthread_mutex_unlock(&req->mutex);
    return NULL;
}

/* Spawn a background thread to fetch the given URL.
 * If a previous fetch is still in-flight, join it first. */
void fetch_start(FetchRequest *req, const char *url)
{
    /* If a previous thread is running, wait for it to finish */
    if (req->thread_active) {
        pthread_join(req->thread, NULL);
        req->thread_active = 0;
        pthread_mutex_destroy(&req->mutex);
    }

    /* Clean up any leftover resources from previous request */
    free(req->url);
    free(req->response);

    req->url = NULL;
    req->response = NULL;
    req->response_len = 0;
    req->status = 0;

    /* Initialize mutex fresh */
    pthread_mutex_init(&req->mutex, NULL);

    req->url = strdup(url);
    if (!req->url) {
        pthread_mutex_destroy(&req->mutex);
        req->status = -1;
        return;
    }

    if (pthread_create(&req->thread, NULL, fetch_thread, req) != 0) {
        free(req->url);
        req->url = NULL;
        pthread_mutex_destroy(&req->mutex);
        req->status = -1;
        return;
    }
    req->thread_active = 1;
}

/* Poll status: 0=pending, 1=done, -1=error.  Lock-protected. */
int fetch_check(FetchRequest *req)
{
    pthread_mutex_lock(&req->mutex);
    int s = req->status;
    pthread_mutex_unlock(&req->mutex);
    return s;
}

/* Transfer ownership of the response string to the caller (who must free it). */
char *fetch_take_response(FetchRequest *req)
{
    pthread_mutex_lock(&req->mutex);
    char *r = req->response;
    req->response = NULL;
    pthread_mutex_unlock(&req->mutex);
    return r;
}

/* Join thread if active, release URL and response memory, destroy mutex.
 * Safe to call on a zero-initialized (never-started) FetchRequest. */
void fetch_cleanup(FetchRequest *req)
{
    if (req->thread_active) {
        pthread_join(req->thread, NULL);
        req->thread_active = 0;
        pthread_mutex_destroy(&req->mutex);
    }
    free(req->url);
    req->url = NULL;
    free(req->response);
    req->response = NULL;
}
