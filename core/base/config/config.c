/**
 * @file config.c
 * @brief Реализация конфигурационного движка VGIK
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

// Используем cJSON для работы с JSON
// В реальной реализации нужно добавить зависимость
#define CJSON_IMPL_SOURCE
#include "cjson/cJSON.h"

#define VGIK_CONFIG_MAX_KEY_LEN 256
#define VGIK_CONFIG_MAX_SUBSCRIPTIONS 64

/**
 * @brief Структура подписки на изменения
 */
typedef struct {
    int id;
    char key[VGIK_CONFIG_MAX_KEY_LEN];
    vgik_config_change_callback_t callback;
    void *user_data;
    bool active;
} config_subscription_t;

/**
 * @brief Глобальное состояние конфигурации
 */
static struct {
    bool initialized;
    cJSON *root;
    cJSON *backup;  // Резервная копия для отката
    config_subscription_t subscriptions[VGIK_CONFIG_MAX_SUBSCRIPTIONS];
    int next_subscription_id;
    vgik_config_log_level_t log_level;
} g_config = {
    .initialized = false,
    .root = NULL,
    .backup = NULL,
    .next_subscription_id = 1,
    .log_level = VGIK_CONFIG_LOG_INFO
};

/**
 * @brief Логирование конфигурации
 */
static void config_log(vgik_config_log_level_t level, const char *format, ...) {
    if (level > g_config.log_level) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    
    const char *level_str = "UNKNOWN";
    switch (level) {
        case VGIK_CONFIG_LOG_ERROR: level_str = "ERROR"; break;
        case VGIK_CONFIG_LOG_WARN:  level_str = "WARN";  break;
        case VGIK_CONFIG_LOG_INFO:  level_str = "INFO";  break;
        case VGIK_CONFIG_LOG_DEBUG: level_str = "DEBUG"; break;
        default: break;
    }
    
    fprintf(stderr, "[CONFIG %s] ", level_str);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    
    va_end(args);
}

/**
 * @brief Уведомление подписчиков об изменении
 */
static void notify_subscribers(const char *key, cJSON *old_value, cJSON *new_value) {
    for (int i = 0; i < VGIK_CONFIG_MAX_SUBSCRIPTIONS; i++) {
        config_subscription_t *sub = &g_config.subscriptions[i];
        if (!sub->active) {
            continue;
        }
        
        // Проверка соответствия ключу (NULL означает все изменения)
        if (sub->key[0] != '\0' && strcmp(sub->key, key) != 0) {
            continue;
        }
        
        // Вызов callback
        if (sub->callback) {
            sub->callback(key, (vgik_config_value_t*)old_value, 
                         (vgik_config_value_t*)new_value, sub->user_data);
        }
    }
}

int vgik_config_init(void) {
    if (g_config.initialized) {
        config_log(VGIK_CONFIG_LOG_WARN, "Config already initialized");
        return 0;
    }
    
    g_config.root = cJSON_CreateObject();
    if (!g_config.root) {
        config_log(VGIK_CONFIG_LOG_ERROR, "Failed to create config root");
        return -1;
    }
    
    g_config.initialized = true;
    config_log(VGIK_CONFIG_LOG_INFO, "Config engine initialized");
    return 0;
}

void vgik_config_deinit(void) {
    if (!g_config.initialized) {
        return;
    }
    
    // Отписка всех подписчиков
    for (int i = 0; i < VGIK_CONFIG_MAX_SUBSCRIPTIONS; i++) {
        g_config.subscriptions[i].active = false;
    }
    
    if (g_config.root) {
        cJSON_Delete(g_config.root);
        g_config.root = NULL;
    }
    
    if (g_config.backup) {
        cJSON_Delete(g_config.backup);
        g_config.backup = NULL;
    }
    
    g_config.initialized = false;
    config_log(VGIK_CONFIG_LOG_INFO, "Config engine deinitialized");
}

