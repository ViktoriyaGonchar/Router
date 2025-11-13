/**
 * @file logging.h
 * @brief Система логирования VGIK
 * 
 * Предоставляет многоуровневое логирование с поддержкой ротации и удалённой отправки
 */

#ifndef VGIK_LOGGING_H
#define VGIK_LOGGING_H

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Уровни логирования
 */
typedef enum {
    VGIK_LOG_DEBUG = 0,
    VGIK_LOG_INFO = 1,
    VGIK_LOG_WARN = 2,
    VGIK_LOG_ERROR = 3,
    VGIK_LOG_FATAL = 4,
    VGIK_LOG_OFF = 5
} vgik_log_level_t;

/**
 * @brief Цели логирования
 */
typedef enum {
    VGIK_LOG_TARGET_CONSOLE = 0x01,
    VGIK_LOG_TARGET_FILE = 0x02,
    VGIK_LOG_TARGET_SYSLOG = 0x04,
    VGIK_LOG_TARGET_REMOTE = 0x08
} vgik_log_target_t;

/**
 * @brief Инициализация системы логирования
 * 
 * @param log_file Путь к файлу логов (может быть NULL)
 * @param level Минимальный уровень логирования
 * @param targets Битовая маска целей логирования
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_log_init(const char *log_file, 
                  vgik_log_level_t level,
                  int targets);

/**
 * @brief Деинициализация системы логирования
 */
void vgik_log_deinit(void);

/**
 * @brief Основная функция логирования
 * 
 * @param level Уровень логирования
 * @param module Модуль (может быть NULL)
 * @param format Формат строки (как в printf)
 * @param ... Аргументы для формата
 */
void vgik_log(vgik_log_level_t level, 
              const char *module,
              const char *format, ...);

/**
 * @brief Логирование с вариативными аргументами
 */
void vgik_logv(vgik_log_level_t level,
               const char *module,
               const char *format,
               va_list args);

/**
 * @brief Макросы для удобства
 */
#define VGIK_LOG_DEBUG(module, ...) \
    vgik_log(VGIK_LOG_DEBUG, module, __VA_ARGS__)

#define VGIK_LOG_INFO(module, ...) \
    vgik_log(VGIK_LOG_INFO, module, __VA_ARGS__)

#define VGIK_LOG_WARN(module, ...) \
    vgik_log(VGIK_LOG_WARN, module, __VA_ARGS__)

#define VGIK_LOG_ERROR(module, ...) \
    vgik_log(VGIK_LOG_ERROR, module, __VA_ARGS__)

#define VGIK_LOG_FATAL(module, ...) \
    vgik_log(VGIK_LOG_FATAL, module, __VA_ARGS__)

/**
 * @brief Установка уровня логирования
 */
void vgik_log_set_level(vgik_log_level_t level);

/**
 * @brief Получение текущего уровня логирования
 */
vgik_log_level_t vgik_log_get_level(void);

/**
 * @brief Установка целей логирования
 */
void vgik_log_set_targets(int targets);

/**
 * @brief Настройка удалённого логирования
 * 
 * @param host Адрес сервера
 * @param port Порт сервера
 * @param protocol Протокол ("udp" или "tcp")
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_log_set_remote(const char *host, uint16_t port, const char *protocol);

/**
 * @brief Ротация логов
 * 
 * @param max_size Максимальный размер файла в байтах
 * @param max_files Максимальное количество файлов
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_log_set_rotation(size_t max_size, int max_files);

/**
 * @brief Получение имени файла лога с номером ротации
 */
const char* vgik_log_get_filename(int rotation_num);

#ifdef __cplusplus
}
#endif

#endif /* VGIK_LOGGING_H */

