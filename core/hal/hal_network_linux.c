/**
 * @file hal_network_linux.c
 * @brief Реализация сетевого HAL для Linux
 */

#include "hal_network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <linux/if_link.h>

#define VGIK_NETWORK_MAX_INTERFACES 32

/**
 * @brief Получение типа интерфейса по имени
 */
static vgik_network_if_type_t get_if_type(const char *name) {
    if (!name) {
        return VGIK_NETWORK_IF_TYPE_UNKNOWN;
    }
    
    // Определение типа по префиксу имени
    if (strncmp(name, "eth", 3) == 0 || strncmp(name, "enp", 3) == 0) {
        return VGIK_NETWORK_IF_TYPE_ETHERNET;
    }
    if (strncmp(name, "wlan", 4) == 0 || strncmp(name, "wlp", 3) == 0) {
        return VGIK_NETWORK_IF_TYPE_WIFI;
    }
    if (strncmp(name, "ppp", 3) == 0) {
        return VGIK_NETWORK_IF_TYPE_PPP;
    }
    if (strncmp(name, "vlan", 4) == 0) {
        return VGIK_NETWORK_IF_TYPE_VLAN;
    }
    if (strncmp(name, "br", 2) == 0) {
        return VGIK_NETWORK_IF_TYPE_BRIDGE;
    }
    
    return VGIK_NETWORK_IF_TYPE_UNKNOWN;
}

/**
 * @brief Получение MAC адреса интерфейса
 */
