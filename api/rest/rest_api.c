/**
 * @file rest_api.c
 * @brief Реализация REST API сервера VGIK
 * 
 * Простая реализация HTTP сервера на базе сокетов
 * В будущем можно заменить на libmicrohttpd
 */

#include "rest_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#define VGIK_REST_API_MAX_ROUTES 64
#define VGIK_REST_API_MAX_CONNECTIONS 16
#define VGIK_REST_API_BUFFER_SIZE 4096
#define VGIK_REST_API_DEFAULT_TIMEOUT 5

/**
 * @brief Структура соединения
 */
typedef struct {
    int socket;
    bool active;
    char buffer[VGIK_REST_API_BUFFER_SIZE];
    size_t buffer_pos;
} connection_t;

/**
 * @brief Глобальное состояние REST API
 */
static struct {
    bool initialized;
    bool running;
    int server_socket;
    uint16_t port;
    char bind_address[64];
    
    vgik_http_route_t routes[VGIK_REST_API_MAX_ROUTES];
    int route_count;
    
    connection_t connections[VGIK_REST_API_MAX_CONNECTIONS];
    int connection_count;
} g_rest_api = {
    .initialized = false,
    .running = false,
    .server_socket = -1,
    .port = 8080,
    .route_count = 0,
    .connection_count = 0
};

/**
 * @brief Получение строки HTTP метода
 */
