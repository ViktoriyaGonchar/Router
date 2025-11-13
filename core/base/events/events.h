/**
 * @file events.h
 * @brief Система событий VGIK
 * 
 * Предоставляет pub/sub механизм для асинхронной коммуникации между компонентами
 */

#ifndef VGIK_EVENTS_H
#define VGIK_EVENTS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Типы событий
 */
typedef enum {
    VGIK_EVENT_NETWORK_INTERFACE_UP,
    VGIK_EVENT_NETWORK_INTERFACE_DOWN,
    VGIK_EVENT_NETWORK_CONNECTION_ESTABLISHED,
    VGIK_EVENT_NETWORK_CONNECTION_LOST,
    VGIK_EVENT_CONFIG_CHANGED,
    VGIK_EVENT_FIRMWARE_UPDATE_STARTED,
    VGIK_EVENT_FIRMWARE_UPDATE_COMPLETED,
    VGIK_EVENT_FIRMWARE_UPDATE_FAILED,
    VGIK_EVENT_SERVICE_STARTED,
    VGIK_EVENT_SERVICE_STOPPED,
    VGIK_EVENT_SERVICE_CRASHED,
    VGIK_EVENT_SYSTEM_REBOOT,
    VGIK_EVENT_CUSTOM,  // Для пользовательских событий
    VGIK_EVENT_MAX
} vgik_event_type_t;

/**
 * @brief Приоритеты событий
 */
typedef enum {
    VGIK_EVENT_PRIORITY_LOW = 0,
    VGIK_EVENT_PRIORITY_NORMAL = 1,
    VGIK_EVENT_PRIORITY_HIGH = 2,
    VGIK_EVENT_PRIORITY_CRITICAL = 3
} vgik_event_priority_t;

/**
 * @brief Структура события
 */
typedef struct {
    vgik_event_type_t type;
    vgik_event_priority_t priority;
    uint64_t timestamp;
    void *data;
    size_t data_size;
    char source[64];  // Источник события
} vgik_event_t;

/**
 * @brief Callback для обработки событий
 * 
 * @param event Событие
 * @param user_data Пользовательские данные
 */
typedef void (*vgik_event_handler_t)(const vgik_event_t *event, void *user_data);

/**
 * @brief ID подписки
 */
typedef int vgik_event_subscription_id_t;

/**
 * @brief Инициализация системы событий
 * 
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_events_init(void);

/**
 * @brief Деинициализация системы событий
 */
void vgik_events_deinit(void);

/**
 * @brief Подписка на события определенного типа
 * 
 * @param type Тип события (VGIK_EVENT_CUSTOM для всех событий)
 * @param handler Обработчик события
 * @param user_data Пользовательские данные
 * @return ID подписки или отрицательное значение при ошибке
 */
vgik_event_subscription_id_t vgik_event_subscribe(vgik_event_type_t type,
                                                   vgik_event_handler_t handler,
                                                   void *user_data);

/**
 * @brief Отписка от событий
 * 
 * @param subscription_id ID подписки
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_event_unsubscribe(vgik_event_subscription_id_t subscription_id);

/**
 * @brief Публикация события
 * 
 * @param event Событие для публикации
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_event_publish(const vgik_event_t *event);

/**
 * @brief Публикация события с автоматическим созданием структуры
 * 
 * @param type Тип события
 * @param priority Приоритет
 * @param data Данные события (может быть NULL)
 * @param data_size Размер данных
 * @param source Источник события
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_event_publish_simple(vgik_event_type_t type,
                              vgik_event_priority_t priority,
                              const void *data,
                              size_t data_size,
                              const char *source);

/**
 * @brief Обработка очереди событий
 * 
 * Вызывает обработчики для всех событий в очереди.
 * Должна вызываться периодически из основного цикла.
 * 
 * @return Количество обработанных событий
 */
int vgik_events_process(void);

/**
 * @brief Очистка очереди событий
 */
void vgik_events_clear(void);

/**
 * @brief Получение количества событий в очереди
 */
size_t vgik_events_queue_size(void);

#ifdef __cplusplus
}
#endif

#endif /* VGIK_EVENTS_H */

