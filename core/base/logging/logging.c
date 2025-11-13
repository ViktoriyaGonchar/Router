/**
 * @file logging.c
 * @brief Реализация системы логирования VGIK
 */

#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define VGIK_LOG_MAX_MODULE_LEN 32
#define VGIK_LOG_MAX_MESSAGE_LEN 512
#define VGIK_LOG_DEFAULT_MAX_SIZE (10 * 1024 * 1024)  // 10 MB
#define VGIK_LOG_DEFAULT_MAX_FILES 5

/**
 * @brief Глобальное состояние логирования
 */
static struct {
    bool initialized;
    vgik_log_level_t level;
    int targets;
    
    // Файловое логирование
    FILE *log_file;
    char log_file_path[256];
    size_t current_file_size;
    size_t max_file_size;
    int max_files;
    int current_rotation;
    
    // Удалённое логирование
    bool remote_enabled;
    char remote_host[256];
    uint16_t remote_port;
    char remote_protocol[8];
} g_log = {
    .initialized = false,
    .level = VGIK_LOG_INFO,
    .targets = VGIK_LOG_TARGET_CONSOLE,
    .log_file = NULL,
    .log_file_path = {0},
    .current_file_size = 0,
    .max_file_size = VGIK_LOG_DEFAULT_MAX_SIZE,
    .max_files = VGIK_LOG_DEFAULT_MAX_FILES,
    .current_rotation = 0,
    .remote_enabled = false,
    .remote_port = 0
};

/**
 * @brief Получение строки уровня логирования
 */