static const char* http_method_to_string(vgik_http_method_t method) {
    switch (method) {
        case VGIK_HTTP_METHOD_GET: return "GET";
        case VGIK_HTTP_METHOD_POST: return "POST";
        case VGIK_HTTP_METHOD_PUT: return "PUT";
        case VGIK_HTTP_METHOD_DELETE: return "DELETE";
        case VGIK_HTTP_METHOD_PATCH: return "PATCH";
        case VGIK_HTTP_METHOD_OPTIONS: return "OPTIONS";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Парсинг HTTP метода из строки
 */
static vgik_http_method_t parse_http_method(const char *method_str) {
    if (strcmp(method_str, "GET") == 0) return VGIK_HTTP_METHOD_GET;
    if (strcmp(method_str, "POST") == 0) return VGIK_HTTP_METHOD_POST;
    if (strcmp(method_str, "PUT") == 0) return VGIK_HTTP_METHOD_PUT;
    if (strcmp(method_str, "DELETE") == 0) return VGIK_HTTP_METHOD_DELETE;
    if (strcmp(method_str, "PATCH") == 0) return VGIK_HTTP_METHOD_PATCH;
    if (strcmp(method_str, "OPTIONS") == 0) return VGIK_HTTP_METHOD_OPTIONS;
    return VGIK_HTTP_METHOD_GET;
}

/**
 * @brief Парсинг HTTP запроса
 */
static int parse_http_request(const char *buffer, size_t size, 
                               vgik_http_request_t *request) {
    if (!buffer || !request) {
        return -1;
    }
    
    memset(request, 0, sizeof(*request));
    
    // Парсинг первой строки: METHOD PATH HTTP/1.1
    char method_str[16];
    if (sscanf(buffer, "%15s %255s", method_str, request->path) != 2) {
        return -1;
    }
    
    request->method = parse_http_method(method_str);
    
    // Поиск query string
    char *query_start = strchr(request->path, '?');
    if (query_start) {
        *query_start = '\0';
        strncpy(request->query_string, query_start + 1, 
                sizeof(request->query_string) - 1);
    }
    
    // Поиск тела запроса (после пустой строки)
    const char *body_start = strstr(buffer, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        size_t body_size = size - (body_start - buffer);
        if (body_size > 0) {
            request->body = malloc(body_size + 1);
            if (request->body) {
                memcpy(request->body, body_start, body_size);
                request->body[body_size] = '\0';
                request->body_size = body_size;
            }
        }
    }
    
    // Копирование заголовков
    const char *headers_end = body_start ? body_start - 4 : buffer + size;
    size_t headers_size = headers_end - buffer;
    if (headers_size > 0) {
        request->headers = malloc(headers_size + 1);
        if (request->headers) {
            memcpy(request->headers, buffer, headers_size);
            request->headers[headers_size] = '\0';
            request->headers_size = headers_size;
        }
    }
    
    return 0;
}

/**
 * @brief Формирование HTTP ответа
 */
static int format_http_response(const vgik_http_response_t *response,
                                 char *buffer, size_t buffer_size) {
    if (!response || !buffer) {
        return -1;
    }
    
    const char *status_text = "OK";
    switch (response->status) {
        case VGIK_HTTP_STATUS_OK: status_text = "OK"; break;
        case VGIK_HTTP_STATUS_CREATED: status_text = "Created"; break;
        case VGIK_HTTP_STATUS_NO_CONTENT: status_text = "No Content"; break;
        case VGIK_HTTP_STATUS_BAD_REQUEST: status_text = "Bad Request"; break;
        case VGIK_HTTP_STATUS_UNAUTHORIZED: status_text = "Unauthorized"; break;
        case VGIK_HTTP_STATUS_FORBIDDEN: status_text = "Forbidden"; break;
        case VGIK_HTTP_STATUS_NOT_FOUND: status_text = "Not Found"; break;
        case VGIK_HTTP_STATUS_METHOD_NOT_ALLOWED: status_text = "Method Not Allowed"; break;
        case VGIK_HTTP_STATUS_INTERNAL_ERROR: status_text = "Internal Server Error"; break;
        default: status_text = "Unknown"; break;
    }
    
    const char *content_type = response->content_type ? response->content_type : "text/plain";
    
    int written = snprintf(buffer, buffer_size,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        response->status, status_text, content_type, 
        response->body_size);
    
    if (written < 0 || (size_t)written >= buffer_size) {
        return -1;
    }
    
    // Добавление тела ответа
    if (response->body && response->body_size > 0) {
        size_t remaining = buffer_size - written;
        if (remaining > response->body_size) {
            memcpy(buffer + written, response->body, response->body_size);
            written += response->body_size;
        }
    }
    
    return written;
}

/**
 * @brief Поиск маршрута для запроса
 */
static const vgik_http_route_t* find_route(vgik_http_method_t method, 
                                           const char *path) {
    for (int i = 0; i < g_rest_api.route_count; i++) {
        if (g_rest_api.routes[i].method == method &&
            strcmp(g_rest_api.routes[i].path, path) == 0) {
            return &g_rest_api.routes[i];
        }
    }
    return NULL;
}

/**
 * @brief Обработка HTTP соединения
 */
static int handle_connection(connection_t *conn) {
    if (!conn || !conn->active) {
        return -1;
    }
    
    // Чтение данных
    ssize_t received = recv(conn->socket, 
                             conn->buffer + conn->buffer_pos,
                             sizeof(conn->buffer) - conn->buffer_pos - 1,
                             0);
    
    if (received <= 0) {
        if (received == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
            // Соединение закрыто или ошибка
            close(conn->socket);
            conn->active = false;
            return -1;
        }
        return 0;  // Нет данных пока
    }
    
    conn->buffer_pos += received;
    conn->buffer[conn->buffer_pos] = '\0';
    
    // Проверка на полный запрос (наличие \r\n\r\n)
    if (strstr(conn->buffer, "\r\n\r\n") == NULL) {
        return 0;  // Запрос еще не полный
    }
    
    // Парсинг запроса
    vgik_http_request_t request;
    if (parse_http_request(conn->buffer, conn->buffer_pos, &request) != 0) {
        // Ошибка парсинга - отправка ошибки
        const char *error_response = 
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n";
        send(conn->socket, error_response, strlen(error_response), 0);
        close(conn->socket);
        conn->active = false;
        return -1;
    }
    
    // Поиск маршрута
    const vgik_http_route_t *route = find_route(request.method, request.path);
    
    // Формирование ответа
    vgik_http_response_t response = {
        .status = VGIK_HTTP_STATUS_NOT_FOUND,
        .content_type = "application/json",
        .body = NULL,
        .body_size = 0
    };
    
    if (route && route->handler) {
        // Вызов обработчика
        route->handler(&request, &response, route->user_data);
    } else {
        // 404 Not Found
        const char *not_found = "{\"error\":\"Not Found\"}";
        response.body = (char*)not_found;
        response.body_size = strlen(not_found);
    }
    
    // Отправка ответа
    char response_buffer[VGIK_REST_API_BUFFER_SIZE];
    int response_size = format_http_response(&response, 
                                              response_buffer, 
                                              sizeof(response_buffer));
    if (response_size > 0) {
        send(conn->socket, response_buffer, response_size, 0);
    }
    
    // Освобождение ресурсов
    if (request.body) free(request.body);
    if (request.headers) free(request.headers);
    if (response.body && response.body != not_found) {
        free(response.body);
    }
    
    // Закрытие соединения
    close(conn->socket);
    conn->active = false;
    
    return 1;  // Обработан один запрос
}

int vgik_rest_api_init(uint16_t port, const char *bind_address) {
    if (g_rest_api.initialized) {
        return 0;
    }
    
    g_rest_api.port = port;
    if (bind_address) {
        strncpy(g_rest_api.bind_address, bind_address, 
                sizeof(g_rest_api.bind_address) - 1);
        g_rest_api.bind_address[sizeof(g_rest_api.bind_address) - 1] = '\0';
    } else {
        g_rest_api.bind_address[0] = '\0';
    }
    
    g_rest_api.initialized = true;
    return 0;
}

void vgik_rest_api_deinit(void) {
    if (!g_rest_api.initialized) {
        return;
    }
    
    vgik_rest_api_stop();
    
    g_rest_api.initialized = false;
    g_rest_api.route_count = 0;
}

int vgik_rest_api_register_route(const vgik_http_route_t *route) {
    if (!route || !route->path || !route->handler) {
        return -1;
    }
    
    if (g_rest_api.route_count >= VGIK_REST_API_MAX_ROUTES) {
        return -1;
    }
    
    g_rest_api.routes[g_rest_api.route_count] = *route;
    g_rest_api.route_count++;
    
    return 0;
}

int vgik_rest_api_start(void) {
    if (!g_rest_api.initialized || g_rest_api.running) {
        return -1;
    }
    
    // Создание сокета
    g_rest_api.server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_rest_api.server_socket < 0) {
        return -1;
    }
    
    // Установка опций сокета
    int opt = 1;
    setsockopt(g_rest_api.server_socket, SOL_SOCKET, SO_REUSEADDR, 
               &opt, sizeof(opt));
    
    // Настройка адреса
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(g_rest_api.port);
    
    if (g_rest_api.bind_address[0] != '\0') {
        inet_pton(AF_INET, g_rest_api.bind_address, &addr.sin_addr);
    } else {
        addr.sin_addr.s_addr = INADDR_ANY;
    }
    
    // Привязка
    if (bind(g_rest_api.server_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(g_rest_api.server_socket);
        g_rest_api.server_socket = -1;
        return -1;
    }
    
    // Прослушивание
    if (listen(g_rest_api.server_socket, VGIK_REST_API_MAX_CONNECTIONS) < 0) {
        close(g_rest_api.server_socket);
        g_rest_api.server_socket = -1;
        return -1;
    }
    
    // Установка неблокирующего режима
    int flags = fcntl(g_rest_api.server_socket, F_GETFL, 0);
    fcntl(g_rest_api.server_socket, F_SETFL, flags | O_NONBLOCK);
    
    g_rest_api.running = true;
    return 0;
}

void vgik_rest_api_stop(void) {
    if (!g_rest_api.running) {
        return;
    }
    
    // Закрытие всех соединений
    for (int i = 0; i < g_rest_api.connection_count; i++) {
        if (g_rest_api.connections[i].active) {
            close(g_rest_api.connections[i].socket);
            g_rest_api.connections[i].active = false;
        }
    }
    g_rest_api.connection_count = 0;
    
    // Закрытие серверного сокета
    if (g_rest_api.server_socket >= 0) {
        close(g_rest_api.server_socket);
        g_rest_api.server_socket = -1;
    }
    
    g_rest_api.running = false;
}

int vgik_rest_api_process(void) {
    if (!g_rest_api.running) {
        return 0;
    }
    
    int processed = 0;
    
    // Принятие новых соединений
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_socket = accept(g_rest_api.server_socket,
                               (struct sockaddr*)&client_addr,
                               &client_len);
    
    if (client_socket >= 0) {
        // Поиск свободного слота
        int slot = -1;
        for (int i = 0; i < VGIK_REST_API_MAX_CONNECTIONS; i++) {
            if (!g_rest_api.connections[i].active) {
                slot = i;
                break;
            }
        }
        
        if (slot >= 0) {
            // Установка неблокирующего режима
            int flags = fcntl(client_socket, F_GETFL, 0);
            fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);
            
            g_rest_api.connections[slot].socket = client_socket;
            g_rest_api.connections[slot].active = true;
            g_rest_api.connections[slot].buffer_pos = 0;
            
            if (slot >= g_rest_api.connection_count) {
                g_rest_api.connection_count = slot + 1;
            }
        } else {
            // Нет свободных слотов
            close(client_socket);
        }
    }
    
    // Обработка существующих соединений
    for (int i = 0; i < g_rest_api.connection_count; i++) {
        if (g_rest_api.connections[i].active) {
            if (handle_connection(&g_rest_api.connections[i]) > 0) {
                processed++;
            }
        }
    }
    
    return processed;
}

int vgik_rest_api_json_response(vgik_http_response_t *response,
                                vgik_http_status_t status,
                                const char *json_data) {
    if (!response) {
        return -1;
    }
    
    response->status = status;
    response->content_type = "application/json";
    
    if (json_data) {
        response->body = malloc(strlen(json_data) + 1);
        if (response->body) {
            strcpy(response->body, json_data);
            response->body_size = strlen(json_data);
        }
    } else {
        response->body = NULL;
        response->body_size = 0;
    }
    
    return 0;
}

int vgik_rest_api_text_response(vgik_http_response_t *response,
                                vgik_http_status_t status,
                                const char *text) {
    if (!response) {
        return -1;
    }
    
    response->status = status;
    response->content_type = "text/plain";
    
    if (text) {
        response->body = malloc(strlen(text) + 1);
        if (response->body) {
            strcpy(response->body, text);
            response->body_size = strlen(text);
        }
    } else {
        response->body = NULL;
        response->body_size = 0;
    }
    
    return 0;
}

int vgik_rest_api_error_response(vgik_http_response_t *response,
                                 vgik_http_status_t status,
                                 const char *error_message) {
    if (!response) {
        return -1;
    }
    
    char json_error[512];
    snprintf(json_error, sizeof(json_error), 
             "{\"error\":\"%s\"}", error_message ? error_message : "Unknown error");
    
    return vgik_rest_api_json_response(response, status, json_error);
}

void* vgik_rest_api_parse_json_body(const vgik_http_request_t *request) {
    // TODO: Реализовать парсинг JSON (используя cJSON)
    // Пока возвращаем указатель на тело запроса
    return (void*)request->body;
}

const char* vgik_rest_api_get_header(const vgik_http_request_t *request,
                                     const char *header_name) {
    if (!request || !request->headers || !header_name) {
        return NULL;
    }
    
    // Простой поиск заголовка
    char search_str[256];
    snprintf(search_str, sizeof(search_str), "%s:", header_name);
    
    const char *header_start = strstr(request->headers, search_str);
    if (!header_start) {
        return NULL;
    }
    
    // Пропуск имени заголовка
    header_start += strlen(search_str);
    
    // Пропуск пробелов
    while (*header_start == ' ') header_start++;
    
    // Поиск конца строки
    const char *header_end = strstr(header_start, "\r\n");
    if (!header_end) {
        return NULL;
    }
    
    // Копирование значения (временное, нужно улучшить)
    static char header_value[256];
    size_t len = header_end - header_start;
    if (len >= sizeof(header_value)) {
        len = sizeof(header_value) - 1;
    }
    strncpy(header_value, header_start, len);
    header_value[len] = '\0';
    
    return header_value;
}

