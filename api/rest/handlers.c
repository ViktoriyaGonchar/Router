/**
 * @file handlers.c
 * @brief Реализация обработчиков REST API эндпоинтов
 */

#include "handlers.h"
#include "rest_api.h"
#include "../../core/base/config/config.h"
#include "../../core/hal/hal_network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

/**
 * @brief Обработчик GET /api/v1/status
 */
static int handler_status(const vgik_http_request_t *request,
                         vgik_http_response_t *response,
                         void *user_data) {
    (void)request;
    (void)user_data;
    
    const char *json = "{\"status\":\"ok\",\"version\":\"1.0.0\"}";
    return vgik_rest_api_json_response(response, VGIK_HTTP_STATUS_OK, json);
}

/**
 * @brief Обработчик GET /api/v1/interfaces
 */
static int handler_get_interfaces(const vgik_http_request_t *request,
                                  vgik_http_response_t *response,
                                  void *user_data) {
    (void)request;
    (void)user_data;
    
    const vgik_network_hal_t *hal = vgik_network_hal_get();
    if (!hal) {
        return vgik_rest_api_error_response(response, 
                                            VGIK_HTTP_STATUS_INTERNAL_ERROR,
                                            "Network HAL not available");
    }
    
    vgik_network_if_list_t list;
    if (hal->get_interfaces(&list) != 0) {
        return vgik_rest_api_error_response(response,
                                            VGIK_HTTP_STATUS_INTERNAL_ERROR,
                                            "Failed to get interfaces");
    }
    
    // Формирование JSON ответа
    char *json = malloc(4096);
    if (!json) {
        hal->free_interfaces(&list);
        return vgik_rest_api_error_response(response,
                                           VGIK_HTTP_STATUS_INTERNAL_ERROR,
                                           "Out of memory");
    }
    
    strcpy(json, "{\"interfaces\":[");
    
    for (size_t i = 0; i < list.count; i++) {
        if (i > 0) strcat(json, ",");
        
        char if_json[512];
        char ip_str[16];
        struct in_addr addr;
        addr.s_addr = list.interfaces[i].ip_config.address;
        inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));
        
        snprintf(if_json, sizeof(if_json),
            "{\"name\":\"%s\","
            "\"type\":%d,"
            "\"state\":%d,"
            "\"mac_address\":\"%s\","
            "\"mtu\":%u,"
            "\"ip_address\":\"%s\"}",
            list.interfaces[i].name,
            list.interfaces[i].type,
            list.interfaces[i].state,
            list.interfaces[i].mac_address,
            list.interfaces[i].mtu,
            ip_str);
        
        strcat(json, if_json);
    }
    
    strcat(json, "]}");
    
    hal->free_interfaces(&list);
    
    int ret = vgik_rest_api_json_response(response, VGIK_HTTP_STATUS_OK, json);
    free(json);
    return ret;
}

/**
 * @brief Обработчик GET /api/v1/config
 */
static int handler_get_config(const vgik_http_request_t *request,
                             vgik_http_response_t *response,
                             void *user_data) {
    (void)request;
    (void)user_data;
    
    // TODO: Получение конфигурации через config API
    const char *json = "{\"config\":{}}";
    return vgik_rest_api_json_response(response, VGIK_HTTP_STATUS_OK, json);
}

/**
 * @brief Обработчик POST /api/v1/config
 */
static int handler_set_config(const vgik_http_request_t *request,
                              vgik_http_response_t *response,
                              void *user_data) {
    (void)user_data;
    
    if (!request->body) {
        return vgik_rest_api_error_response(response,
                                           VGIK_HTTP_STATUS_BAD_REQUEST,
                                           "Missing request body");
    }
    
    // TODO: Парсинг и применение конфигурации
    const char *json = "{\"status\":\"ok\",\"message\":\"Config applied\"}";
    return vgik_rest_api_json_response(response, VGIK_HTTP_STATUS_OK, json);
}

/**
 * @brief Обработчик GET /api/v1/statistics
 */
static int handler_get_statistics(const vgik_http_request_t *request,
                                  vgik_http_response_t *response,
                                  void *user_data) {
    (void)request;
    (void)user_data;
    
    const char *json = "{\"statistics\":{}}";
    return vgik_rest_api_json_response(response, VGIK_HTTP_STATUS_OK, json);
}

int vgik_rest_handlers_register_all(void) {
    vgik_http_route_t routes[] = {
        {VGIK_HTTP_METHOD_GET, "/api/v1/status", handler_status, NULL},
        {VGIK_HTTP_METHOD_GET, "/api/v1/interfaces", handler_get_interfaces, NULL},
        {VGIK_HTTP_METHOD_GET, "/api/v1/config", handler_get_config, NULL},
        {VGIK_HTTP_METHOD_POST, "/api/v1/config", handler_set_config, NULL},
        {VGIK_HTTP_METHOD_GET, "/api/v1/statistics", handler_get_statistics, NULL}
    };
    
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        if (vgik_rest_api_register_route(&routes[i]) != 0) {
            return -1;
        }
    }
    
    return 0;
}