int vgik_config_load(const char *path) {
    if (!g_config.initialized) {
        config_log(VGIK_CONFIG_LOG_ERROR, "Config not initialized");
        return -1;
    }
    
    if (!path) {
        config_log(VGIK_CONFIG_LOG_ERROR, "Invalid path");
        return -1;
    }
    
    FILE *f = fopen(path, "r");
    if (!f) {
        config_log(VGIK_CONFIG_LOG_ERROR, "Failed to open file: %s (%s)", 
                   path, strerror(errno));
        return -1;
    }
    
    // Чтение файла
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buffer = malloc(size + 1);
    if (!buffer) {
        fclose(f);
        config_log(VGIK_CONFIG_LOG_ERROR, "Failed to allocate memory");
        return -1;
    }
    
    size_t read_size = fread(buffer, 1, size, f);
    fclose(f);
    
    if (read_size != (size_t)size) {
        free(buffer);
        config_log(VGIK_CONFIG_LOG_ERROR, "Failed to read file completely");
        return -1;
    }
    
    buffer[size] = '\0';
    
    // Парсинг JSON
    cJSON *new_root = cJSON_Parse(buffer);
    free(buffer);
    
    if (!new_root) {
        const char *error_ptr = cJSON_GetErrorPtr();
        config_log(VGIK_CONFIG_LOG_ERROR, "Failed to parse JSON: %s", 
                   error_ptr ? error_ptr : "unknown error");
        return -1;
    }
    
    // Замена текущей конфигурации
    if (g_config.root) {
        cJSON_Delete(g_config.root);
    }
    g_config.root = new_root;
    
    config_log(VGIK_CONFIG_LOG_INFO, "Config loaded from: %s", path);
    return 0;
}

int vgik_config_load_from_string(const char *data, size_t size) {
    if (!g_config.initialized) {
        config_log(VGIK_CONFIG_LOG_ERROR, "Config not initialized");
        return -1;
    }
    
    if (!data) {
        config_log(VGIK_CONFIG_LOG_ERROR, "Invalid data");
        return -1;
    }
    
    // Создание нуль-терминированной строки
    char *buffer = malloc(size + 1);
    if (!buffer) {
        config_log(VGIK_CONFIG_LOG_ERROR, "Failed to allocate memory");
        return -1;
    }
    
    memcpy(buffer, data, size);
    buffer[size] = '\0';
    
    // Парсинг JSON
    cJSON *new_root = cJSON_Parse(buffer);
    free(buffer);
    
    if (!new_root) {
        const char *error_ptr = cJSON_GetErrorPtr();
        config_log(VGIK_CONFIG_LOG_ERROR, "Failed to parse JSON: %s", 
                   error_ptr ? error_ptr : "unknown error");
        return -1;
    }
    
    // Замена текущей конфигурации
    if (g_config.root) {
        cJSON_Delete(g_config.root);
    }
    g_config.root = new_root;
    
    config_log(VGIK_CONFIG_LOG_INFO, "Config loaded from string");
    return 0;
}

int vgik_config_validate(const char *schema_path) {
    // TODO: Реализовать валидацию через JSON Schema
    // Пока возвращаем успех
    config_log(VGIK_CONFIG_LOG_WARN, "Schema validation not yet implemented");
    return 0;
}

int vgik_config_validate_from_string(const char *schema_data, size_t schema_size) {
    // TODO: Реализовать валидацию через JSON Schema
    config_log(VGIK_CONFIG_LOG_WARN, "Schema validation not yet implemented");
    return 0;
}

int vgik_config_apply(void) {
    if (!g_config.initialized || !g_config.root) {
        config_log(VGIK_CONFIG_LOG_ERROR, "No config to apply");
        return -1;
    }
    
    // Создание резервной копии для возможного отката
    if (g_config.backup) {
        cJSON_Delete(g_config.backup);
    }
    g_config.backup = cJSON_Duplicate(g_config.root, 1);
    
    if (!g_config.backup) {
        config_log(VGIK_CONFIG_LOG_ERROR, "Failed to create backup");
        return -1;
    }
    
    // TODO: Применение конфигурации к системе
    // Здесь будет вызов различных сервисов для применения настроек
    
    config_log(VGIK_CONFIG_LOG_INFO, "Config applied successfully");
    return 0;
}

int vgik_config_rollback(void) {
    if (!g_config.backup) {
        config_log(VGIK_CONFIG_LOG_ERROR, "No backup to rollback");
        return -1;
    }
    
    if (g_config.root) {
        cJSON_Delete(g_config.root);
    }
    
    g_config.root = cJSON_Duplicate(g_config.backup, 1);
    if (!g_config.root) {
        config_log(VGIK_CONFIG_LOG_ERROR, "Failed to restore backup");
        return -1;
    }
    
    config_log(VGIK_CONFIG_LOG_INFO, "Config rolled back");
    return 0;
}