static int get_mac_address(const char *name, char *mac_str, size_t mac_str_size) {
    struct ifreq ifr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (sock < 0) {
        return -1;
    }
    
    strncpy(ifr.ifr_name, name, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    
    if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) {
        close(sock);
        return -1;
    }
    
    unsigned char *mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
    snprintf(mac_str, mac_str_size, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    close(sock);
    return 0;
}

/**
 * @brief Получение состояния интерфейса
 */
static vgik_network_if_state_t get_if_state(const char *name) {
    struct ifreq ifr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (sock < 0) {
        return VGIK_NETWORK_IF_STATE_UNKNOWN;
    }
    
    strncpy(ifr.ifr_name, name, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
        close(sock);
        return VGIK_NETWORK_IF_STATE_UNKNOWN;
    }
    
    close(sock);
    
    if (ifr.ifr_flags & IFF_UP) {
        return VGIK_NETWORK_IF_STATE_UP;
    }
    return VGIK_NETWORK_IF_STATE_DOWN;
}

/**
 * @brief Получение MTU интерфейса
 */
static int get_mtu(const char *name, uint32_t *mtu) {
    struct ifreq ifr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (sock < 0) {
        return -1;
    }
    
    strncpy(ifr.ifr_name, name, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    
    if (ioctl(sock, SIOCGIFMTU, &ifr) < 0) {
        close(sock);
        return -1;
    }
    
    *mtu = ifr.ifr_mtu;
    close(sock);
    return 0;
}

/**
 * @brief Инициализация сетевого HAL
 */
static int network_hal_init(void) {
    return 0;
}

/**
 * @brief Деинициализация сетевого HAL
 */
static void network_hal_deinit(void) {
    // Ничего не нужно делать
}

/**
 * @brief Получение списка интерфейсов
 */
static int network_hal_get_interfaces(vgik_network_if_list_t *list) {
    if (!list) {
        return -1;
    }
    
    struct ifaddrs *ifaddrs_ptr, *ifa;
    
    if (getifaddrs(&ifaddrs_ptr) != 0) {
        return -1;
    }
    
    // Подсчет интерфейсов
    size_t count = 0;
    for (ifa = ifaddrs_ptr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            count++;
        }
    }
    
    if (count == 0) {
        freeifaddrs(ifaddrs_ptr);
        list->interfaces = NULL;
        list->count = 0;
        list->capacity = 0;
        return 0;
    }
    
    // Выделение памяти
    list->interfaces = calloc(count, sizeof(vgik_network_if_info_t));
    if (!list->interfaces) {
        freeifaddrs(ifaddrs_ptr);
        return -1;
    }
    
    list->capacity = count;
    list->count = 0;
    
    // Заполнение информации об интерфейсах
    for (ifa = ifaddrs_ptr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        
        vgik_network_if_info_t *info = &list->interfaces[list->count];
        
        strncpy(info->name, ifa->ifa_name, sizeof(info->name) - 1);
        info->name[sizeof(info->name) - 1] = '\0';
        
        info->type = get_if_type(info->name);
        info->state = get_if_state(info->name);
        
        // MAC адрес
        if (get_mac_address(info->name, info->mac_address, sizeof(info->mac_address)) != 0) {
            info->mac_address[0] = '\0';
        }
        
        // MTU
        if (get_mtu(info->name, &info->mtu) != 0) {
            info->mtu = 1500;  // Значение по умолчанию
        }
        
        // IP конфигурация
        struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;
        info->ip_config.address = sin->sin_addr.s_addr;
        
        if (ifa->ifa_netmask) {
            struct sockaddr_in *netmask = (struct sockaddr_in *)ifa->ifa_netmask;
            info->ip_config.netmask = netmask->sin_addr.s_addr;
        }
        
        info->ip_config.gateway = 0;  // Нужно получать отдельно
        info->ip_config.dhcp_enabled = false;  // Нужно проверять отдельно
        
        // Обнуление статистики (получение отдельно)
        memset(&info->stats, 0, sizeof(info->stats));
        
        list->count++;
    }
    
    freeifaddrs(ifaddrs_ptr);
    return 0;
}

/**
 * @brief Освобождение списка интерфейсов
 */
static void network_hal_free_interfaces(vgik_network_if_list_t *list) {
    if (list && list->interfaces) {
        free(list->interfaces);
        list->interfaces = NULL;
        list->count = 0;
        list->capacity = 0;
    }
}

/**
 * @brief Получение информации об интерфейсе
 */
static int network_hal_get_interface_info(const char *name, vgik_network_if_info_t *info) {
    if (!name || !info) {
        return -1;
    }
    
    vgik_network_if_list_t list;
    if (network_hal_get_interfaces(&list) != 0) {
        return -1;
    }
    
    for (size_t i = 0; i < list.count; i++) {
        if (strcmp(list.interfaces[i].name, name) == 0) {
            *info = list.interfaces[i];
            network_hal_free_interfaces(&list);
            return 0;
        }
    }
    
    network_hal_free_interfaces(&list);
    return -1;
}

/**
 * @brief Настройка интерфейса
 */
static int network_hal_configure_interface(const char *name, 
                                            const vgik_network_ip_config_t *config) {
    if (!name || !config) {
        return -1;
    }
    
    // TODO: Реализовать настройку IP через netlink или ioctl
    // Это сложная операция, требующая root прав
    
    return 0;
}

/**
 * @brief Включение/выключение интерфейса
 */
static int network_hal_set_interface_state(const char *name, 
                                            vgik_network_if_state_t state) {
    if (!name) {
        return -1;
    }
    
    struct ifreq ifr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (sock < 0) {
        return -1;
    }
    
    strncpy(ifr.ifr_name, name, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    
    // Получение текущих флагов
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
        close(sock);
        return -1;
    }
    
    // Установка флага UP
    if (state == VGIK_NETWORK_IF_STATE_UP) {
        ifr.ifr_flags |= IFF_UP;
    } else {
        ifr.ifr_flags &= ~IFF_UP;
    }
    
    // Применение флагов
    if (ioctl(sock, SIOCSIFFLAGS, &ifr) < 0) {
        close(sock);
        return -1;
    }
    
    close(sock);
    return 0;
}

/**
 * @brief Получение статистики интерфейса
 */
static int network_hal_get_interface_stats(const char *name, 
                                            vgik_network_if_stats_t *stats) {
    if (!name || !stats) {
        return -1;
    }
    
    FILE *f = fopen("/proc/net/dev", "r");
    if (!f) {
        return -1;
    }
    
    char line[256];
    // Пропуск заголовков
    fgets(line, sizeof(line), f);
    fgets(line, sizeof(line), f);
    
    memset(stats, 0, sizeof(*stats));
    
    while (fgets(line, sizeof(line), f)) {
        char if_name[32];
        unsigned long long rx_bytes, tx_bytes, rx_packets, tx_packets;
        unsigned long long rx_errors, tx_errors, rx_dropped, tx_dropped;
        
        // Парсинг строки /proc/net/dev
        if (sscanf(line, "%31[^:]: %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                   if_name, &rx_bytes, &rx_packets, &rx_errors, &rx_dropped,
                   NULL, NULL, NULL, NULL,
                   &tx_bytes, &tx_packets, &tx_errors, &tx_dropped,
                   NULL, NULL, NULL, NULL) >= 9) {
            
            // Удаление пробелов из имени интерфейса
            char *p = if_name;
            while (*p == ' ') p++;
            
            if (strcmp(p, name) == 0) {
                stats->rx_bytes = rx_bytes;
                stats->tx_bytes = tx_bytes;
                stats->rx_packets = rx_packets;
                stats->tx_packets = tx_packets;
                stats->rx_errors = rx_errors;
                stats->tx_errors = tx_errors;
                stats->rx_dropped = rx_dropped;
                stats->tx_dropped = tx_dropped;
                
                fclose(f);
                return 0;
            }
        }
    }
    
    fclose(f);
    return -1;
}

/**
 * @brief Установка MTU
 */
static int network_hal_set_mtu(const char *name, uint32_t mtu) {
    if (!name) {
        return -1;
    }
    
    struct ifreq ifr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (sock < 0) {
        return -1;
    }
    
    strncpy(ifr.ifr_name, name, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    ifr.ifr_mtu = mtu;
    
    if (ioctl(sock, SIOCSIFMTU, &ifr) < 0) {
        close(sock);
        return -1;
    }
    
    close(sock);
    return 0;
}

/**
 * @brief Экспорт HAL структуры
 */
static const vgik_network_hal_t g_network_hal = {
    .init = network_hal_init,
    .deinit = network_hal_deinit,
    .get_interfaces = network_hal_get_interfaces,
    .free_interfaces = network_hal_free_interfaces,
    .get_interface_info = network_hal_get_interface_info,
    .configure_interface = network_hal_configure_interface,
    .set_interface_state = network_hal_set_interface_state,
    .get_interface_stats = network_hal_get_interface_stats,
    .set_mtu = network_hal_set_mtu
};

const vgik_network_hal_t* vgik_network_hal_get(void) {
    return &g_network_hal;
}

