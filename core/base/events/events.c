/**
 * @file events.c
 * @brief Реализация системы событий VGIK
 */

#include "events.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#define VGIK_EVENTS_MAX_SUBSCRIPTIONS 128
#define VGIK_EVENTS_QUEUE_SIZE 256

/**
 * @brief Структура подписки
 */
typedef struct {
    vgik_event_subscription_id_t id;
    vgik_event_type_t type;
    vgik_event_handler_t handler;
    void *user_data;
    bool active;
} event_subscription_t;

/**
 * @brief Узел очереди событий
 */
typedef struct event_queue_node {
    vgik_event_t event;
    struct event_queue_node *next;
} event_queue_node_t;

/**
 * @brief Глобальное состояние системы событий
 */
static struct {
    bool initialized;
    event_subscription_t subscriptions[VGIK_EVENTS_MAX_SUBSCRIPTIONS];
    vgik_event_subscription_id_t next_subscription_id;
    
    // Очередь событий (приоритетная)
    event_queue_node_t *queue_head;
    event_queue_node_t *queue_tail;
    size_t queue_size;
    
    // Пул узлов для переиспользования
    event_queue_node_t *node_pool;
} g_events = {
    .initialized = false,
    .next_subscription_id = 1,
    .queue_head = NULL,
    .queue_tail = NULL,
    .queue_size = 0,
    .node_pool = NULL
};

/**
 * @brief Получение текущего времени в микросекундах
 */
static uint64_t get_timestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Создание узла очереди
 */
static event_queue_node_t* create_queue_node(void) {
    event_queue_node_t *node;
    
    // Попытка переиспользовать узел из пула
    if (g_events.node_pool) {
        node = g_events.node_pool;
        g_events.node_pool = node->next;
        node->next = NULL;
    } else {
        node = malloc(sizeof(event_queue_node_t));
        if (!node) {
            return NULL;
        }
    }
    
    return node;
}

/**
 * @brief Освобождение узла очереди
 */
static void free_queue_node(event_queue_node_t *node) {
    if (!node) {
        return;
    }
    
    // Освобождение данных события
    if (node->event.data) {
        free(node->event.data);
        node->event.data = NULL;
    }
    
    // Добавление в пул для переиспользования
    node->next = g_events.node_pool;
    g_events.node_pool = node;
}

/**
 * @brief Вставка события в очередь с учетом приоритета
 */
static int enqueue_event(const vgik_event_t *event) {
    if (g_events.queue_size >= VGIK_EVENTS_QUEUE_SIZE) {
        fprintf(stderr, "[EVENTS] Queue full, dropping event type=%d\n", event->type);
        return -1;
    }
    
    event_queue_node_t *node = create_queue_node();
    if (!node) {
        return -1;
    }
    
    // Копирование события
    node->event = *event;
    node->event.timestamp = get_timestamp();
    
    // Копирование данных если есть
    if (event->data && event->data_size > 0) {
        node->event.data = malloc(event->data_size);
        if (!node->event.data) {
            free(node);
            return -1;
        }
        memcpy(node->event.data, event->data, event->data_size);
    } else {
        node->event.data = NULL;
    }
    
    node->next = NULL;
    
    // Вставка в очередь с учетом приоритета
    if (!g_events.queue_head) {
        // Пустая очередь
        g_events.queue_head = node;
        g_events.queue_tail = node;
    } else {
        // Поиск места для вставки по приоритету
        event_queue_node_t *prev = NULL;
        event_queue_node_t *current = g_events.queue_head;
        
        while (current && current->event.priority >= event->priority) {
            prev = current;
            current = current->next;
        }
        
        if (prev) {
            node->next = prev->next;
            prev->next = node;
            if (!node->next) {
                g_events.queue_tail = node;
            }
        } else {
            // Вставка в начало
            node->next = g_events.queue_head;
            g_events.queue_head = node;
        }
    }
    
    g_events.queue_size++;
    return 0;
}

/**
 * @brief Извлечение события из очереди
 */
static int dequeue_event(vgik_event_t *event) {
    if (!g_events.queue_head || !event) {
        return -1;
    }
    
    event_queue_node_t *node = g_events.queue_head;
    g_events.queue_head = node->next;
    
    if (!g_events.queue_head) {
        g_events.queue_tail = NULL;
    }
    
    *event = node->event;
    // Не копируем данные - они остаются в узле
    node->event.data = NULL;
    
    free_queue_node(node);
    g_events.queue_size--;
    
    return 0;
}

int vgik_events_init(void) {
    if (g_events.initialized) {
        return 0;
    }
    
    g_events.initialized = true;
    fprintf(stderr, "[EVENTS] Event system initialized\n");
    return 0;
}

