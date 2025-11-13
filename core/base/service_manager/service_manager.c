/**
 * @file service_manager.c
 * @brief Реализация менеджера сервисов VGIK
 */

#include "service_manager.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define VGIK_SERVICE_MAX_SERVICES 64

/**
 * @brief Глобальное состояние менеджера сервисов
 */
static struct {
    bool initialized;
    vgik_service_t services[VGIK_SERVICE_MAX_SERVICES];
    int service_count;
} g_service_manager = {
    .initialized = false,
    .service_count = 0
};

/**
 * @brief Получение текущего времени в миллисекундах
 */
static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/**
 * @brief Поиск сервиса по имени
 */
static vgik_service_t* find_service(const char *name) {
    if (!name) {
        return NULL;
    }
    
    for (int i = 0; i < g_service_manager.service_count; i++) {
        if (g_service_manager.services[i].name &&
            strcmp(g_service_manager.services[i].name, name) == 0) {
            return &g_service_manager.services[i];
        }
    }
    
    return NULL;
}

/**
 * @brief Проверка зависимостей сервиса
 */
static bool check_dependencies(vgik_service_t *service) {
    if (!service || !service->dependencies) {
        return true;
    }
    
    for (size_t i = 0; i < service->dependencies_count; i++) {
        const char *dep_name = service->dependencies[i];
        vgik_service_t *dep = find_service(dep_name);
        
        if (!dep) {
            return false;  // Зависимость не найдена
        }
        
        if (dep->state != VGIK_SERVICE_STATE_RUNNING) {
            return false;  // Зависимость не запущена
        }
    }
    
    return true;
}

/**
 * @brief Запуск зависимостей сервиса
 */
static int start_dependencies(vgik_service_t *service) {
    if (!service || !service->dependencies) {
        return 0;
    }
    
    for (size_t i = 0; i < service->dependencies_count; i++) {
        const char *dep_name = service->dependencies[i];
        int ret = vgik_service_start(dep_name);
        if (ret != 0) {
            return ret;
        }
    }
    
    return 0;
}

int vgik_service_manager_init(void) {
    if (g_service_manager.initialized) {
        return 0;
    }
    
    memset(g_service_manager.services, 0, sizeof(g_service_manager.services));
    g_service_manager.initialized = true;
    return 0;
}

void vgik_service_manager_deinit(void) {
    if (!g_service_manager.initialized) {
        return;
    }
    
    // Остановка всех сервисов
    vgik_service_stop_all();
    
    // Очистка
    memset(g_service_manager.services, 0, sizeof(g_service_manager.services));
    g_service_manager.service_count = 0;
    g_service_manager.initialized = false;
}

int vgik_service_register(vgik_service_t *service) {
    if (!g_service_manager.initialized || !service || !service->name) {
        return -1;
    }
    
    // Проверка на дубликаты
    if (find_service(service->name)) {
        return -1;
    }
    
    // Проверка наличия места
    if (g_service_manager.service_count >= VGIK_SERVICE_MAX_SERVICES) {
        return -1;
    }
    
    // Копирование описания сервиса
    vgik_service_t *new_service = &g_service_manager.services[g_service_manager.service_count];
    *new_service = *service;
    new_service->state = VGIK_SERVICE_STATE_STOPPED;
    new_service->start_time = 0;
    new_service->last_restart_time = 0;
    new_service->restart_count = 0;
    
    g_service_manager.service_count++;
    return 0;
}

int vgik_service_unregister(const char *name) {
    if (!name) {
        return -1;
    }
    
    for (int i = 0; i < g_service_manager.service_count; i++) {
        if (g_service_manager.services[i].name &&
            strcmp(g_service_manager.services[i].name, name) == 0) {
            
            // Остановка сервиса если запущен
            if (g_service_manager.services[i].state == VGIK_SERVICE_STATE_RUNNING) {
                vgik_service_stop(name);
            }
            
            // Сдвиг массива
            for (int j = i; j < g_service_manager.service_count - 1; j++) {
                g_service_manager.services[j] = g_service_manager.services[j + 1];
            }
            
            g_service_manager.service_count--;
            return 0;
        }
    }
    
    return -1;
}

