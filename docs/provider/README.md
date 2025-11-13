# Руководство для интернет-провайдеров

Это руководство предназначено для интернет-провайдеров, которые хотят интегрировать устройства с прошивкой VGIK в свою инфраструктуру.

## Быстрый старт

### 1. Подключение устройства

1. Подключите устройство к сети провайдера
2. Устройство автоматически получит IP-адрес через DHCP
3. Устройство подключится к ACS (Auto Configuration Server) через TR-069

### 2. Первичная настройка

Устройство можно настроить через:
- **TR-069**: Автоматическая настройка через ACS
- **REST API**: Программная настройка
- **Web-интерфейс**: Ручная настройка

## Интеграция через TR-069

### Настройка ACS

1. Настройте TR-069 сервер (ACS)
2. Укажите URL ACS в конфигурации устройства
3. Устройство автоматически подключится и получит конфигурацию

### Data Model

VGIK поддерживает стандартный TR-069 Data Model для роутеров:
- `InternetGatewayDevice`
- `DeviceInfo`
- `LANDevice`
- `WANDevice`
- `WANConnectionDevice`

### Пример конфигурации через TR-069

```xml
<SetParameterValues>
  <ParameterList>
    <ParameterValueStruct>
      <Name>InternetGatewayDevice.WANDevice.1.WANConnectionDevice.1.WANPPPConnection.1.Username</Name>
      <Value>user@provider.com</Value>
    </ParameterValueStruct>
    <ParameterValueStruct>
      <Name>InternetGatewayDevice.WANDevice.1.WANConnectionDevice.1.WANPPPConnection.1.Password</Name>
      <Value>password123</Value>
    </ParameterValueStruct>
  </ParameterList>
</SetParameterValues>
```

## Интеграция через REST API

### Аутентификация

```bash
# Получение токена
curl -X POST https://device-ip/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username": "admin", "password": "password"}'
```

### Настройка PPPoE

```bash
curl -X POST https://device-ip/api/v1/config \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{
    "network": {
      "pppoe": {
        "enabled": true,
        "interface": "eth1",
        "username": "user@provider.com",
        "password": "password123"
      }
    }
  }'
```

### Настройка VLAN

```bash
curl -X POST https://device-ip/api/v1/config \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{
    "network": {
      "vlan": [
        {
          "id": 100,
          "interfaces": ["eth0", "eth1"],
          "description": "Provider VLAN"
        }
      ]
    }
  }'
```

## Мониторинг устройств

### Получение статистики

```bash
curl -X GET https://device-ip/api/v1/statistics \
  -H "Authorization: Bearer <token>"
```

### Получение логов

```bash
curl -X GET https://device-ip/api/v1/logs \
  -H "Authorization: Bearer <token>"
```

### SNMP

Устройства поддерживают SNMP для мониторинга:
- SNMP v2c
- SNMP v3
- Стандартные MIB для роутеров

## Обновление прошивки

### Через REST API

```bash
curl -X POST https://device-ip/api/v1/firmware/update \
  -H "Authorization: Bearer <token>" \
  -F "firmware=@firmware.bin"
```

### Через TR-069

Используйте стандартные TR-069 методы для обновления прошивки.

## Интеграция с OSS/BSS системами

### REST API адаптер

VGIK предоставляет REST API для интеграции с системами провайдера:
- Управление устройствами
- Получение статистики
- Настройка конфигураций
- Мониторинг состояния

### Webhooks

Устройства могут отправлять события на сервер провайдера:
- Изменение статуса соединения
- Ошибки подключения
- Обновления прошивки

## Безопасность

### Рекомендации

1. Используйте HTTPS для всех коммуникаций
2. Настройте firewall правила
3. Регулярно обновляйте прошивки
4. Используйте сильные пароли
5. Включите логирование безопасности

### Сертификаты

Устройства поддерживают:
- Самоподписанные сертификаты
- Сертификаты от CA
- Сертификаты от провайдера

## Troubleshooting

### Проблемы с подключением

1. Проверьте физическое подключение
2. Проверьте настройки VLAN (если используются)
3. Проверьте логи устройства
4. Проверьте настройки DHCP

### Проблемы с TR-069

1. Проверьте доступность ACS сервера
2. Проверьте сертификаты
3. Проверьте логи TR-069 соединения

## Поддержка

Для получения поддержки обращайтесь к команде VGIK или производителю устройства.

