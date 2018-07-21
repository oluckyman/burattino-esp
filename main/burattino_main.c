#include "freertos/FreeRTOS.h"
/* #include "esp_wifi.h" */
#include "esp_log.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/api.h"
#include "wifi_connect.h"

/* FreeRTOS event group to signal when we are connected and ready to use wifi */
static EventGroupHandle_t event_group;

static const char *TAG = "bur";

static void http_server_netconn_serve(struct netconn *conn)
{
    struct netbuf *inbuf;
    char *buf;
    u16_t buflen;
    err_t err;

    /* Read the data from the port, blocking if nothing yet there.
       We assume the request (the part we care about) is in one netbuf */
    err = netconn_recv(conn, &inbuf);

    char http_html_hdr[] = "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";

    if (err == ERR_OK) {
        netbuf_data(inbuf, (void**)&buf, &buflen);

        /* Is this an HTTP GET command? (only check the first 5 chars, since
           there are other formats for GET, and we're keeping it very simple )*/
        printf("buffer = %s \n", buf);
        if (buflen>=5 &&
                buf[0]=='G' &&
                buf[1]=='E' &&
                buf[2]=='T' &&
                buf[3]==' ' &&
                buf[4]=='/' ) {
            printf("buf[5] = %c\n", buf[5]);
            /* Send the HTML header
             * subtract 1 from the size, since we dont send the \0 in the string
             * NETCONN_NOCOPY: our data is const static, so no need to copy it
             */

            netconn_write(conn, http_html_hdr, sizeof(http_html_hdr)-1, NETCONN_NOCOPY);

            char http_index_html[] = "boo";
            netconn_write(conn, http_index_html, sizeof(http_index_html)-1, NETCONN_NOCOPY);
        }
    }
    /* Close the connection (server closes in HTTP) */
    netconn_close(conn);

    /* Delete the buffer (netconn_recv gives us ownership,
       so we have to make sure to deallocate the buffer) */
    netbuf_delete(inbuf);
}

static void http_server_task(void *pvParameters)
{
    struct netconn *conn, *newconn;
    err_t err;
    conn = netconn_new(NETCONN_TCP);
    netconn_bind(conn, NULL, 80);
    netconn_listen(conn);
    ESP_LOGI(TAG, "start webserver");
    do {
        err = netconn_accept(conn, &newconn);
        ESP_LOGI(TAG, "got connection");
        if (err == ERR_OK) {
            http_server_netconn_serve(newconn);
            netconn_delete(newconn);
        }
    } while(err == ERR_OK);
    netconn_close(conn);
    netconn_delete(conn);
}


static void initialize_webserver()
{
    xEventGroupWaitBits(event_group, WIFI_SETUP_DONE_BIT, true, false, portMAX_DELAY);

    xTaskCreate(&http_server_task, "http_server_task", 2048, NULL, 5, NULL);
}


void app_main()
{
    ESP_LOGI(TAG, "\n\n\n====== Поехали! ======\n\n");
    ESP_ERROR_CHECK( nvs_flash_init() );


    event_group = xEventGroupCreate();

    initialize_wifi(event_group);
    initialize_webserver();
}