vgik_config_value_t* vgik_config_get(const char *key) {
    if (!g_config.initialized || !g_config.root || !key) {
        return NULL;
    }
    
    // Простой поиск по ключу (поддержка вложенных ключей через точку)
    // TODO: Реализовать полноценный парсинг пути (например, "network.interfaces.0.name")
    cJSON *item = cJSON_GetObjectItemCaseSensitive(g_config.root, key);
    
    return (vgik_config_value_t*)item;
}

int vgik_config_set(const char *key, vgik_config_value_t *value) {
    if (!g_config.initialized || !g_config.root || !key || !value) {
        return -1;
    }
    
    cJSON *old_value = cJSON_GetObjectItemCaseSensitive(g_config.root, key);
    cJSON *old_copy = old_value ? cJSON_Duplicate(old_value, 1) : NULL;
    
    // Установка нового значения
    cJSON *new_item = cJSON_DetachItemFromObject(g_config.root, key);
    if (new_item) {
        cJSON_Delete(new_item);
    }
    
    cJSON *new_value = cJSON_Duplicate((cJSON*)value, 1);
    cJSON_AddItemToObject(g_config.root, key, new_value);
    
    // Уведомление подписчиков
    notify_subscribers(key, old_copy, new_value);
    
    if (old_copy) {
        cJSON_Delete(old_copy);
    }
    
    config_log(VGIK_CONFIG_LOG_DEBUG, "Config value set: %s", key);
    return 0;
}

int vgik_config_save(const char *path) {
    if (!g_config.initialized || !g_config.root || !path) {
        return -1;
    }
    
    char *json_string = cJSON_Print(g_config.root);
    if (!json_string) {
        config_log(VGIK_CONFIG_LOG_ERROR, "Failed to serialize config");
        return -1;
    }
    
    FILE *f = fopen(path, "w");
    if (!f) {
        free(json_string);
        config_log(VGIK_CONFIG_LOG_ERROR, "Failed to open file for writing: %s", path);
        return -1;
    }
    
    fprintf(f, "%s", json_string);
    fclose(f);
    free(json_string);
    
    config_log(VGIK_CONFIG_LOG_INFO, "Config saved to: %s", path);
    return 0;
}

const char* vgik_config_get_version(void) {
    if (!g_config.initialized || !g_config.root) {
        return NULL;
    }
    
    cJSON *version = cJSON_GetObjectItemCaseSensitive(g_config.root, "version");
    if (version && cJSON_IsString(version)) {
        return version->valuestring;
    }
    
    return NULL;
}

vgik_config_value_t* vgik_config_value_create(vgik_config_type_t type) {
    cJSON *item = NULL;
    
    switch (type) {
        case VGIK_CONFIG_TYPE_NULL:
            item = cJSON_CreateNull();
            break;
        case VGIK_CONFIG_TYPE_BOOL:
            item = cJSON_CreateBool(false);
            break;
        case VGIK_CONFIG_TYPE_INT:
            item = cJSON_CreateNumber(0);
            break;
        case VGIK_CONFIG_TYPE_DOUBLE:
            item = cJSON_CreateNumber(0.0);
            break;
        case VGIK_CONFIG_TYPE_STRING:
            item = cJSON_CreateString("");
            break;
        case VGIK_CONFIG_TYPE_OBJECT:
            item = cJSON_CreateObject();
            break;
        case VGIK_CONFIG_TYPE_ARRAY:
            item = cJSON_CreateArray();
            break;
    }
    
    return (vgik_config_value_t*)item;
}

void vgik_config_value_destroy(vgik_config_value_t *value) {
    if (value) {
        cJSON_Delete((cJSON*)value);
    }
}

