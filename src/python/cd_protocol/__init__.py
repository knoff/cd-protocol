import struct
from dataclasses import dataclass
from enum import IntEnum

# --- CONSTANTS (Must match headunit_protocol.h) ---
HU_PROTOCOL_MAGIC = 0xA5
HU_MAX_PAYLOAD_SIZE = 230


# --- ENUMS ---
class DeviceID(IntEnum):
    BROADCAST = 0xFF
    COORDINATOR = 0x01
    BOILER = 0x10
    PUMP = 0x11
    GROUP_HEAD = 0x12
    SCALES = 0x13
    GRINDER = 0x14


class MsgType(IntEnum):
    PING = 0x01
    ACK = 0x02
    ERROR = 0x03
    CMD_SET_STATE = 0x10
    CMD_PROFILE = 0x11
    CMD_START = 0x12
    DATA_SENSOR = 0x20
    EVENT_CRITICAL = 0x21


# --- PACKING FORMAT ---
# < = Little Endian
# B = uint8 (Magic)
# B = uint8 (Flags)
# B = uint8 (Src)
# B = uint8 (Dst)
# B = uint8 (Via)
# B = uint8 (Type)
# H = uint16 (Seq)
# B = uint8 (Len)
# Total: 9 bytes header
HEADER_FORMAT = "<BBBBBBHB"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)


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
        """Serializes the frame to binary bytes for transmission."""
        payload_len = len(self.payload)
        if payload_len > HU_MAX_PAYLOAD_SIZE:
            raise ValueError(
                f"Payload too large: {payload_len} > {HU_MAX_PAYLOAD_SIZE}"
            )

        header = struct.pack(
            HEADER_FORMAT,
            HU_PROTOCOL_MAGIC,
            self.flags,
            self.src_id,
            self.dst_id,
            self.via_id,
            self.msg_type,
            self.seq_num,
            payload_len,
        )
        return header + self.payload

    @classmethod
    def unpack(cls, data: bytes):
        """
        Parses bytes and returns (MeshFrame, remaining_bytes).
        Returns None if data is insufficient for a full frame.
        """
        if len(data) < HEADER_SIZE:
            return None, data

        # Unpack header
        magic, flags, src, dst, via, msg_type, seq, p_len = struct.unpack(
            HEADER_FORMAT, data[:HEADER_SIZE]
        )

        if magic != HU_PROTOCOL_MAGIC:
            # Sync error: find next magic byte or discard
            # For simplicity here: discard first byte and try again next time
            return None, data[1:]

        total_len = HEADER_SIZE + p_len
        if len(data) < total_len:
            # Not enough data for payload yet
            return None, data

        payload = data[HEADER_SIZE:total_len]
        remaining = data[total_len:]

        frame = cls(
            src_id=src,
            dst_id=dst,
            msg_type=msg_type,
            payload=payload,
            via_id=via,
            seq_num=seq,
            flags=flags,
        )
        return frame, remaining
