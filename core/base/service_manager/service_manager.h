/**
 * @file service_manager.h
 * @brief Менеджер сервисов VGIK
 * 
 * Управляет жизненным циклом сервисов: запуск, остановка, мониторинг
 */

#ifndef VGIK_SERVICE_MANAGER_H
#define VGIK_SERVICE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Состояния сервиса
 */
typedef enum {
    VGIK_SERVICE_STATE_STOPPED,
    VGIK_SERVICE_STATE_STARTING,
    VGIK_SERVICE_STATE_RUNNING,
    VGIK_SERVICE_STATE_STOPPING,
    VGIK_SERVICE_STATE_FAILED,
    VGIK_SERVICE_STATE_RESTARTING
} vgik_service_state_t;

/**
 * @brief Callback для запуска сервиса
 * 
 * @param user_data Пользовательские данные
 * @return 0 при успехе, отрицательное значение при ошибке
 */
typedef int (*vgik_service_start_cb_t)(void *user_data);

/**
 * @brief Callback для остановки сервиса
 * 
 * @param user_data Пользовательские данные
 * @return 0 при успехе, отрицательное значение при ошибке
 */
typedef int (*vgik_service_stop_cb_t)(void *user_data);

/**
 * @brief Callback для проверки здоровья сервиса
 * 
 * @param user_data Пользовательские данные
 * @return true если сервис здоров, false иначе
 */
typedef bool (*vgik_service_health_cb_t)(void *user_data);

/**
 * @brief Структура описания сервиса
 */
typedef struct {
    const char *name;
    vgik_service_start_cb_t start_cb;
    vgik_service_stop_cb_t stop_cb;
    vgik_service_health_cb_t health_cb;
    void *user_data;
    
    // Зависимости
    const char **dependencies;
    size_t dependencies_count;
    
    // Автоматический перезапуск
    bool auto_restart;
    uint32_t restart_delay_ms;
    uint32_t max_restart_attempts;
    uint32_t restart_count;
    
    // Внутреннее состояние
    vgik_service_state_t state;
    uint64_t start_time;
    uint64_t last_restart_time;
} vgik_service_t;

/**
 * @brief Инициализация менеджера сервисов
 * 
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_service_manager_init(void);

/**
 * @brief Деинициализация менеджера сервисов
 */
void vgik_service_manager_deinit(void);

/**
 * @brief Регистрация сервиса
 * 
 * @param service Описание сервиса
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_service_register(vgik_service_t *service);

/**
 * @brief Отмена регистрации сервиса
 * 
 * @param name Имя сервиса
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_service_unregister(const char *name);

/**
 * @brief Запуск сервиса
 * 
 * @param name Имя сервиса
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_service_start(const char *name);

/**
 * @brief Остановка сервиса
 * 
 * @param name Имя сервиса
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_service_stop(const char *name);

/**
 * @brief Перезапуск сервиса
 * 
 * @param name Имя сервиса
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_service_restart(const char *name);

/**
 * @brief Получение состояния сервиса
 * 
 * @param name Имя сервиса
 * @return Состояние сервиса или VGIK_SERVICE_STATE_FAILED если не найден
 */
vgik_service_state_t vgik_service_get_state(const char *name);

/**
 * @brief Проверка здоровья сервиса
 * 
 * @param name Имя сервиса
 * @return true если сервис здоров, false иначе
 */
bool vgik_service_is_healthy(const char *name);

/**
 * @brief Запуск всех зарегистрированных сервисов
 * 
 * @return Количество успешно запущенных сервисов
 */
int vgik_service_start_all(void);

/**
 * @brief Остановка всех сервисов
 * 
 * @return Количество успешно остановленных сервисов
 */
int vgik_service_stop_all(void);

/**
 * @brief Обработка менеджера сервисов
 * 
 * Должна вызываться периодически из основного цикла.
 * Выполняет проверку здоровья сервисов и автоматический перезапуск.
 */
void vgik_service_manager_process(void);

/**
 * @brief Получение списка всех сервисов
 * 
 * @param names Массив для хранения имен (выделяется вызывающим)
 * @param max_count Максимальное количество имен
 * @return Количество сервисов
 */
int vgik_service_list(const char **names, int max_count);

#ifdef __cplusplus
}
#endif

#endif /* VGIK_SERVICE_MANAGER_H */

