/**
 * @file config.h
 * @brief Конфигурационный движок VGIK
 * 
 * Предоставляет API для загрузки, валидации и применения конфигураций
 * в формате JSON/YAML.
 */

#ifndef VGIK_CONFIG_H
#define VGIK_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Типы конфигурационных значений
 */
typedef enum {
    VGIK_CONFIG_TYPE_NULL,
    VGIK_CONFIG_TYPE_BOOL,
    VGIK_CONFIG_TYPE_INT,
    VGIK_CONFIG_TYPE_DOUBLE,
    VGIK_CONFIG_TYPE_STRING,
    VGIK_CONFIG_TYPE_OBJECT,
    VGIK_CONFIG_TYPE_ARRAY
} vgik_config_type_t;

/**
 * @brief Уровень логирования конфигурации
 */
typedef enum {
    VGIK_CONFIG_LOG_NONE = 0,
    VGIK_CONFIG_LOG_ERROR = 1,
    VGIK_CONFIG_LOG_WARN = 2,
    VGIK_CONFIG_LOG_INFO = 3,
    VGIK_CONFIG_LOG_DEBUG = 4
} vgik_config_log_level_t;

/**
 * @brief Структура конфигурационного значения
 */
typedef struct vgik_config_value vgik_config_value_t;

/**
 * @brief Структура конфигурации
 */
typedef struct vgik_config vgik_config_t;

/**
 * @brief Callback для уведомлений об изменениях
 */
typedef void (*vgik_config_change_callback_t)(const char *key, 
                                               vgik_config_value_t *old_value,
                                               vgik_config_value_t *new_value,
                                               void *user_data);

/**
 * @brief Инициализация конфигурационного движка
 * 
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_config_init(void);

/**
 * @brief Деинициализация конфигурационного движка
 */
void vgik_config_deinit(void);

/**
 * @brief Загрузка конфигурации из файла
 * 
 * @param path Путь к файлу конфигурации (JSON или YAML)
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_config_load(const char *path);

/**
 * @brief Загрузка конфигурации из строки
 * 
 * @param data Строка с конфигурацией (JSON)
 * @param size Размер строки
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_config_load_from_string(const char *data, size_t size);

/**
 * @brief Валидация конфигурации по схеме
 * 
 * @param schema_path Путь к файлу JSON Schema
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_config_validate(const char *schema_path);

/**
 * @brief Валидация конфигурации по схеме из строки
 * 
 * @param schema_data Строка с JSON Schema
 * @param schema_size Размер строки
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_config_validate_from_string(const char *schema_data, size_t schema_size);

/**
 * @brief Применение конфигурации
 * 
 * Применяет загруженную конфигурацию к системе.
 * Изменения применяются транзакционно - при ошибке выполняется откат.
 * 
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_config_apply(void);

/**
 * @brief Откат последних изменений
 * 
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_config_rollback(void);

/**
 * @brief Получение значения конфигурации по ключу
 * 
 * @param key Путь к значению (например, "network.interfaces.0.name")
 * @return Указатель на значение или NULL если не найдено
 */
vgik_config_value_t* vgik_config_get(const char *key);

/**
 * @brief Установка значения конфигурации
 * 
 * @param key Путь к значению
 * @param value Новое значение
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_config_set(const char *key, vgik_config_value_t *value);

/**
 * @brief Сохранение конфигурации в файл
 * 
 * @param path Путь к файлу для сохранения
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_config_save(const char *path);

/**
 * @brief Получение версии конфигурации
 * 
 * @return Версия конфигурации или NULL
 */
const char* vgik_config_get_version(void);

/**
 * @brief Создание нового значения конфигурации
 */
vgik_config_value_t* vgik_config_value_create(vgik_config_type_t type);

/**
 * @brief Удаление значения конфигурации
 */
void vgik_config_value_destroy(vgik_config_value_t *value);

/**
 * @brief Получение типа значения
 */
vgik_config_type_t vgik_config_value_get_type(vgik_config_value_t *value);

/**
 * @brief Получение булевого значения
 */
bool vgik_config_value_get_bool(vgik_config_value_t *value);

/**
 * @brief Получение целочисленного значения
 */
int64_t vgik_config_value_get_int(vgik_config_value_t *value);

/**
 * @brief Получение значения с плавающей точкой
 */
double vgik_config_value_get_double(vg_config_value_t *value);

/**
 * @brief Получение строкового значения
 */
const char* vgik_config_value_get_string(vgik_config_value_t *value);

/**
 * @brief Установка булевого значения
 */
int vgik_config_value_set_bool(vgik_config_value_t *value, bool b);

/**
 * @brief Установка целочисленного значения
 */
int vgik_config_value_set_int(vgik_config_value_t *value, int64_t i);

/**
 * @brief Установка значения с плавающей точкой
 */
int vgik_config_value_set_double(vgik_config_value_t *value, double d);

/**
 * @brief Установка строкового значения
 */
int vgik_config_value_set_string(vgik_config_value_t *value, const char *s);

/**
 * @brief Подписка на изменения конфигурации
 * 
 * @param key Ключ для отслеживания (NULL для всех изменений)
 * @param callback Callback функция
 * @param user_data Пользовательские данные
 * @return ID подписки или отрицательное значение при ошибке
 */
int vgik_config_subscribe(const char *key, 
                          vgik_config_change_callback_t callback,
                          void *user_data);

/**
 * @brief Отписка от изменений конфигурации
 * 
 * @param subscription_id ID подписки
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_config_unsubscribe(int subscription_id);

/**
 * @brief Установка уровня логирования
 */
void vgik_config_set_log_level(vgik_config_log_level_t level);

#ifdef __cplusplus
}
#endif

#endif /* VGIK_CONFIG_H */

