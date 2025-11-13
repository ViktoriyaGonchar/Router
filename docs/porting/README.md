# Руководство по портированию VGIK на новую платформу

Это руководство предназначено для производителей оборудования, которые хотят портировать VGIK на свои устройства.

## Обзор процесса

Портирование VGIK на новую платформу включает следующие шаги:

1. Анализ аппаратной платформы
2. Создание HAL-реализации
3. Настройка системы сборки
4. Тестирование
5. Документация

## Шаг 1: Анализ аппаратной платформы

### Необходимая информация

- **Чипсет**: Производитель и модель
- **Архитектура**: MIPS, ARM, x86
- **CPU**: Количество ядер, частота
- **Память**: Объём RAM и Flash
- **Сеть**: Количество Ethernet портов, поддержка WiFi
- **GPIO/LED**: Количество и назначение
- **Существующая прошивка**: OpenWrt, vendor firmware, etc.

### Инструменты

- Схемы устройства
- Документация чипсета
- Существующие драйверы (если есть)

## Шаг 2: Создание HAL-реализации

### Структура HAL-модуля

```
hal/
├── hal_network_<platform>.c
├── hal_gpio_<platform>.c
├── hal_memory_<platform>.c
└── hal_system_<platform>.c
```

### Пример: Сетевой HAL

```c
#include "hal_network.h"

static int platform_network_init(void) {
    // Инициализация сетевых интерфейсов
    return 0;
}

static int platform_get_interfaces(interface_list_t *list) {
    // Получение списка интерфейсов
    return 0;
}

network_hal_t platform_network_hal = {
    .init = platform_network_init,
    .get_interfaces = platform_get_interfaces,
    // ... другие функции
};
```

## Шаг 3: Настройка системы сборки

### Создание device profile

Создайте файл `build/profiles/<device-name>.mk`:

```makefile
DEVICE_NAME = example-router-v1
DEVICE_ARCH = arm64
DEVICE_CHIPSET = mediatek-mt7622

# Toolchain
TOOLCHAIN_PATH = /opt/toolchains/arm64-gcc

# Kernel config
KERNEL_CONFIG = configs/kernel-mediatek.config

# Device tree
DEVICE_TREE = dts/mediatek-mt7622-example.dts
```

## Шаг 4: Тестирование

### Модульные тесты HAL

```c
void test_network_hal(void) {
    // Тесты сетевого HAL
}
```

### Интеграционные тесты

- Тестирование всех сетевых интерфейсов
- Тестирование GPIO/LED
- Тестирование работы с памятью

## Шаг 5: Документация

Создайте файл `docs/porting/<device-name>.md` с описанием:
- Характеристики устройства
- Особенности портирования
- Известные проблемы
- Инструкции по сборке

## Примеры

См. примеры портирования в `docs/porting/examples/`.

## Поддержка

При возникновении вопросов обращайтесь к команде разработки VGIK.

