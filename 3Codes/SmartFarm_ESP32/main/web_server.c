/**
 * @file web_server.c
 * @brief Level 3: Local HTTP server implementation.
 *
 * Key design decisions:
 *  1. HTML is stored as a const char[] in Flash (.rodata) — no heap
 *     allocation, no file system required.  Size is ~1.8 KB.
 *  2. /api/data builds JSON directly with snprintf() — no cJSON heap
 *     allocation needed for such a small, fixed-schema response.
 *  3. /api/cmd uses cJSON for robust parsing of the incoming body
 *     (handles whitespace, different key ordering, etc.).
 *  4. max_open_sockets = HTTP_MAX_OPEN_SOCKETS (4) caps RAM usage:
 *     each open socket consumes ~3 KB of heap.
 *  5. lru_purge_enable = true automatically closes the least-recently-
 *     used connection when the socket limit is hit.
 */

#include "web_server.h"
#include "sensor_store.h"
#include "uart_handler.h"
#include "app_config.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"

#include <string.h>
#include <stdio.h>

static const char *TAG = "WEB_SERVER";

static httpd_handle_t s_server = NULL;

/* ── Embedded HTML page (Flash .rodata, NOT RAM) ─────────────────────────── */
/**
 * Minified single-page dashboard.
 * JavaScript:
 *   update()        → fetch /api/data every 2 s, update DOM values
 *   sendCmd(c)      → POST /api/cmd {"cmd": c}, show alert on response
 */
