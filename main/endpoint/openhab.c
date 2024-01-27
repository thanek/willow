#include "audio_hal.h"
#include "audio_thread.h"
#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"

#include "../http.h"
#include "audio.h"
#include "config.h"
#include "shared.h"
#include "slvgl.h"
#include "timer.h"

#define DEFAULT_TOKEN "http://your_openhab_url"
#define DEFAULT_URL   "your_openhab_token"

#define OH_URI_INTERPRETERS "/rest/voice/interpreters"

static const char *TAG = "WILLOW/OPENHAB";

void openhab_send(const char *data)
{
    bool ok = false;
    char *body = NULL, *url = NULL;
    esp_err_t ret;
    int http_status = 0, len_url = 0;

    char *openhab_url = config_get_char("openhab_url", DEFAULT_URL);
    len_url = strlen(openhab_url) + strlen(OH_URI_INTERPRETERS) + 1;
    url = calloc(sizeof(char), len_url);
    snprintf(url, len_url, "%s%s", openhab_url, OH_URI_INTERPRETERS);
    free(openhab_url);

    cJSON *cjson = cJSON_Parse(data);
    if (!cJSON_IsObject(cjson)) {
        goto end;
    }
    cJSON *text = cJSON_GetObjectItemCaseSensitive(cjson, "text");
    if (!cJSON_IsString(text) && text->valuestring != NULL) {
        goto end;
    }

    esp_http_client_handle_t hdl_hc = init_http_client();
    char *openhab_token = config_get_char("openhab_token", DEFAULT_TOKEN);
    ret = http_set_basic_auth(hdl_hc, openhab_token, "");
    free(openhab_token);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to enable HTTP Basic Authentication: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "sending '%s' to openHAB REST API on '%s'", text->valuestring, url);

    ret = http_post(hdl_hc, url, "text/plain", text->valuestring, &body, &http_status);
    cJSON_Delete(cjson);
    if (ret == ESP_OK) {
        if (http_status >= 200 && http_status <= 299) {
            ok = true;
        }
    } else {
        ESP_LOGE(TAG, "failed to read HTTP POST response from openHAB");
    }

end:
    free(url);
    if (ok) {
        war.fn_ok("Success");
    } else {
        war.fn_err("Error");
    }

    if (body != NULL && strlen(body) > 1) {
        ESP_LOGI(TAG, "REST response: %s", body);
    }

    if (lvgl_port_lock(lvgl_lock_timeout)) {
        lv_obj_clear_flag(lbl_ln4, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(lbl_ln5, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_event_cb(lbl_ln4, cb_btn_cancel);
        lv_label_set_text_static(lbl_ln4, "Status polecenia:");
        if (body != NULL && strlen(body) > 1) {
            lv_label_set_text(lbl_ln5, body);
        } else {
            lv_label_set_text(lbl_ln5, ok ? "Sukces" : "Błąd");
        }
        lvgl_port_unlock();
    }

    reset_timer(hdl_display_timer, config_get_int("display_timeout", DEFAULT_DISPLAY_TIMEOUT), false);

    free(body);
}
