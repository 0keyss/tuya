#include <time.h>
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "mbedtls/md.h"


#define WIFI_SSID        "TP-LINK_FC12"
#define WIFI_PASS        "88573946"

#define TUYA_PRODUCT_ID    "wx6jg4kqgsdyuh4q"
#define TUYA_DEVICE_ID     "260cd98f45cb3194c2jyiq"
#define TUYA_DEVICE_SECRET "Utzc2UElLbVbQgFD"
#define TUYA_MQTT_HOST     "m1.tuyaeu.com"  
#define TUYA_MQTT_PORT     8883

static const char *TAG = "APP";
static esp_mqtt_client_handle_t mqtt_client = NULL;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi disconnected, reconnecting...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    ESP_EVENT_ANY_ID,
                    &wifi_event_handler,
                    NULL,
                    NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                    IP_EVENT_STA_GOT_IP,
                    &wifi_event_handler,
                    NULL,
                    NULL));

    wifi_config_t wifi_config = { 0 };
    strcpy((char *)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char *)wifi_config.sta.password, WIFI_PASS);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        break;
    }
}

const char tuya_cacert_pem[] = {\
"-----BEGIN CERTIFICATE-----\n"\
"MIIDxTCCAq2gAwIBAgIBADANBgkqhkiG9w0BAQsFADCBgzELMAkGA1UEBhMCVVMx\n"\
"EDAOBgNVBAgTB0FyaXpvbmExEzARBgNVBAcTClNjb3R0c2RhbGUxGjAYBgNVBAoT\n"\
"EUdvRGFkZHkuY29tLCBJbmMuMTEwLwYDVQQDEyhHbyBEYWRkeSBSb290IENlcnRp\n"\
"ZmljYXRlIEF1dGhvcml0eSAtIEcyMB4XDTA5MDkwMTAwMDAwMFoXDTM3MTIzMTIz\n"\
"NTk1OVowgYMxCzAJBgNVBAYTAlVTMRAwDgYDVQQIEwdBcml6b25hMRMwEQYDVQQH\n"\
"EwpTY290dHNkYWxlMRowGAYDVQQKExFHb0RhZGR5LmNvbSwgSW5jLjExMC8GA1UE\n"\
"AxMoR28gRGFkZHkgUm9vdCBDZXJ0aWZpY2F0ZSBBdXRob3JpdHkgLSBHMjCCASIw\n"\
"DQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAL9xYgjx+lk09xvJGKP3gElY6SKD\n"\
"E6bFIEMBO4Tx5oVJnyfq9oQbTqC023CYxzIBsQU+B07u9PpPL1kwIuerGVZr4oAH\n"\
"/PMWdYA5UXvl+TW2dE6pjYIT5LY/qQOD+qK+ihVqf94Lw7YZFAXK6sOoBJQ7Rnwy\n"\
"DfMAZiLIjWltNowRGLfTshxgtDj6AozO091GB94KPutdfMh8+7ArU6SSYmlRJQVh\n"\
"GkSBjCypQ5Yj36w6gZoOKcUcqeldHraenjAKOc7xiID7S13MMuyFYkMlNAJWJwGR\n"\
"tDtwKj9useiciAF9n9T521NtYJ2/LOdYq7hfRvzOxBsDPAnrSTFcaUaz4EcCAwEA\n"\
"AaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYwHQYDVR0OBBYE\n"\
"FDqahQcQZyi27/a9BUFuIMGU2g/eMA0GCSqGSIb3DQEBCwUAA4IBAQCZ21151fmX\n"\
"WWcDYfF+OwYxdS2hII5PZYe096acvNjpL9DbWu7PdIxztDhC2gV7+AJ1uP2lsdeu\n"\
"9tfeE8tTEH6KRtGX+rcuKxGrkLAngPnon1rpN5+r5N9ss4UXnT3ZJE95kTXWXwTr\n"\
"gIOrmgIttRD02JDHBHNA7XIloKmf7J6raBKZV8aPEjoJpL1E/QYVN8Gb5DKj7Tjo\n"\
"2GTzLH4U/ALqn83/B2gX2yKQOC16jdFU8WnjXzPKej17CuPKf1855eJ1usV2GDPO\n"\
"LPAvTK33sefOT6jEm0pUBsV/fdUID+Ic/n4XuKxe9tQWskMJDE32p2u0mYRlynqI\n"\
"4uJEvlz36hz1\n"\
"-----END CERTIFICATE-----\n"};

