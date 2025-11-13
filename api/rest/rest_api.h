/**
 * @file rest_api.h
 * @brief REST API сервер VGIK
 * 
 * Предоставляет HTTP API для управления устройством
 */

#ifndef VGIK_REST_API_H
#define VGIK_REST_API_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief HTTP методы
 */
typedef enum {
    VGIK_HTTP_METHOD_GET,
    VGIK_HTTP_METHOD_POST,
    VGIK_HTTP_METHOD_PUT,
    VGIK_HTTP_METHOD_DELETE,
    VGIK_HTTP_METHOD_PATCH,
    VGIK_HTTP_METHOD_OPTIONS
} vgik_http_method_t;

/**
 * @brief HTTP статус коды
 */
typedef enum {
    VGIK_HTTP_STATUS_OK = 200,
    VGIK_HTTP_STATUS_CREATED = 201,
    VGIK_HTTP_STATUS_NO_CONTENT = 204,
    VGIK_HTTP_STATUS_BAD_REQUEST = 400,
    VGIK_HTTP_STATUS_UNAUTHORIZED = 401,
    VGIK_HTTP_STATUS_FORBIDDEN = 403,
    VGIK_HTTP_STATUS_NOT_FOUND = 404,
    VGIK_HTTP_STATUS_METHOD_NOT_ALLOWED = 405,
    VGIK_HTTP_STATUS_INTERNAL_ERROR = 500
} vgik_http_status_t;

/**
 * @brief Структура HTTP запроса
 */
typedef struct {
    vgik_http_method_t method;
    char path[256];
    char query_string[512];
    char *headers;
    size_t headers_size;
    char *body;
    size_t body_size;
    void *user_data;  // Для внутреннего использования
} vgik_http_request_t;

/**
 * @brief Структура HTTP ответа
 */
typedef struct {
    vgik_http_status_t status;
    char *headers;
    size_t headers_size;
    char *body;
    size_t body_size;
    const char *content_type;
} vgik_http_response_t;

/**
 * @brief Callback для обработки HTTP запросов
 * 
 * @param request HTTP запрос
 * @param response HTTP ответ (заполняется обработчиком)
 * @param user_data Пользовательские данные
 * @return 0 при успехе, отрицательное значение при ошибке
 */
typedef int (*vgik_http_handler_t)(const vgik_http_request_t *request,
                                    vgik_http_response_t *response,
                                    void *user_data);

/**
 * @brief Структура маршрута
 */
typedef struct {
    vgik_http_method_t method;
    const char *path;
    vgik_http_handler_t handler;
    void *user_data;
} vgik_http_route_t;

/**
 * @brief Инициализация REST API сервера
 * 
 * @param port Порт для прослушивания
 * @param bind_address Адрес для привязки (NULL для всех интерфейсов)
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_rest_api_init(uint16_t port, const char *bind_address);

/**
 * @brief Деинициализация REST API сервера
 */
void vgik_rest_api_deinit(void);

/**
 * @brief Регистрация маршрута
 * 
 * @param route Описание маршрута
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_rest_api_register_route(const vgik_http_route_t *route);

/**
 * @brief Запуск сервера
 * 
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_rest_api_start(void);

/**
 * @brief Остановка сервера
 */
void vgik_rest_api_stop(void);

/**
 * @brief Обработка запросов (должна вызываться периодически)
 * 
 * @return Количество обработанных запросов
 */
int vgik_rest_api_process(void);

/**
 * @brief Создание JSON ответа
 * 
 * @param response Структура ответа
 * @param status HTTP статус
 * @param json_data JSON данные (строка)
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_rest_api_json_response(vgik_http_response_t *response,
                                vgik_http_status_t status,
                                const char *json_data);

/**
 * @brief Создание текстового ответа
 */
int vgik_rest_api_text_response(vgik_http_response_t *response,
                                vgik_http_status_t status,
                                const char *text);

/**
 * @brief Создание ответа об ошибке
 */
int vgik_rest_api_error_response(vgik_http_response_t *response,
                                 vgik_http_status_t status,
                                 const char *error_message);

/**
 * @brief Парсинг JSON тела запроса
 * 
 * @param request HTTP запрос
 * @return Указатель на распарсенный JSON или NULL при ошибке
 */
void* vgik_rest_api_parse_json_body(const vgik_http_request_t *request);

/**
 * @brief Получение значения заголовка запроса
 */
const char* vgik_rest_api_get_header(const vgik_http_request_t *request,
                                     const char *header_name);

#ifdef __cplusplus
}
#endif

#endif /* VGIK_REST_API_H */