static const char* get_level_string(vgik_log_level_t level) {
    switch (level) {
        case VGIK_LOG_DEBUG: return "DEBUG";
        case VGIK_LOG_INFO:  return "INFO";
        case VGIK_LOG_WARN:  return "WARN";
        case VGIK_LOG_ERROR: return "ERROR";
        case VGIK_LOG_FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Получение текущего времени в формате строки
 */
static void get_timestamp_string(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/**
 * @brief Ротация файла логов
 */
static int rotate_log_file(void) {
    if (!g_log.log_file) {
        return 0;
    }
    
    fclose(g_log.log_file);
    g_log.log_file = NULL;
    
    // Переименование текущего файла
    char old_name[256];
    g_log.current_rotation++;
    
    if (g_log.current_rotation >= g_log.max_files) {
        g_log.current_rotation = 0;
    }
    
    snprintf(old_name, sizeof(old_name), "%s.%d", 
             g_log.log_file_path, g_log.current_rotation);
    
    // Удаление старого файла если он существует
    remove(old_name);
    
    // Переименование текущего файла
    if (g_log.current_rotation > 0) {
        char prev_name[256];
        snprintf(prev_name, sizeof(prev_name), "%s.%d", 
                 g_log.log_file_path, g_log.current_rotation - 1);
        rename(prev_name, old_name);
    } else {
        rename(g_log.log_file_path, old_name);
    }
    
    // Открытие нового файла
    g_log.log_file = fopen(g_log.log_file_path, "a");
    if (!g_log.log_file) {
        fprintf(stderr, "[LOG] Failed to reopen log file: %s\n", 
                strerror(errno));
        return -1;
    }
    
    g_log.current_file_size = 0;
    return 0;
}

/**
 * @brief Запись в файл логов
 */
static void write_to_file(vgik_log_level_t level, 
                          const char *module,
                          const char *message) {
    if (!g_log.log_file) {
        return;
    }
    
    char timestamp[64];
    get_timestamp_string(timestamp, sizeof(timestamp));
    
    const char *level_str = get_level_string(level);
    const char *module_str = module ? module : "SYSTEM";
    
    size_t written = fprintf(g_log.log_file, "[%s] [%s] [%s] %s\n",
                             timestamp, level_str, module_str, message);
    
    fflush(g_log.log_file);
    
    g_log.current_file_size += written;
    
    // Ротация при превышении размера
    if (g_log.current_file_size >= g_log.max_file_size) {
        rotate_log_file();
    }
}

/**
 * @brief Отправка в syslog (заглушка)
 */
static void write_to_syslog(vgik_log_level_t level,
                            const char *module,
                            const char *message) {
    // TODO: Реализовать отправку в syslog
    (void)level;
    (void)module;
    (void)message;
}

/**
 * @brief Отправка на удалённый сервер (заглушка)
 */
static void write_to_remote(vgik_log_level_t level,
                            const char *module,
                            const char *message) {
    // TODO: Реализовать удалённую отправку
    (void)level;
    (void)module;
    (void)message;
}

int vgik_log_init(const char *log_file, 
                  vgik_log_level_t level,
                  int targets) {
    if (g_log.initialized) {
        return 0;
    }
    
    g_log.level = level;
    g_log.targets = targets;
    
    // Открытие файла логов если нужно
    if ((targets & VGIK_LOG_TARGET_FILE) && log_file) {
        strncpy(g_log.log_file_path, log_file, sizeof(g_log.log_file_path) - 1);
        g_log.log_file_path[sizeof(g_log.log_file_path) - 1] = '\0';
        
        g_log.log_file = fopen(log_file, "a");
        if (!g_log.log_file) {
            fprintf(stderr, "[LOG] Failed to open log file: %s (%s)\n",
                    log_file, strerror(errno));
            return -1;
        }
        
        // Получение текущего размера файла
        fseek(g_log.log_file, 0, SEEK_END);
        g_log.current_file_size = ftell(g_log.log_file);
        fseek(g_log.log_file, 0, SEEK_END);
    }
    
    g_log.initialized = true;
    return 0;
}

void vgik_log_deinit(void) {
    if (!g_log.initialized) {
        return;
    }
    
    if (g_log.log_file) {
        fclose(g_log.log_file);
        g_log.log_file = NULL;
    }
    
    g_log.initialized = false;
}

void vgik_log(vgik_log_level_t level, 
              const char *module,
              const char *format, ...) {
    if (!g_log.initialized || level < g_log.level) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    vgik_logv(level, module, format, args);
    va_end(args);
}

void vgik_logv(vgik_log_level_t level,
               const char *module,
               const char *format,
               va_list args) {
    if (!g_log.initialized || level < g_log.level) {
        return;
    }
    
    char message[VGIK_LOG_MAX_MESSAGE_LEN];
    vsnprintf(message, sizeof(message), format, args);
    
    // Запись в консоль
    if (g_log.targets & VGIK_LOG_TARGET_CONSOLE) {
        char timestamp[64];
        get_timestamp_string(timestamp, sizeof(timestamp));
        const char *level_str = get_level_string(level);
        const char *module_str = module ? module : "SYSTEM";
        
        fprintf(stderr, "[%s] [%s] [%s] %s\n",
                timestamp, level_str, module_str, message);
    }
    
    // Запись в файл
    if (g_log.targets & VGIK_LOG_TARGET_FILE) {
        write_to_file(level, module, message);
    }
    
    // Отправка в syslog
    if (g_log.targets & VGIK_LOG_TARGET_SYSLOG) {
        write_to_syslog(level, module, message);
    }
    
    // Отправка на удалённый сервер
    if (g_log.targets & VGIK_LOG_TARGET_REMOTE && g_log.remote_enabled) {
        write_to_remote(level, module, message);
    }
}

void vgik_log_set_level(vgik_log_level_t level) {
    g_log.level = level;
}

vgik_log_level_t vgik_log_get_level(void) {
    return g_log.level;
}

void vgik_log_set_targets(int targets) {
    g_log.targets = targets;
}

int vgik_log_set_remote(const char *host, uint16_t port, const char *protocol) {
    if (!host || !protocol) {
        return -1;
    }
    
    strncpy(g_log.remote_host, host, sizeof(g_log.remote_host) - 1);
    g_log.remote_host[sizeof(g_log.remote_host) - 1] = '\0';
    g_log.remote_port = port;
    strncpy(g_log.remote_protocol, protocol, sizeof(g_log.remote_protocol) - 1);
    g_log.remote_protocol[sizeof(g_log.remote_protocol) - 1] = '\0';
    g_log.remote_enabled = true;
    
    return 0;
}

int vgik_log_set_rotation(size_t max_size, int max_files) {
    g_log.max_file_size = max_size;
    g_log.max_files = max_files;
    return 0;
}

const char* vgik_log_get_filename(int rotation_num) {
    static char filename[256];
    if (rotation_num == 0) {
        return g_log.log_file_path;
    }
    snprintf(filename, sizeof(filename), "%s.%d", 
             g_log.log_file_path, rotation_num);
    return filename;
}

