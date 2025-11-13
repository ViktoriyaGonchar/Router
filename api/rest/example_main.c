/**
 * @file example_main.c
 * @brief Пример использования REST API сервера
 * 
 * Компиляция: gcc -o rest_api_example example_main.c rest_api.c handlers.c \
 *              -I../../core/base/config -I../../core/hal \
 *              -L../../build/bin -lvgik_core -lpthread
 */

#include "rest_api.h"
#include "handlers.h"
#include "../../core/base/logging/logging.h"
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

static bool g_running = true;

void signal_handler(int sig) {
    (void)sig;
    g_running = false;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    // Инициализация логирования
    vgik_log_init("rest_api.log", VGIK_LOG_INFO, 
                  VGIK_LOG_TARGET_CONSOLE | VGIK_LOG_TARGET_FILE);
    
    // Обработка сигналов
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Инициализация REST API
    uint16_t port = 8080;
    if (argc > 1) {
        port = (uint16_t)atoi(argv[1]);
    }
    
    if (vgik_rest_api_init(port, NULL) != 0) {
        VGIK_LOG_ERROR("REST_API", "Failed to initialize REST API");
        return 1;
    }
    
    // Регистрация обработчиков
    if (vgik_rest_handlers_register_all() != 0) {
        VGIK_LOG_ERROR("REST_API", "Failed to register handlers");
        vgik_rest_api_deinit();
        return 1;
    }
    
    // Запуск сервера
    if (vgik_rest_api_start() != 0) {
        VGIK_LOG_ERROR("REST_API", "Failed to start REST API server");
        vgik_rest_api_deinit();
        return 1;
    }
    
    VGIK_LOG_INFO("REST_API", "REST API server started on port %d", port);
    
    // Основной цикл
    while (g_running) {
        int processed = vgik_rest_api_process();
        if (processed < 0) {
            break;
        }
        
        // Небольшая задержка для снижения нагрузки на CPU
        usleep(10000);  // 10ms
    }
    
    // Остановка сервера
    VGIK_LOG_INFO("REST_API", "Stopping REST API server...");
    vgik_rest_api_stop();
    vgik_rest_api_deinit();
    
    vgik_log_deinit();
    
    return 0;
}