void vgik_events_deinit(void) {
    if (!g_events.initialized) {
        return;
    }
    
    // Очистка очереди
    vgik_events_clear();
    
    // Очистка пула узлов
    event_queue_node_t *node = g_events.node_pool;
    while (node) {
        event_queue_node_t *next = node->next;
        free(node);
        node = next;
    }
    g_events.node_pool = NULL;
    
    // Отписка всех подписчиков
    for (int i = 0; i < VGIK_EVENTS_MAX_SUBSCRIPTIONS; i++) {
        g_events.subscriptions[i].active = false;
    }
    
    g_events.initialized = false;
    fprintf(stderr, "[EVENTS] Event system deinitialized\n");
}

vgik_event_subscription_id_t vgik_event_subscribe(vgik_event_type_t type,
                                                   vgik_event_handler_t handler,
                                                   void *user_data) {
    if (!handler || !g_events.initialized) {
        return -1;
    }
    
    // Поиск свободного слота
    for (int i = 0; i < VGIK_EVENTS_MAX_SUBSCRIPTIONS; i++) {
        if (!g_events.subscriptions[i].active) {
            event_subscription_t *sub = &g_events.subscriptions[i];
            sub->id = g_events.next_subscription_id++;
            sub->type = type;
            sub->handler = handler;
            sub->user_data = user_data;
            sub->active = true;
            
            fprintf(stderr, "[EVENTS] Subscription created: id=%d, type=%d\n", 
                   sub->id, type);
            return sub->id;
        }
    }
    
    fprintf(stderr, "[EVENTS] No free subscription slots\n");
    return -1;
}

int vgik_event_unsubscribe(vgik_event_subscription_id_t subscription_id) {
    for (int i = 0; i < VGIK_EVENTS_MAX_SUBSCRIPTIONS; i++) {
        if (g_events.subscriptions[i].active && 
            g_events.subscriptions[i].id == subscription_id) {
            g_events.subscriptions[i].active = false;
            fprintf(stderr, "[EVENTS] Subscription removed: id=%d\n", subscription_id);
            return 0;
        }
    }
    
    fprintf(stderr, "[EVENTS] Subscription not found: id=%d\n", subscription_id);
    return -1;
}

int vgik_event_publish(const vgik_event_t *event) {
    if (!event || !g_events.initialized) {
        return -1;
    }
    
    // Добавление в очередь
    if (enqueue_event(event) != 0) {
        return -1;
    }
    
    return 0;
}

int vgik_event_publish_simple(vgik_event_type_t type,
                              vgik_event_priority_t priority,
                              const void *data,
                              size_t data_size,
                              const char *source) {
    vgik_event_t event = {
        .type = type,
        .priority = priority,
        .timestamp = 0,  // Установится при добавлении в очередь
        .data = NULL,
        .data_size = data_size,
        .source = {0}
    };
    
    if (source) {
        strncpy(event.source, source, sizeof(event.source) - 1);
        event.source[sizeof(event.source) - 1] = '\0';
    }
    
    // Копирование данных если есть
    if (data && data_size > 0) {
        event.data = malloc(data_size);
        if (!event.data) {
            return -1;
        }
        memcpy(event.data, data, data_size);
    }
    
    int ret = vgik_event_publish(&event);
    
    // Освобождение данных если они были скопированы
    if (event.data) {
        free(event.data);
    }
    
    return ret;
}

int vgik_events_process(void) {
    if (!g_events.initialized) {
        return 0;
    }
    
    int processed = 0;
    vgik_event_t event;
    
    // Обработка всех событий в очереди
    while (dequeue_event(&event) == 0) {
        // Поиск подписчиков для этого типа события
        for (int i = 0; i < VGIK_EVENTS_MAX_SUBSCRIPTIONS; i++) {
            event_subscription_t *sub = &g_events.subscriptions[i];
            if (!sub->active) {
                continue;
            }
            
            // Проверка соответствия типа (VGIK_EVENT_CUSTOM означает все события)
            if (sub->type != VGIK_EVENT_CUSTOM && sub->type != event.type) {
                continue;
            }
            
            // Вызов обработчика
            if (sub->handler) {
                sub->handler(&event, sub->user_data);
            }
        }
        
        // Освобождение данных события
        if (event.data) {
            free(event.data);
            event.data = NULL;
        }
        
        processed++;
    }
    
    return processed;
}

void vgik_events_clear(void) {
    vgik_event_t event;
    while (dequeue_event(&event) == 0) {
        if (event.data) {
            free(event.data);
        }
    }
}

size_t vgik_events_queue_size(void) {
    return g_events.queue_size;
}

