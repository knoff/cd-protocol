import struct
from dataclasses import dataclass
from enum import IntEnum
from typing import Optional, Tuple

# === 1. CONSTANTS ===
HU_PROTOCOL_MAGIC = 0xA5
HU_PROTOCOL_VERSION = 0x02
HU_MAX_PAYLOAD_SIZE = 230
HEADER_SIZE = 9  # struct.calcsize("<BBBBBBHB")


# === 2. DEVICE ADDRESSING ===
class DeviceID(IntEnum):
    BROADCAST = 0xFF
    COORDINATOR = 0x01

    # Actuators & Sensors
    BOILER_MAIN = 0x10
    BOILER_STEAM = 0x11
    GROUP_HEAD = 0x12
    PUMP_MAIN = 0x13

    # HMI & Peripherals
    SCALES = 0x20
    HAPTIC_KNOB_L = 0x30
    HAPTIC_KNOB_R = 0x31
    STEAM_LEVER = 0x32
    BUTTON_PAD = 0x33


# === 3. MESSAGE TYPES ===
class MsgType(IntEnum):
    # System
    PING = 0x01
    ACK = 0x02
    ERROR = 0x03
    DISCOVERY = 0x04

    # Control (RPi -> Node)
    CMD_SET_STATE = 0x10
    CMD_PROFILE_STEP = 0x11
    CMD_HAPTIC_CFG = 0x12
    CMD_UI_WIDGET = 0x13
    CMD_UI_MENU = 0x14

    # Events (Node -> RPi)
    EVENT_UI_INPUT = 0x20
    EVENT_CRITICAL = 0x21
    EVENT_FLOW_START = 0x22

    # Telemetry
    DATA_SENSOR = 0x30
    DATA_MULTI = 0x31
    DATA_SCALE = 0x32


# === 4. ENUMS ===
class Priority(IntEnum):
    FLOW = 0
    PRESSURE = 1
    HYBRID = 2


class HapticMode(IntEnum):
    FREE = 0
    DETENTS = 1
    SPRING = 2
    BARRIER = 3
    SERVO = 4


class InputEvent(IntEnum):
    CLICK_SHORT = 0
    CLICK_LONG = 1
    HOLD_START = 2
    HOLD_END = 3
    ROTATE = 4
    TOUCH = 5


# === 5. PAYLOAD STRUCTURES ===


@dataclass
class PayloadProfileStep:
    duration_ms: int
    target_temp_c: int  # x100
    target_flow_ml: int  # x100
    target_press_bar: int  # x100
    priority: int  # Priority Enum
    flags: int = 0

    def pack(self) -> bytes:
        # H = uint16, h = int16, B = uint8
        return struct.pack(
            "<HhhhBB",
            self.duration_ms,
            self.target_temp_c,
            self.target_flow_ml,
            self.target_press_bar,
            self.priority,
            self.flags,
        )


@dataclass
class PayloadHapticConfig:
    mode: int  # HapticMode Enum
    strength: int  # 0-100
    param_1: int  # int16
    param_2: int  # int16

    def pack(self) -> bytes:
        return struct.pack(
            "<BBhh", self.mode, self.strength, self.param_1, self.param_2
        )


@dataclass
class PayloadScaleData:
    timestamp_ms: int  # uint32
    weight_mg: int  # int32
    flow_mg_s: int  # int16
    status: int  # uint8

    @classmethod
    def unpack(cls, data: bytes):
        # I = uint32, i = int32, h = int16, B = uint8
        if len(data) < 11:
            return None
        vals = struct.unpack("<IihB", data[:11])
        return cls(*vals)


@dataclass
class PayloadInputEvent:
    source_index: int
    event_type: int
    value: int  # int32

    @classmethod
    def unpack(cls, data: bytes):
        if len(data) < 6:
            return None
        # B = uint8, i = int32
        vals = struct.unpack("<BBi", data[:6])
        return cls(*vals)


# === 6. MAIN FRAME ===

_HEADER_STRUCT = struct.Struct("<BBBBBBHB")


@dataclass
class MeshFrame:
    src_id: int
    dst_id: int
    msg_type: int
    payload: bytes = b""
    via_id: int = 0
    seq_num: int = 0
    flags: int = 0

    def pack(self) -> bytes:
        if len(self.payload) > HU_MAX_PAYLOAD_SIZE:
            raise ValueError(f"Payload too large: {len(self.payload)}")

        header = _HEADER_STRUCT.pack(
            HU_PROTOCOL_MAGIC,
            self.flags,
            self.src_id,
            self.dst_id,
            self.via_id,
            self.msg_type,
            self.seq_num,
            len(self.payload),
        )
        return header + self.payload

    @classmethod
    def unpack(cls, data: bytes) -> Tuple[Optional["MeshFrame"], bytes]:
        if len(data) < HEADER_SIZE:
            return None, data

        magic, flags, src, dst, via, msg_type, seq, p_len = _HEADER_STRUCT.unpack(
            data[:HEADER_SIZE]
        )

        if magic != HU_PROTOCOL_MAGIC:
            return None, data[1:]  # Sync lost

        total_len = HEADER_SIZE + p_len
        if len(data) < total_len:
            return None, data  # Wait for more data

        frame = cls(
            src_id=src,
            dst_id=dst,
            msg_type=msg_type,
            payload=data[HEADER_SIZE:total_len],
            via_id=via,
            seq_num=seq,
            flags=flags,
        )
        return frame, data[total_len:]
