# STM32 <-> PC Protocol v1

## 1) Frame format (binary, little-endian)

| Offset | Size | Field | Description |
|---|---:|---|---|
| 0 | 1 | `SOF1` | `0xAA` |
| 1 | 1 | `SOF2` | `0x55` |
| 2 | 1 | `version` | Protocol version (`0x01`) |
| 3 | 1 | `msg_type` | Message type |
| 4 | 1 | `flags` | Reserved for future use (set `0`) |
| 5 | 1 | `seq` | Sequence number (`0..255`) |
| 6 | 2 | `payload_len` | Payload length (`0..64`) |
| 8 | N | `payload` | Message payload |
| 8 + N | 2 | `crc16` | CRC16-CCITT over bytes `[version..payload_end]` |

CRC parameters:
- Polynomial: `0x1021`
- Init value: `0xFFFF`
- RefIn: `false`
- RefOut: `false`
- XorOut: `0x0000`

## 2) Message types

- `0x01` `TELEMETRY`
- `0x10` `CMD_SET_SPEED`
- `0x11` `CMD_STOP`
- `0x12` `CMD_PING`
- `0x13` `PONG`

## 3) Payload definitions

### `TELEMETRY` (`msg_type=0x01`, payload 15 bytes)

| Offset | Size | Type | Field |
|---|---:|---|---|
| 0 | 4 | `uint32` | `timestamp_ms` |
| 4 | 2 | `uint16` | `emg` |
| 6 | 2 | `uint16` | `fsr2` |
| 8 | 2 | `uint16` | `fsr1` |
| 10 | 1 | `uint8` | `hall_bits` (`bit0=H1`, `bit1=H2`, `bit2=H3`) |
| 11 | 1 | `int8` | `motor_speed_pct` (`-100..100`) |
| 12 | 1 | `uint8` | `motor_duty_pct` (`0..100`) |
| 13 | 1 | `uint8` | `motor_direction` (`0=STOP,1=FWD,2=REV`) |
| 14 | 1 | `uint8` | `motor_fault_flags_lsb` |

### `CMD_SET_SPEED` (`msg_type=0x10`, payload 1 byte)
- Byte 0: `int8 speed_pct` (`-100..100`).

### `CMD_STOP` (`msg_type=0x11`, payload 0 bytes)
- No payload.

### `CMD_PING` (`msg_type=0x12`, payload 4 bytes)
- `uint32 nonce`.

### `PONG` (`msg_type=0x13`)
- Echoes `CMD_PING` payload.

## 4) Hex examples

### `CMD_SET_SPEED +30%`, `seq=0x01`
`aa 55 01 10 00 01 01 00 1e 50 e1`

### `CMD_STOP`, `seq=0x02`
`aa 55 01 11 00 02 00 00 db 8b`

### `CMD_PING nonce=0x12345678`, `seq=0x03`
`aa 55 01 12 00 03 04 00 78 56 34 12 3c 15`

### `TELEMETRY sample`, `seq=0x10`
Decoded payload values:
- `timestamp_ms=5000`
- `emg=400`, `fsr2=800`, `fsr1=576`
- `hall_bits=0b00000101` (H1=1,H2=0,H3=1)
- `motor_speed_pct=-20`, `motor_duty_pct=20`, `motor_direction=REV`, `fault=0`

Frame:
`aa 55 01 01 00 10 0f 00 88 13 00 00 90 01 20 03 40 02 05 ec 14 02 00 67 73`

## 5) Backward-compatible extension rule

For future MPU6050 fields:
1. Keep `version=0x01` and append optional extension bytes only if both sides agree via flags or dedicated capability exchange.
2. Or introduce `version=0x02` with new payload layout and keep v1 decoder unchanged.