int vgik_service_start(const char *name) {
    vgik_service_t *service = find_service(name);
    if (!service) {
        return -1;
    }
    
    // Проверка текущего состояния
    if (service->state == VGIK_SERVICE_STATE_RUNNING ||
        service->state == VGIK_SERVICE_STATE_STARTING) {
        return 0;  // Уже запущен или запускается
    }
    
    // Проверка зависимостей
    if (!check_dependencies(service)) {
        // Попытка запустить зависимости
        if (start_dependencies(service) != 0) {
            service->state = VGIK_SERVICE_STATE_FAILED;
            return -1;
        }
    }
    
    // Запуск сервиса
    service->state = VGIK_SERVICE_STATE_STARTING;
    
    if (service->start_cb) {
        int ret = service->start_cb(service->user_data);
        if (ret != 0) {
            service->state = VGIK_SERVICE_STATE_FAILED;
            return ret;
        }
    }
    
    service->state = VGIK_SERVICE_STATE_RUNNING;
    service->start_time = get_timestamp_ms();
    
    return 0;
}

int vgik_service_stop(const char *name) {
    vgik_service_t *service = find_service(name);
    if (!service) {
        return -1;
    }
    
    // Проверка текущего состояния
    if (service->state == VGIK_SERVICE_STATE_STOPPED ||
        service->state == VGIK_SERVICE_STATE_STOPPING) {
        return 0;  // Уже остановлен или останавливается
    }
    
    // Остановка сервиса
    service->state = VGIK_SERVICE_STATE_STOPPING;
    
    if (service->stop_cb) {
        int ret = service->stop_cb(service->user_data);
        if (ret != 0) {
            service->state = VGIK_SERVICE_STATE_FAILED;
            return ret;
        }
    }
    
    service->state = VGIK_SERVICE_STATE_STOPPED;
    service->start_time = 0;
    
    return 0;
}

int vgik_service_restart(const char *name) {
    int ret = vgik_service_stop(name);
    if (ret != 0) {
        return ret;
    }
    
    // Небольшая задержка перед перезапуском
    // В реальной реализации можно использовать sleep или таймеры
    
    return vgik_service_start(name);
}

vgik_service_state_t vgik_service_get_state(const char *name) {
    vgik_service_t *service = find_service(name);
    if (!service) {
        return VGIK_SERVICE_STATE_FAILED;
    }
    
    return service->state;
}

bool vgik_service_is_healthy(const char *name) {
    vgik_service_t *service = find_service(name);
    if (!service) {
        return false;
    }
    
    if (service->state != VGIK_SERVICE_STATE_RUNNING) {
        return false;
    }
    
    if (service->health_cb) {
        return service->health_cb(service->user_data);
    }
    
    // Если нет health check, считаем здоровым если запущен
    return true;
}

int vgik_service_start_all(void) {
    int started = 0;
    
    for (int i = 0; i < g_service_manager.service_count; i++) {
        if (vgik_service_start(g_service_manager.services[i].name) == 0) {
            started++;
        }
    }
    
    return started;
}

int vgik_service_stop_all(void) {
    int stopped = 0;
    
    for (int i = 0; i < g_service_manager.service_count; i++) {
        if (vgik_service_stop(g_service_manager.services[i].name) == 0) {
            stopped++;
        }
    }
    
    return stopped;
}

void vgik_service_manager_process(void) {
    if (!g_service_manager.initialized) {
        return;
    }
    
    uint64_t now = get_timestamp_ms();
    
    for (int i = 0; i < g_service_manager.service_count; i++) {
        vgik_service_t *service = &g_service_manager.services[i];
        
        // Проверка здоровья запущенных сервисов
        if (service->state == VGIK_SERVICE_STATE_RUNNING) {
            if (!vgik_service_is_healthy(service->name)) {
                // Сервис нездоров, но не перезапускаем автоматически
                // Это можно сделать через отдельный механизм
            }
        }
        
        // Автоматический перезапуск при сбое
        if (service->auto_restart && 
            service->state == VGIK_SERVICE_STATE_FAILED) {
            
            // Проверка лимита попыток перезапуска
            if (service->max_restart_attempts > 0 &&
                service->restart_count >= service->max_restart_attempts) {
                continue;  // Превышен лимит попыток
            }
            
            // Проверка задержки перед перезапуском
            if (service->last_restart_time == 0 ||
                (now - service->last_restart_time) >= service->restart_delay_ms) {
                
                service->restart_count++;
                service->last_restart_time = now;
                service->state = VGIK_SERVICE_STATE_RESTARTING;
                
                // Перезапуск
                vgik_service_start(service->name);
            }
        }
    }
}

int vgik_service_list(const char **names, int max_count) {
    if (!names || max_count <= 0) {
        return 0;
    }
    
    int count = 0;
    for (int i = 0; i < g_service_manager.service_count && count < max_count; i++) {
        if (g_service_manager.services[i].name) {
            names[count++] = g_service_manager.services[i].name;
        }
    }
    
    return count;
}

