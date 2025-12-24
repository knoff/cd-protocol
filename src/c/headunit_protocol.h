/**
 * @file headunit_protocol.h
 * @brief Binary Protocol for Coffee Digital HeadUnit (ESP-NOW)
 * @version 0.2.3 (Compact Profile Nodes)
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
  HU_TYPE_PUMP_CTRL = 0x11,      // Motor + Flow meter
  HU_TYPE_VALVE_SOLENOID = 0x12, // Simple ON/OFF
  HU_TYPE_VALVE_SERVO = 0x13,    // Variable position

  // Sensors & UI
  HU_TYPE_SCALES = 0x20,      // Load cell bridge
  HU_TYPE_HAPTIC_KNOB = 0x30, // Motor + Encoder + Screen
  HU_TYPE_BUTTON_PAD = 0x31,  // Simple buttons
  HU_TYPE_SENSOR_HUB = 0x32,  // Multiple temp/pressure sensors

  HU_TYPE_TEST_DEVICE = 0xA0
} hu_device_type_t;

// === 3. MESSAGE TYPES ===
typedef enum
{
  // --- System / Provisioning ---
  HU_MSG_PING = 0x01,
  HU_MSG_ACK = 0x02,
  HU_MSG_ERROR = 0x03,

  // Discovery & Config
  HU_MSG_SYS_DISCOVERY_REQ = 0x05, // RPi -> Broadcast
  HU_MSG_SYS_DISCOVERY_RES = 0x06, // Node -> RPi
  HU_MSG_SYS_ASSIGN_ID = 0x07,     // RPi -> Node
  HU_MSG_SYS_REBOOT = 0x08,

  // --- Control (RPi -> Node) ---
  HU_MSG_CMD_SET_STATE = 0x10,
  HU_MSG_CMD_PROFILE_LOAD = 0x11, // Load full profile chunk
  HU_MSG_CMD_HAPTIC_CFG = 0x12,
  HU_MSG_CMD_UI_WIDGET = 0x13,
  HU_MSG_CMD_UI_MENU = 0x14,

  // --- Events (Node -> RPi) ---
  HU_MSG_EVENT_UI_INPUT = 0x20,
  HU_MSG_EVENT_CRITICAL = 0x21,
  HU_MSG_EVENT_FLOW_START = 0x22, // First drop

  // --- Telemetry (Node -> RPi) ---
  HU_MSG_DATA_SENSOR = 0x30,
  HU_MSG_DATA_MULTI = 0x31,
  HU_MSG_DATA_SCALE = 0x32
} hu_msg_type_t;

// === 4. ENUMS & FLAGS ===

typedef enum
{
  HU_PRIORITY_FLOW_IN = 0,  // Pump priority
  HU_PRIORITY_PRESSURE = 1, // Pressure priority
  HU_PRIORITY_FLOW_OUT = 2, // Scales priority (Gravimetric)
  HU_PRIORITY_ENERGY = 3    // Energy priority
} hu_profile_priority_t;

typedef enum
{
  HU_INTERPOLATION_LINEAR = 0,
  HU_INTERPOLATION_SPLINE = 1,
  HU_INTERPOLATION_STEP = 2
} hu_interpolation_t;

typedef enum
{
  KNOB_MODE_FREE = 0,
  KNOB_MODE_DETENTS = 1,
  KNOB_MODE_SPRING = 2,
  KNOB_MODE_BARRIER = 3,
  KNOB_MODE_SERVO = 4
} hu_haptic_mode_t;

typedef enum
{
  INPUT_CLICK_SHORT = 0,
  INPUT_CLICK_LONG = 1,
  INPUT_HOLD_START = 2,
  INPUT_HOLD_END = 3,
  INPUT_ROTATE = 4,
  INPUT_TOUCH = 5
} hu_input_event_t;

#pragma pack(push, 1)

// === 5. FRAME STRUCTURES ===

// Transport Header (9 bytes)
typedef struct
{
  uint8_t magic;    // 0xA5
  uint8_t flags;    // 0x01=NEED_ACK
  uint8_t src_id;   // hu_device_address_t
  uint8_t dst_id;   // hu_device_address_t
  uint8_t via_id;   // 0 = Direct
  uint8_t msg_type; // hu_msg_type_t
  uint16_t seq_num; // Deduplication
  uint8_t payload_len;
} hu_frame_header_t;

// --- PAYLOADS ---

// Discovery Response
typedef struct
{
  uint8_t device_type; // hu_device_type_t
  uint8_t hw_revision;
  uint8_t fw_major;
  uint8_t fw_minor;
  uint8_t current_id; // 0xFE if unassigned
} hu_payload_discovery_res_t;

// Assign ID
typedef struct
{
  uint8_t target_mac[6];
  uint8_t new_logical_id;
} hu_payload_assign_id_t;

// COMPACT PROFILE NODE (13 bytes)
// Scaling:
// Temp:   1 LSB = 0.5 C   (0 .. 127.5 C)
// Press:  1 LSB = 0.1 Bar (0 .. 25.5 Bar)
// Flow:   1 LSB = 0.1 ml/s(0 .. 25.5 ml/s)
// Energy: 1 LSB = 1 Unit  (0 .. 255)
typedef struct
{
  uint16_t time_offset_ms; // 0..65535 ms

  // Bits 0-1: hu_interpolation_t
  // Bits 2-3: hu_profile_priority_t
  // Bits 4-7: Reserved
  uint8_t config_flags;

  uint8_t temp_target; // Val * 0.5
  uint8_t temp_tol;    // Val * 0.5

  uint8_t press_target; // Val * 0.1
  uint8_t press_tol;    // Val * 0.1

  uint8_t flow_in_target; // Val * 0.1 (Pump)
  uint8_t flow_in_tol;    // Val * 0.1

  uint8_t flow_out_target; // Val * 0.1 (Scales)
  uint8_t flow_out_tol;    // Val * 0.1

  uint8_t energy_target; // Raw Index
  uint8_t energy_tol;    // Raw Index
} hu_profile_node_t;

// Profile Load Packet (Max ~17 nodes)
typedef struct
{
  uint8_t profile_id;
  uint8_t total_nodes;
  // hu_profile_node_t nodes[];
} hu_payload_profile_load_t;

// Haptic Config
typedef struct
{
  uint8_t mode;     // hu_haptic_mode_t
  uint8_t strength; // 0-100%
  int16_t param_1;  // Steps / Center / Min
  int16_t param_2;  // Snap / Stiffness / Max
} hu_payload_haptic_cfg_t;

// Scales Telemetry
typedef struct
{
  uint32_t timestamp_ms;
  int32_t weight_mg;
  int16_t flow_mg_s;
  uint8_t status;
} hu_payload_scale_data_t;

// Input Event
typedef struct
{
  uint8_t source_index;
  uint8_t event_type; // hu_input_event_t
  int32_t value;
} hu_payload_event_input_t;

#pragma pack(pop)
