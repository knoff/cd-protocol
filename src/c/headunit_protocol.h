/**
 * @file headunit_protocol.h
 * @brief Binary Protocol for Coffee Digital HeadUnit (ESP-NOW)
 * @version 0.2.0 (Architecture Release)
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

// === 1. TRANSPORT LAYER CONSTANTS ===
#define HU_PROTOCOL_MAGIC 0xA5
#define HU_PROTOCOL_VERSION 0x02
#define HU_MAX_PAYLOAD_SIZE 230 // ESP-NOW limit minus headers

// === 2. DEVICE ADDRESSING (Logical IDs) ===
typedef enum
{
  HU_DEV_BROADCAST = 0xFF,
  HU_DEV_COORDINATOR = 0x01, // USB Dongle / RPi Gateway

  // Actuators & Sensors
  HU_DEV_BOILER_MAIN = 0x10,
  HU_DEV_BOILER_STEAM = 0x11,
  HU_DEV_GROUP_HEAD = 0x12,
  HU_DEV_PUMP_MAIN = 0x13,

  // HMI & Peripherals
  HU_DEV_SCALES = 0x20,
  HU_DEV_HAPTIC_KNOB_L = 0x30, // Левая группа
  HU_DEV_HAPTIC_KNOB_R = 0x31, // Правая группа
  HU_DEV_STEAM_LEVER = 0x32,
  HU_DEV_BUTTON_PAD = 0x33
} hu_device_id_t;

// === 3. MESSAGE TYPES ===
typedef enum
{
  // --- System Level ---
  HU_MSG_PING = 0x01,
  HU_MSG_ACK = 0x02,
  HU_MSG_ERROR = 0x03,
  HU_MSG_DISCOVERY = 0x04, // "Я тут, мой ID такой-то"

  // --- Control (RPi -> Node) ---
  HU_MSG_CMD_SET_STATE = 0x10,    // Simple ON/OFF (Valve, Relay)
  HU_MSG_CMD_PROFILE_STEP = 0x11, // Vector update (Time, T, P, F)
  HU_MSG_CMD_HAPTIC_CFG = 0x12,   // Config motor physics
  HU_MSG_CMD_UI_WIDGET = 0x13,    // Draw single widget
  HU_MSG_CMD_UI_MENU = 0x14,      // Update list/menu items

  // --- Events (Node -> RPi / Broadcast) ---
  HU_MSG_EVENT_UI_INPUT = 0x20,   // Button click, Knob turn
  HU_MSG_EVENT_CRITICAL = 0x21,   // ERROR! Stop everything
  HU_MSG_EVENT_FLOW_START = 0x22, // Scale: First drop detected

  // --- Telemetry (Node -> RPi) ---
  HU_MSG_DATA_SENSOR = 0x30, // Single float value
  HU_MSG_DATA_MULTI = 0x31,  // Compact array
  HU_MSG_DATA_SCALE = 0x32   // Weight + Flow Rate
} hu_msg_type_t;

// === 4. ENUMS & FLAGS ===

// Priority modes for Profile (Conflict Resolution)
typedef enum
{
  HU_PRIORITY_FLOW = 0,     // Keep flow, ignore pressure
  HU_PRIORITY_PRESSURE = 1, // Keep pressure, sacrifice flow
  HU_PRIORITY_HYBRID = 2    // Energy / Experimental
} hu_profile_priority_t;

// Haptic Modes
typedef enum
{
  KNOB_MODE_FREE = 0,    // Подшипник
  KNOB_MODE_DETENTS = 1, // Щелчки (Menu)
  KNOB_MODE_SPRING = 2,  // Пружина (Manual Shot)
  KNOB_MODE_BARRIER = 3, // Упоры (Min/Max)
  KNOB_MODE_SERVO = 4    // Force movement
} hu_haptic_mode_t;

// UI Input Types
typedef enum
{
  INPUT_CLICK_SHORT = 0,
  INPUT_CLICK_LONG = 1,
  INPUT_HOLD_START = 2,
  INPUT_HOLD_END = 3,
  INPUT_ROTATE = 4, // +Value / -Value
  INPUT_TOUCH = 5
} hu_input_event_t;

#pragma pack(push, 1)

// === 5. FRAME STRUCTURES ===

// Transport Header (9 bytes)
typedef struct
{
  uint8_t magic;    // 0xA5
  uint8_t flags;    // 0x01=NEED_ACK, 0x02=RETRANSMITTED
  uint8_t src_id;   // hu_device_id_t
  uint8_t dst_id;   // hu_device_id_t
  uint8_t via_id;   // 0 = Direct
  uint8_t msg_type; // hu_msg_type_t
  uint16_t seq_num; // Deduplication
  uint8_t payload_len;
} hu_frame_header_t;

// --- PAYLOADS ---

// 1. Vector Profile Step (JIT Execution)
typedef struct
{
  uint16_t duration_ms;     // How long to hold this state
  int16_t target_temp_c;    // x100 (9350 = 93.5 C)
  int16_t target_flow_ml;   // x100 (ml/s)
  int16_t target_press_bar; // x100 (900 = 9.0 bar)
  uint8_t priority;         // hu_profile_priority_t
  uint8_t flags;            // Bitmask (e.g., "Interpolate to next")
} hu_payload_profile_step_t;

// 2. Haptic Config
typedef struct
{
  uint8_t mode;     // hu_haptic_mode_t
  uint8_t strength; // 0-100% (Force/Current)
  int16_t param_1;  // Steps count / Spring Center / Min Angle
  int16_t param_2;  // Snap strength / Stiffness / Max Angle
} hu_payload_haptic_cfg_t;

// 3. UI Menu Item (Single entry)
// Used inside an array. Max 5-6 items per packet.
typedef struct
{
  uint8_t item_id; // ID to send back on click
  uint8_t icon_id; // 0=None, 1=Settings, 2=Coffee...
  uint8_t flags;   // 1=Selected, 2=Disabled, 4=IsBack, 8=IsNext
  char text[24];   // UTF-8 string (approx 12 Cyrillic chars)
} hu_menu_item_t;

// 3b. UI Menu Packet
typedef struct
{
  uint8_t list_id;     // Context ID
  uint8_t total_items; // Total in list (for scrollbar calc)
  uint8_t start_index; // Index of the first item in this packet
  uint8_t items_count; // How many items follow (max 5)
                       // hu_menu_item_t items[]; // Variable length
} hu_payload_ui_menu_t;

// 4. Scales Telemetry
typedef struct
{
  uint32_t timestamp_ms;
  int32_t weight_mg; // Current weight (mg)
  int16_t flow_mg_s; // Output Flow (derivative)
  uint8_t status;    // 1=Stable, 2=TareDone
} hu_payload_scale_data_t;

// 5. Input Event (Knob/Button)
typedef struct
{
  uint8_t source_index; // Which button/encoder on the board (0, 1...)
  uint8_t event_type;   // hu_input_event_t
  int32_t value;        // Duration (ms) or Encoder Delta (+1/-1) or Absolute Pos
} hu_payload_event_input_t;

#pragma pack(pop)