static char tuya_timestamp[16];
static char tuya_username[160];
static char tuya_content[160];
static char tuya_password[80];
static char tuya_client_id[80];

void tuya_calc_password(const char *device_secret,
                        const char *content,
                        char *out_hex, size_t out_hex_len)
{
    unsigned char hmac[32];
    const mbedtls_md_info_t *md_info =
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, md_info, 1);
    mbedtls_md_hmac_starts(&ctx,
                           (const unsigned char *)device_secret,
                           strlen(device_secret));
    mbedtls_md_hmac_update(&ctx,
                           (const unsigned char *)content,
                           strlen(content));
    mbedtls_md_hmac_finish(&ctx, hmac);
    mbedtls_md_free(&ctx);

    for (int i = 0; i < 32 && (i * 2 + 1) < (int)out_hex_len; i++) {
        snprintf(out_hex + i * 2, out_hex_len - i * 2,
                 "%02x", hmac[i]);
    }
}


//obtain time funkcja

static void mqtt_start(void)
{
    // time_t now = 0;
    // time(&now);
    // // snprintf(tuya_timestamp, sizeof(tuya_timestamp),
    // "%ld", (long) (int32_t)now);  // jeśli wiesz, że mieści się w 32 bitach

    time_t now = time(NULL);

    // jeśli wystarczy Ci 32-bitowy timestamp (do roku 2038):
    uint32_t now32 = (uint32_t) now;
    //ESP_LOGI(TAG, "now = %" PRIu32, now32);
    
    // username – na DeviceID
    snprintf(tuya_username, sizeof(tuya_username),
             "%s|signMethod=hmacSha256,timestamp=%s,secureMode=1,accessType=1",
             TUYA_DEVICE_ID,
             tuya_timestamp);

    // content – na DeviceID
    snprintf(tuya_content, sizeof(tuya_content),
             "deviceId=%s,timestamp=%s,secureMode=1,accessType=1",
             TUYA_DEVICE_ID,
             tuya_timestamp);
    // password (HMAC)
    tuya_calc_password(TUYA_DEVICE_SECRET, tuya_content,
                       tuya_password, sizeof(tuya_password));

    // clientId = "tuyalink_" + DeviceID
    snprintf(tuya_client_id, sizeof(tuya_client_id),
             "tuyalink_%s", TUYA_DEVICE_ID);
                       
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtts://" TUYA_MQTT_HOST ":8883",
        .broker.address.port = TUYA_MQTT_PORT,
        .broker.verification = {
            .certificate     = tuya_cacert_pem,
            .certificate_len = sizeof(tuya_cacert_pem),
            .skip_cert_common_name_check = true,
        },
        .credentials.client_id = tuya_client_id,
        .credentials.username  = tuya_username,
        .credentials.authentication.password = tuya_password,
        .session.keepalive = 60,
    };



    ESP_LOGI(TAG, "DeviceID: %s", TUYA_DEVICE_ID);
    ESP_LOGI(TAG, "Timestamp: %s", tuya_timestamp);
    ESP_LOGI(TAG, "Content: %s", tuya_content);
    ESP_LOGI(TAG, "Password: %s", tuya_password);
    ESP_LOGI(TAG, "ClientID: %s", tuya_client_id);
    ESP_LOGI(TAG, "Username: %s", tuya_username);
    ESP_LOGI(TAG, "now = %" PRIu32, now32);
   
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));

    ESP_LOGI(TAG, "MQTT client started");
}



void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init_sta();

    // poczekaj aż dostaniemy IP
    vTaskDelay(pdMS_TO_TICKS(5000));

    mqtt_start();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
