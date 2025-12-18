import struct
from dataclasses import dataclass
from enum import IntEnum
from typing import Optional, List, Tuple

# === 1. CONSTANTS ===
HU_PROTOCOL_MAGIC = 0xA5
HU_PROTOCOL_VERSION = 0x02
HU_MAX_PAYLOAD_SIZE = 230
HEADER_SIZE = 9  # struct.calcsize("<BBBBBBHB")


# === 2. ADDRESSING & TYPES ===
class DeviceAddress(IntEnum):
    COORDINATOR = 0x01
    BROADCAST = 0xFF
    UNASSIGNED = 0xFE


class DeviceType(IntEnum):
    UNKNOWN = 0x00
    COORDINATOR = 0x01
    BOILER_PID = 0x10
    PUMP_CTRL = 0x11
    VALVE_SOLENOID = 0x12
    VALVE_SERVO = 0x13
    SCALES = 0x20
    HAPTIC_KNOB = 0x30
    BUTTON_PAD = 0x31
    SENSOR_HUB = 0x32


# === 3. MSG TYPES ===
class MsgType(IntEnum):
    PING = 0x01
    ACK = 0x02
    ERROR = 0x03

    # Provisioning
    SYS_DISCOVERY_REQ = 0x05
    SYS_DISCOVERY_RES = 0x06
    SYS_ASSIGN_ID = 0x07
    SYS_REBOOT = 0x08

    # Control
    CMD_SET_STATE = 0x10
    CMD_PROFILE_LOAD = 0x11  # Updated
    CMD_HAPTIC_CFG = 0x12
    CMD_UI_WIDGET = 0x13
    CMD_UI_MENU = 0x14

    # Events
    EVENT_UI_INPUT = 0x20
    EVENT_CRITICAL = 0x21
    EVENT_FLOW_START = 0x22

    # Telemetry
    DATA_SENSOR = 0x30
    DATA_MULTI = 0x31
    DATA_SCALE = 0x32


# === 4. ENUMS ===
class Priority(IntEnum):
    FLOW_IN = 0
    PRESSURE = 1
    FLOW_OUT = 2
    ENERGY = 3


class Interpolation(IntEnum):
    LINEAR = 0
    SPLINE = 1
    STEP = 2


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
class PayloadDiscoveryRes:
    device_type: int
    hw_revision: int
    fw_major: int
    fw_minor: int
    current_id: int

    @classmethod
    def unpack(cls, data: bytes):
        if len(data) < 5:
            return None
        return cls(*struct.unpack("<BBBBB", data[:5]))


@dataclass
class PayloadAssignID:
    target_mac: bytes  # 6 bytes
    new_logical_id: int

    def pack(self) -> bytes:
        return struct.pack("<6sB", self.target_mac, self.new_logical_id)


@dataclass
class PayloadProfileNode:
    time_offset_ms: int
    priority: int  # Enum Priority
    interpolation: int  # Enum Interpolation

    # Values passed as Floats (Human Readable)
    # They will be scaled and packed as uint8
    temp_target: float  # C
    temp_tol: float

    press_target: float  # Bar
    press_tol: float

    flow_in_target: float  # ml/s (Pump)
    flow_in_tol: float

    flow_out_target: float  # ml/s (Scales)
    flow_out_tol: float

    energy_target: int  # 0-255
    energy_tol: int

    def pack(self) -> bytes:
        # 1. Config Flags: Bits 0-1 (Interp), Bits 2-3 (Prio)
        config = (self.interpolation & 0x03) | ((self.priority & 0x03) << 2)

        # 2. Scaling helper
        def _s(val, factor):
            i = int(round(val / factor))
            return max(0, min(255, i))

        # 3. Pack: H (time) B (flags) 10B (targets/tols)
        return struct.pack(
            "<HB BBBBBBBBBB",
            self.time_offset_ms,
            config,
            _s(self.temp_target, 0.5),
            _s(self.temp_tol, 0.5),
            _s(self.press_target, 0.1),
            _s(self.press_tol, 0.1),
            _s(self.flow_in_target, 0.1),
            _s(self.flow_in_tol, 0.1),
            _s(self.flow_out_target, 0.1),
            _s(self.flow_out_tol, 0.1),
            _s(self.energy_target, 1),
            _s(self.energy_tol, 1),
        )


@dataclass
class PayloadProfileLoad:
    profile_id: int
    nodes: List[PayloadProfileNode]

    def pack(self) -> bytes:
        if len(self.nodes) > 17:
            raise ValueError(f"Too many nodes: {len(self.nodes)} > 17")

        payload = struct.pack("<BB", self.profile_id, len(self.nodes))
        for node in self.nodes:
            payload += node.pack()
        return payload


@dataclass
class PayloadHapticConfig:
    mode: int
    strength: int
    param_1: int
    param_2: int

    def pack(self) -> bytes:
        return struct.pack(
            "<BBhh", self.mode, self.strength, self.param_1, self.param_2
        )


@dataclass
class PayloadScaleData:
    timestamp_ms: int
    weight_mg: int
    flow_mg_s: int
    status: int

    @classmethod
    def unpack(cls, data: bytes):
        if len(data) < 11:
            return None
        return cls(*struct.unpack("<IihB", data[:11]))


@dataclass
class PayloadInputEvent:
    source_index: int
    event_type: int
    value: int

    @classmethod
    def unpack(cls, data: bytes):
        if len(data) < 6:
            return None
        return cls(*struct.unpack("<BBi", data[:6]))


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
            return None, data[1:]

        total_len = HEADER_SIZE + p_len
        if len(data) < total_len:
            return None, data

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