static const char DASHBOARD_HTML[] =
    "<!DOCTYPE html><html lang='en'><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Smart Farm Monitor</title>"
    "<style>"
    "body{font-family:Arial,sans-serif;background:#f0f4f0;margin:0;padding:16px}"
    "h1{color:#2d7d2d;margin-bottom:4px}"
    "p.sub{color:#666;margin:0 0 16px}"
    ".card{background:#fff;border-radius:10px;padding:16px;margin-bottom:12px;"
    "      box-shadow:0 2px 6px rgba(0,0,0,.10)}"
    ".row{display:flex;align-items:center;margin:6px 0}"
    ".lbl{width:130px;color:#555;font-size:14px}"
    ".val{font-size:22px;font-weight:700;color:#2d7d2d;min-width:70px}"
    ".unit{color:#888;font-size:13px;margin-left:4px}"
    ".dot{width:12px;height:12px;border-radius:50%;display:inline-block;margin-right:6px}"
    ".on{background:#4CAF50}.off{background:#bbb}"
    "button{padding:9px 18px;margin:4px;border:none;border-radius:6px;"
    "       cursor:pointer;font-size:13px;font-weight:600;transition:opacity .2s}"
    "button:active{opacity:.7}"
    ".btn-on{background:#4CAF50;color:#fff}"
    ".btn-off{background:#f44336;color:#fff}"
    ".btn-auto{background:#2196F3;color:#fff}"
    ".btn-man{background:#FF9800;color:#fff}"
    "#ts{color:#999;font-size:12px}"
    "</style></head><body>"
    "<h1>&#127807; Smart Farm Monitor</h1>"
    "<p class='sub'>ESP32-WROOM-32 | ESP-IDF v5</p>"

    "<div class='card'>"
    "<b>&#127782; Sensor Readings</b>"
    "<div class='row'><span class='lbl'>Temperature</span>"
    "<span class='val' id='temp'>--</span><span class='unit'>°C</span></div>"
    "<div class='row'><span class='lbl'>Humidity</span>"
    "<span class='val' id='hum'>--</span><span class='unit'>%RH</span></div>"
    "<div class='row'><span class='lbl'>Light</span>"
    "<span class='val' id='light'>--</span><span class='unit'>lux</span></div>"
    "<div class='row'><span class='lbl'>Soil Moisture</span>"
    "<span class='val' id='soil'>--</span><span class='unit'>%</span></div>"
    "</div>"

    "<div class='card'>"
    "<b>&#9881;&#65039; Actuator Control</b>"
    "<div class='row'><span class='lbl'>Water Pump</span>"
    "<span class='dot' id='pdot'></span>"
    "<span id='pst' style='margin-right:12px'>--</span>"
    "<button class='btn-on'  onclick=\"sendCmd('PUMP_ON')\">ON</button>"
    "<button class='btn-off' onclick=\"sendCmd('PUMP_OFF')\">OFF</button></div>"
    "<div class='row'><span class='lbl'>Mist / Fan</span>"
    "<span class='dot' id='mdot'></span>"
    "<span id='mst' style='margin-right:12px'>--</span>"
    "<button class='btn-on'  onclick=\"sendCmd('MIST_ON')\">ON</button>"
    "<button class='btn-off' onclick=\"sendCmd('MIST_OFF')\">OFF</button></div>"
    "<div class='row'><span class='lbl'>Mode</span>"
    "<button class='btn-auto' onclick=\"sendCmd('AUTO_ENABLE')\">AUTO</button>"
    "<button class='btn-man'  onclick=\"sendCmd('MANUAL_ENABLE')\">MANUAL</button></div>"
    "</div>"

    "<p id='ts'>Waiting for first update...</p>"

    "<script>"
    "function update(){"
    "fetch('/api/data').then(r=>r.json()).then(d=>{"
    "  if(!d.valid){"
    "    document.getElementById('temp').textContent='--';"
    "    document.getElementById('hum').textContent='--';"
    "    document.getElementById('light').textContent='--';"
    "    document.getElementById('soil').textContent='--';"
    "    document.getElementById('pst').textContent='--';"
    "    document.getElementById('pdot').className='dot off';"
    "    document.getElementById('mst').textContent='--';"
    "    document.getElementById('mdot').className='dot off';"
    "    document.getElementById('ts').textContent='Waiting for data...';"
    "    return;"
    "  }"
    "  document.getElementById('temp').textContent=d.temp.toFixed(1);"
    "  document.getElementById('hum').textContent=d.hum.toFixed(1);"
    "  document.getElementById('light').textContent=d.light;"
    "  document.getElementById('soil').textContent=d.soil;"
    "  var pon=d.pump===1;"
    "  document.getElementById('pst').textContent=pon?'ON':'OFF';"
    "  document.getElementById('pdot').className='dot '+(pon?'on':'off');"
    "  var mon=d.mist===1;"
    "  document.getElementById('mst').textContent=mon?'ON':'OFF';"
    "  document.getElementById('mdot').className='dot '+(mon?'on':'off');"
    "  document.getElementById('ts').textContent="
    "    'Last update: '+new Date().toLocaleTimeString();"
    "}).catch(e=>console.error('Fetch error',e));}"
    "function sendCmd(c){"
    "fetch('/api/cmd',{method:'POST',"
    "  headers:{'Content-Type':'application/json'},"
    "  body:JSON.stringify({cmd:c})})"
    ".then(r=>r.json()).then(d=>{"
    "  alert('STM32 response: '+d.status);"
    "}).catch(e=>alert('Send failed: '+e));}"
    "update();"
    "setInterval(update,2000);"   /* Refresh every 2 seconds */
    "</script>"
    "</body></html>";

/* ── Handler: GET / ───────────────────────────────────────────────────────── */

/**
 * @brief Serve the embedded HTML dashboard page.
 */
static esp_err_t handler_root_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_send(req, DASHBOARD_HTML, sizeof(DASHBOARD_HTML) - 1);
    return ESP_OK;
}

/* ── Handler: GET /api/data ───────────────────────────────────────────────── */

/**
 * @brief Return current sensor data as a JSON object.
 *
 * Response example:
 *   {"temp":25.30,"hum":58.00,"light":320,"soil":45,"pump":1,"mist":0}
 */
static esp_err_t handler_api_data_get(httpd_req_t *req)
{
    sensor_data_t data = {0};
    sensor_store_get(&data);

    char json[160];
    snprintf(json, sizeof(json),
             "{\"temp\":%.2f,\"hum\":%.2f,\"light\":%u,"
             "\"soil\":%u,\"pump\":%u,\"mist\":%u,\"valid\":%s}",
             data.temperature,          data.humidity,
             (unsigned)data.light,      (unsigned)data.soil_moisture,
             (unsigned)data.pump_status,(unsigned)data.mist_status,
             data.valid ? "true" : "false");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_send(req, json, (ssize_t)strlen(json));
    return ESP_OK;
}

