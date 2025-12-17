/**
 * @file headunit_protocol.h
 * @brief Бинарный протокол внутренней шины Coffee Digital HeadUnit (ESP-NOW)
 * @version 0.1.0
 */

#pragma once

#include <stdint.h>

// Магическое число для синхронизации и фильтрации чужих пакетов
#define HU_PROTOCOL_MAGIC 0xA5

// Максимальный размер полезной нагрузки (ESP-NOW limit 250 - header)
#define HU_MAX_PAYLOAD_SIZE 230

// --- DEVICE TYPES (LOGICAL IDs) ---
// Используются для маршрутизации вместо MAC-адресов
#define HU_DEV_BROADCAST 0xFF
#define HU_DEV_COORDINATOR 0x01
#define HU_DEV_BOILER 0x10
#define HU_DEV_PUMP 0x11
#define HU_DEV_GROUP_HEAD 0x12
#define HU_DEV_SCALES 0x13
#define HU_DEV_GRINDER 0x14

// --- MESSAGE TYPES ---
typedef enum
{
  HU_MSG_PING = 0x01,  // Проверка связи / Discovery
  HU_MSG_ACK = 0x02,   // Подтверждение уровня приложения
  HU_MSG_ERROR = 0x03, // Ошибка исполнения

  // Команды управления (RPi -> Node)
  HU_MSG_CMD_SET_STATE = 0x10, // Включить/Выключить
  HU_MSG_CMD_PROFILE = 0x11,   // Загрузить чанк профиля
  HU_MSG_CMD_START = 0x12,     // Синхронный старт (Trigger)

  // Телеметрия (Node -> RPi/Broadcast)
  HU_MSG_DATA_SENSOR = 0x20,    // Регулярные данные датчиков
  HU_MSG_EVENT_CRITICAL = 0x21, // Авария (Dry run, Overheat)
} hu_msg_type_t;

// --- FLAGS ---
#define HU_FLAG_NEED_ACK (1 << 0)      // Требуется подтверждение приложения
#define HU_FLAG_RETRANSMITTED (1 << 1) // Пакет был переслан репитером

#pragma pack(push, 1)

/**
 * @brief Заголовок протокола (Transport Layer)
 * Общий размер: 8 байт + Payload + 1 байт CRC
 */
typedef struct
{
  uint8_t magic;       // 0xA5
  uint8_t flags;       // Битовая маска
  uint8_t src_id;      // Кто отправил (Logical ID)
  uint8_t dst_id;      // Кому предназначено (Logical ID)
  uint8_t via_id;      // Через кого слать (0 = Direct, иначе ID репитера)
  uint8_t msg_type;    // hu_msg_type_t
  uint16_t seq_num;    // Счетчик для дедупликации
  uint8_t payload_len; // Длина данных
                       // uint8_t payload[];   // Данные (variable length)
                       // uint8_t crc8;        // CRC8 всего пакета (header + payload)
} hu_frame_header_t;

// --- PAYLOAD STRUCTURES ---

// Payload для HU_MSG_CMD_START
typedef struct
{
  uint8_t profile_id;
  uint32_t start_delay_ms; // Через сколько мс начать (от момента приема)
  uint32_t volume_ml;      // Целевой объем (если применимо)
} hu_payload_cmd_start_t;

// Payload для HU_MSG_DATA_SENSOR (Универсальный)
typedef struct
{
  uint32_t timestamp_ms; // Локальное время узла
  float value_primary;   // Температура / Давление
  float value_secondary; // Мощность / Поток
  uint8_t state_flags;   // Битовая маска состояния узла
} hu_payload_sensor_t;

#pragma pack(pop)
