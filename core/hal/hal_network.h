/**
 * @file hal_network.h
 * @brief Аппаратный слой абстракции для сетевых интерфейсов
 */

#ifndef VGIK_HAL_NETWORK_H
#define VGIK_HAL_NETWORK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Типы сетевых интерфейсов
 */
typedef enum {
    VGIK_NETWORK_IF_TYPE_ETHERNET,
    VGIK_NETWORK_IF_TYPE_WIFI,
    VGIK_NETWORK_IF_TYPE_PPP,
    VGIK_NETWORK_IF_TYPE_VLAN,
    VGIK_NETWORK_IF_TYPE_BRIDGE,
    VGIK_NETWORK_IF_TYPE_UNKNOWN
} vgik_network_if_type_t;

/**
 * @brief Состояния интерфейса
 */
typedef enum {
    VGIK_NETWORK_IF_STATE_DOWN,
    VGIK_NETWORK_IF_STATE_UP,
    VGIK_NETWORK_IF_STATE_UNKNOWN
} vgik_network_if_state_t;

/**
 * @brief Конфигурация IP-адреса
 */
typedef struct {
    uint32_t address;      // IPv4 адрес в сетевом порядке байт
    uint32_t netmask;      // Маска подсети
    uint32_t gateway;      // Шлюз по умолчанию
    bool dhcp_enabled;     // Использовать DHCP
} vgik_network_ip_config_t;

/**
 * @brief Статистика интерфейса
 */
typedef struct {
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_errors;
    uint64_t tx_errors;
    uint64_t rx_dropped;
    uint64_t tx_dropped;
} vgik_network_if_stats_t;

/**
 * @brief Информация об интерфейсе
 */
typedef struct {
    char name[32];
    vgik_network_if_type_t type;
    vgik_network_if_state_t state;
    char mac_address[18];  // MAC адрес в формате "XX:XX:XX:XX:XX:XX"
    uint32_t mtu;
    vgik_network_ip_config_t ip_config;
    vgik_network_if_stats_t stats;
} vgik_network_if_info_t;

/**
 * @brief Список интерфейсов
 */
typedef struct {
    vgik_network_if_info_t *interfaces;
    size_t count;
    size_t capacity;
} vgik_network_if_list_t;

/**
 * @brief Структура HAL для сетевых интерфейсов
 */
typedef struct {
    /**
     * @brief Инициализация сетевого HAL
     */
    int (*init)(void);
    
    /**
     * @brief Деинициализация сетевого HAL
     */
    void (*deinit)(void);
    
    /**
     * @brief Получение списка интерфейсов
     */
    int (*get_interfaces)(vgik_network_if_list_t *list);
    
    /**
     * @brief Освобождение списка интерфейсов
     */
    void (*free_interfaces)(vgik_network_if_list_t *list);
    
    /**
     * @brief Получение информации об интерфейсе
     */
    int (*get_interface_info)(const char *name, vgik_network_if_info_t *info);
    
    /**
     * @brief Настройка интерфейса
     */
    int (*configure_interface)(const char *name, const vgik_network_ip_config_t *config);
    
    /**
     * @brief Включение/выключение интерфейса
     */
    int (*set_interface_state)(const char *name, vgik_network_if_state_t state);
    
    /**
     * @brief Получение статистики интерфейса
     */
    int (*get_interface_stats)(const char *name, vgik_network_if_stats_t *stats);
    
    /**
     * @brief Установка MTU
     */
    int (*set_mtu)(const char *name, uint32_t mtu);
} vgik_network_hal_t;

/**
 * @brief Получение реализации сетевого HAL
 * 
 * @return Указатель на структуру HAL или NULL при ошибке
 */
const vgik_network_hal_t* vgik_network_hal_get(void);

#ifdef __cplusplus
}
#endif

#endif /* VGIK_HAL_NETWORK_H */

