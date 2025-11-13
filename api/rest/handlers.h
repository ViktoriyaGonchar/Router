/**
 * @file handlers.h
 * @brief Обработчики REST API эндпоинтов
 */

#ifndef VGIK_REST_HANDLERS_H
#define VGIK_REST_HANDLERS_H

#include "rest_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Регистрация всех обработчиков
 * 
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int vgik_rest_handlers_register_all(void);

#ifdef __cplusplus
}
#endif

#endif /* VGIK_REST_HANDLERS_H */

