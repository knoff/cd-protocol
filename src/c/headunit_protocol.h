/**
 * @file headunit_protocol.h
 * @brief Binary Protocol for Coffee Digital HeadUnit (ESP-NOW)
 * @version 0.2.1 (Dynamic Addressing Support)
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

// === 1. TRANSPORT LAYER CONSTANTS ===
#define HU_PROTOCOL_MAGIC 0xA5
#define HU_PROTOCOL_VERSION 0x02
#define HU_MAX_PAYLOAD_SIZE 230

// === 2. ADDRESSING & TYPES ===

// Logical Addresses (assigned by RPi)
typedef enum
{
  HU_ADDR_COORDINATOR = 0x01, // RPi Host
  HU_ADDR_BROADCAST = 0xFF,   // To All
  HU_ADDR_UNASSIGNED = 0xFE,  // Default for new devices

  // Dynamic Range: 0x10 ... 0xFD
  HU_ADDR_MIN_DYNAMIC = 0x10,
  HU_ADDR_MAX_DYNAMIC = 0xFD
} hu_device_address_t;

// Device Types (Firmware Class)
typedef enum
{
  HU_TYPE_UNKNOWN = 0x00,
  HU_TYPE_COORDINATOR = 0x01,

  // Actuators
  HU_TYPE_BOILER_PID = 0x10,     // Heater + NTC
  HU_TYPE_PUMP_CTRL = 0x11,      // Motor + Flow Meter
  HU_TYPE_VALVE_SOLENOID = 0x12, // Simple ON/OFF
  HU_TYPE_VALVE_SERVO = 0x13,    // Variable position

  // Sensors & UI
  HU_TYPE_SCALES = 0x20,      // Load cell bridge
  HU_TYPE_HAPTIC_KNOB = 0x30, // Motor + Encoder + Screen
  HU_TYPE_BUTTON_PAD = 0x31,  // Simple buttons
  HU_TYPE_SENSOR_HUB = 0x32   // Multiple temp/pressure sensors
} hu_device_type_t;

// === 3. MESSAGE TYPES ===
typedef enum
{
  // --- System / Provisioning ---
  HU_MSG_PING = 0x01,
  HU_MSG_ACK = 0x02,
  HU_MSG_ERROR = 0x03,

  // Discovery & Config
  HU_MSG_SYS_DISCOVERY_REQ = 0x05, // RPi -> Broadcast: "Who is out there?"
  HU_MSG_SYS_DISCOVERY_RES = 0x06, // Node -> RPi: "I am Type X, MAC Y"
  HU_MSG_SYS_ASSIGN_ID = 0x07,     // RPi -> Node: "Your new Logical ID is Z"
  HU_MSG_SYS_REBOOT = 0x08,

  // --- Control (RPi -> Node) ---
  HU_MSG_CMD_SET_STATE = 0x10,
  HU_MSG_CMD_PROFILE_STEP = 0x11,
  HU_MSG_CMD_HAPTIC_CFG = 0x12,
  HU_MSG_CMD_UI_WIDGET = 0x13,
  HU_MSG_CMD_UI_MENU = 0x14,

  // --- Events (Node -> RPi) ---
  HU_MSG_EVENT_UI_INPUT = 0x20,
  HU_MSG_EVENT_CRITICAL = 0x21,
  HU_MSG_EVENT_FLOW_START = 0x22,

  // --- Telemetry (Node -> RPi) ---
  HU_MSG_DATA_SENSOR = 0x30,
  HU_MSG_DATA_MULTI = 0x31,
  HU_MSG_DATA_SCALE = 0x32
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
  uint8_t src_id;   // hu_device_address_t
  uint8_t dst_id;   // hu_device_address_t
  uint8_t via_id;   // 0 = Direct
  uint8_t msg_type; // hu_msg_type_t
  uint16_t seq_num; // Deduplication
  uint8_t payload_len;
} hu_frame_header_t;

// --- PAYLOADS ---

// Discovery Response (Node -> RPi)
typedef struct
{
  uint8_t device_type; // hu_device_type_t
  uint8_t hw_revision; // e.g. 1 (Rev A)
  uint8_t fw_major;
  uint8_t fw_minor;
  uint8_t current_id; // 0xFE if not configured
                      // MAC is in the L2 frame header
} hu_payload_discovery_res_t;

// Assign ID (RPi -> Node)
typedef struct
{
  uint8_t target_mac[6];  // Safety check
  uint8_t new_logical_id; // 0x10...0xFD
} hu_payload_assign_id_t;

// 1. Vector Profile Step (JIT Execution)
typedef struct
{
  uint16_t duration_ms;     // How long to hold this state
  int16_t target_temp_c;    // x100 (9350 = 93.5 C)
  int16_t target_flow_ml;   // x100 (ml/s)
  int16_t target_press_bar; // x100 (900 = 9.0 bar)
  uint8_t priority;         // hu_profile_priority_t
  uint8_t flags;            // Bitmask
} hu_payload_profile_step_t;

// 2. Haptic Config
typedef struct
{
  uint8_t mode;     // hu_haptic_mode_t
  uint8_t strength; // 0-100% (Force/Current)
  int16_t param_1;  // Steps count / Spring Center / Min Angle
  int16_t param_2;  // Snap strength / Stiffness / Max Angle
} hu_payload_haptic_cfg_t;

// 3. UI Menu Item
typedef struct
{
  uint8_t item_id; // ID to send back on click
  uint8_t icon_id; // 0=None
  uint8_t flags;   // 1=Selected, 2=Disabled
  char text[24];   // UTF-8 string
} hu_menu_item_t;

// 3b. UI Menu Packet
typedef struct
{
  uint8_t list_id;
  uint8_t total_items;
  uint8_t start_index;
  uint8_t items_count;
  // hu_menu_item_t items[];
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
  uint8_t source_index; // Which button/encoder
  uint8_t event_type;   // hu_input_event_t
  int32_t value;        // Duration or Delta
} hu_payload_event_input_t;

#pragma pack(pop)