/* ── Handler: POST /api/cmd ───────────────────────────────────────────────── */

/**
 * @brief Accept a JSON command body and forward it to STM32 via UART.
 *
 * Expected body: {"cmd":"PUMP_ON"}
 * Supported values: PUMP_ON, PUMP_OFF, MIST_ON, MIST_OFF,
 *                   AUTO_ENABLE, MANUAL_ENABLE
 *
 * Response: {"status":"OK"} or HTTP 400 on error.
 */
static esp_err_t handler_api_cmd_post(httpd_req_t *req)
{
    /* Read body (limit to 80 bytes — more than enough for our schema) */
    char body[80] = {0};
    int received = httpd_req_recv(req, body, sizeof(body) - 1);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }
    body[received] = '\0';

    /* Parse JSON with cJSON */
    cJSON *root = cJSON_ParseWithLength(body, (size_t)received);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    const cJSON *cmd_item = cJSON_GetObjectItemCaseSensitive(root, "cmd");
    if (!cJSON_IsString(cmd_item) || !cmd_item->valuestring) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'cmd' field");
        return ESP_FAIL;
    }

    /* Map string → enum */
    const char *cs = cmd_item->valuestring;
    farm_cmd_t  cmd = CMD_UNKNOWN;

    if      (strcmp(cs, "PUMP_ON")       == 0) cmd = CMD_PUMP_ON;
    else if (strcmp(cs, "PUMP_OFF")      == 0) cmd = CMD_PUMP_OFF;
    else if (strcmp(cs, "MIST_ON")       == 0) cmd = CMD_MIST_ON;
    else if (strcmp(cs, "MIST_OFF")      == 0) cmd = CMD_MIST_OFF;
    else if (strcmp(cs, "AUTO_ENABLE")   == 0) cmd = CMD_MODE_AUTO;
    else if (strcmp(cs, "MANUAL_ENABLE") == 0) cmd = CMD_MODE_MANUAL;

    cJSON_Delete(root);

    if (cmd == CMD_UNKNOWN) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Unknown command");
        return ESP_FAIL;
    }

    /* Forward to STM32 */
    uart_handler_send_cmd(cmd);
    ESP_LOGI(TAG, "HTTP command: %s", cs);

    /* Respond with OK */
    const char *resp = "{\"status\":\"OK\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, (ssize_t)strlen(resp));
    return ESP_OK;
}

/* ── URI route table (stored in Flash) ───────────────────────────────────── */

static const httpd_uri_t s_routes[] = {
    {
        .uri     = "/",
        .method  = HTTP_GET,
        .handler = handler_root_get,
    },
    {
        .uri     = "/api/data",
        .method  = HTTP_GET,
        .handler = handler_api_data_get,
    },
    {
        .uri     = "/api/cmd",
        .method  = HTTP_POST,
        .handler = handler_api_cmd_post,
    },
};

/* ── Public API ──────────────────────────────────────────────────────────── */

esp_err_t web_server_start(void)
{
    httpd_config_t cfg   = HTTPD_DEFAULT_CONFIG();
    cfg.server_port      = HTTP_SERVER_PORT;
    cfg.send_wait_timeout = HTTP_SEND_TIMEOUT_SEC;
    cfg.max_open_sockets = HTTP_MAX_OPEN_SOCKETS;
    cfg.lru_purge_enable = true;   /* Auto-close LRU socket on socket limit */
    cfg.stack_size       = 6144;   /* Enough for cJSON + snprintf on POST   */

    if (httpd_start(&s_server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }

    /* Register all routes */
    for (int i = 0; i < (int)(sizeof(s_routes) / sizeof(s_routes[0])); i++) {
        httpd_register_uri_handler(s_server, &s_routes[i]);
    }

    ESP_LOGI(TAG, "HTTP server started on port %d", HTTP_SERVER_PORT);
    ESP_LOGI(TAG, "Dashboard → http://<ESP32_IP>/");
    return ESP_OK;
}

void web_server_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
}