vgik_config_type_t vgik_config_value_get_type(vgik_config_value_t *value) {
    if (!value) {
        return VGIK_CONFIG_TYPE_NULL;
    }
    
    cJSON *item = (cJSON*)value;
    
    if (cJSON_IsNull(item)) return VGIK_CONFIG_TYPE_NULL;
    if (cJSON_IsBool(item)) return VGIK_CONFIG_TYPE_BOOL;
    if (cJSON_IsNumber(item)) {
        // Проверка, целое или дробное
        double num = item->valuedouble;
        if (num == (int64_t)num) {
            return VGIK_CONFIG_TYPE_INT;
        }
        return VGIK_CONFIG_TYPE_DOUBLE;
    }
    if (cJSON_IsString(item)) return VGIK_CONFIG_TYPE_STRING;
    if (cJSON_IsObject(item)) return VGIK_CONFIG_TYPE_OBJECT;
    if (cJSON_IsArray(item)) return VGIK_CONFIG_TYPE_ARRAY;
    
    return VGIK_CONFIG_TYPE_NULL;
}

bool vgik_config_value_get_bool(vgik_config_value_t *value) {
    if (!value) return false;
    cJSON *item = (cJSON*)value;
    return cJSON_IsTrue(item);
}

int64_t vgik_config_value_get_int(vgik_config_value_t *value) {
    if (!value) return 0;
    cJSON *item = (cJSON*)value;
    return (int64_t)item->valuedouble;
}

double vgik_config_value_get_double(vgik_config_value_t *value) {
    if (!value) return 0.0;
    cJSON *item = (cJSON*)value;
    return item->valuedouble;
}

const char* vgik_config_value_get_string(vgik_config_value_t *value) {
    if (!value) return NULL;
    cJSON *item = (cJSON*)value;
    if (cJSON_IsString(item)) {
        return item->valuestring;
    }
    return NULL;
}

int vgik_config_value_set_bool(vgik_config_value_t *value, bool b) {
    if (!value) return -1;
    cJSON *item = (cJSON*)value;
    cJSON_SetBoolValue(item, b);
    return 0;
}

int vgik_config_value_set_int(vgik_config_value_t *value, int64_t i) {
    if (!value) return -1;
    cJSON *item = (cJSON*)value;
    item->valuedouble = (double)i;
    item->valueint = (int)i;
    return 0;
}

int vgik_config_value_set_double(vgik_config_value_t *value, double d) {
    if (!value) return -1;
    cJSON *item = (cJSON*)value;
    item->valuedouble = d;
    return 0;
}

int vgik_config_value_set_string(vgik_config_value_t *value, const char *s) {
    if (!value || !s) return -1;
    cJSON *item = (cJSON*)value;
    cJSON_SetValuestring(item, s);
    return 0;
}

int vgik_config_subscribe(const char *key, 
                          vgik_config_change_callback_t callback,
                          void *user_data) {
    if (!callback) {
        return -1;
    }
    
    // Поиск свободного слота
    for (int i = 0; i < VGIK_CONFIG_MAX_SUBSCRIPTIONS; i++) {
        if (!g_config.subscriptions[i].active) {
            config_subscription_t *sub = &g_config.subscriptions[i];
            sub->id = g_config.next_subscription_id++;
            sub->callback = callback;
            sub->user_data = user_data;
            sub->active = true;
            
            if (key) {
                strncpy(sub->key, key, VGIK_CONFIG_MAX_KEY_LEN - 1);
                sub->key[VGIK_CONFIG_MAX_KEY_LEN - 1] = '\0';
            } else {
                sub->key[0] = '\0';
            }
            
            config_log(VGIK_CONFIG_LOG_DEBUG, "Subscription created: id=%d, key=%s", 
                      sub->id, key ? key : "all");
            return sub->id;
        }
    }
    
    config_log(VGIK_CONFIG_LOG_ERROR, "No free subscription slots");
    return -1;
}

int vgik_config_unsubscribe(int subscription_id) {
    for (int i = 0; i < VGIK_CONFIG_MAX_SUBSCRIPTIONS; i++) {
        if (g_config.subscriptions[i].active && 
            g_config.subscriptions[i].id == subscription_id) {
            g_config.subscriptions[i].active = false;
            config_log(VGIK_CONFIG_LOG_DEBUG, "Subscription removed: id=%d", subscription_id);
            return 0;
        }
    }
    
    config_log(VGIK_CONFIG_LOG_WARN, "Subscription not found: id=%d", subscription_id);
    return -1;
}

void vgik_config_set_log_level(vgik_config_log_level_t level) {
    g_config.log_level = level;
}

