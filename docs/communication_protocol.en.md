# NNC6521 Microcurrent Beauty Device Communication Protocol Architecture

## Overview

This document describes the communication protocol architecture for the microcurrent beauty device project, referencing the message.c/message.h design from WorkItem-djm-GL-01 project.

---

## Protocol Frame Format

### Frame Structure

```
+----------+----------+--------+----------+----------+--------+--------+----------+----------+--------+
| Head1    | Head2    | Length | DeviceID_H| DeviceID_L| Type  | Status | Data     | CRC16_H  | CRC16_L |
| (1 Byte) | (1 Byte) | (1 B)  | (1 Byte) | (1 Byte) | (1 B)  | (1 B)  | (N Bytes)| (1 Byte) | (1 Byte)|
| 0x55     | 0xAA     | Len    | 0x00     | 0x48     | Type   | Status | Data[]   | CRC_H    | CRC_L   |
+----------+----------+--------+----------+----------+--------+--------+----------+----------+--------+
```

### Frame Field Definitions

| Field | Offset | Length | Description |
|-------|--------|--------|-------------|
| FRAME_HEAD1 | 0 | 1 | Frame header 1, fixed value 0x55 |
| FRAME_HEAD2 | 1 | 1 | Frame header 2, fixed value 0xAA |
| LENGTH | 2 | 1 | Data length (excluding header and CRC) |
| DEVICE_ID_H | 3 | 1 | Device ID high byte, fixed value 0x00 |
| DEVICE_ID_L | 4 | 1 | Device ID low byte, fixed value 0x48 |
| CMD_TYPE | 5 | 1 | Command type |
| CMD_STATUS | 6 | 1 | Command status |
| DATA[] | 7 | N | Data field (N = LENGTH - 4) |
| CRC16_H | 7+N | 1 | CRC16 checksum high byte |
| CRC16_L | 8+N | 1 | CRC16 checksum low byte |

---

## Command Type Definitions

```c
#define FRAME_TYPE_ACT      0x44    // Action command (control command)
#define FRAME_TYPE_GET      0x45    // Parameter query (read command)
```

---

## Frame Status Definitions

```c
#define FRAME_STATE_ASK     0x02    // Host request (Android → STM32)
#define FRAME_STATE_ACK     0x01    // Device response (STM32 → Android)
```

---

## Device ID Definitions

```c
#define DEVICE_ID_H         0x00    // Device ID high byte
#define DEVICE_ID_L         0x48    // Device ID low byte
#define DEVICE_VERSION      0x01    // Firmware version
```

---

## CRC16 Checksum Algorithm

采用 Modbus CRC16 校验算法：

```c
uint16_t CrcCalc_Crc16Modbus(uint8_t *dat, uint8_t len)
{
    uint16_t CRC_index = 0xFFFF;
    uint16_t buffer;
    uint8_t i, j;
    
    for(i = 0; i < len; i++) {
        buffer = dat[i];
        CRC_index ^= buffer;
        for(j = 0; j < 8; j++) {
            if(CRC_index & 0x0001) {
                CRC_index >>= 1;
                CRC_index ^= 0xA001;
            } else {
                CRC_index >>= 1;
            }
        }
    }
    return CRC_index;
}
```

**Checksum Scope**: From LENGTH field to the last byte of DATA[]

---

## Parser State Machine

### State Definition

```c
typedef enum {
    Decode_Step_0 = 0,  // Wait for header1 (0x55)
    Decode_Step_1,      // Wait for header2 (0xAA)
    Decode_Step_2,      // Receive length
    Decode_Step_3,      // Verify deviceID_H
    Decode_Step_4,      // Verify deviceID_L
    Decode_Step_5,      // Receive data field
    Decode_Step_6,      // Receive CRC16_H
    Decode_Step_7       // Receive CRC16_L
} DecodeStep_StructType;
```

### Parser Flow Diagram

```
┌─────────────┐
│  Step 0     │ ← Wait for 0x55
│  Wait Head1 │
└──────┬──────┘
       │ Received 0x55
       ▼
┌─────────────┐
│  Step 1     │ ← Wait for 0xAA
│  Wait Head2 │
└──────┬──────┘
       │ Received 0xAA
       ▼
┌─────────────┐
│  Step 2     │ ← Receive length byte
│  Recv Len   │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  Step 3     │ ← Verify 0x00
│  Verify ID_H│
└──────┬──────┘
       │ Match
       ▼
┌─────────────┐
│  Step 4     │ ← Verify 0x48
│  Verify ID_L│
└──────┬──────┘
       │ Match
       ▼
┌─────────────┐
│  Step 5     │ ← Receive LENGTH-4 bytes data
│  Recv Data  │
└──────┬──────┘
       │ Data received
       ▼
┌─────────────┐
│  Step 6     │ ← Receive CRC high byte
│  Recv CRC_H │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  Step 7     │ ← Receive CRC low byte
│  Recv CRC_L │
└──────┬──────┘
       │ CRC verification
       ▼
   ┌───────┐
   │ Verify │ → Pass: Execute command
   │  CRC   │ → Fail: Discard frame
   └───────┘
```

---

## Command Definitions

### Action Commands (FRAME_TYPE_ACT = 0x44)

| Code | Function | Data Field | Description |
|------|----------|------------|-------------|
| 0x01 | Channel waveform output | [Channel, WaveType, PointNum, ...] | Output waveform on specified channel |
| 0x02 | Stop output | [Channel] | Stop waveform on specified channel |
| 0x03 | Set current | [Channel, Current_H, Current_L] | Set channel current intensity |
| 0x04 | Set frequency | [Channel, Freq_H, Freq_L] | Set waveform frequency |
| 0x05 | Enable LOD | [Enable] | Enable/disable lead-off detection |
| 0x06 | Enable SCD | [Enable] | Enable/disable short-circuit detection |
| 0x07 | Reset chip | [ChipID] | Reset specified chip |
| 0x08 | Read status | [ChipID] | Read chip status |
| 0x09 | Current gear adjust | [Channel, GearLevel] | Adjust current output gear level |
| 0x0A | Custom waveform | [Channel, PointNum, Data...] | Send custom waveform data |
| 0x0B | Amplitude adjust | [Channel, Amplitude_H, Amplitude_L] | Real-time amplitude adjustment |

### Current Gear Definitions

| Gear | Value | Current Range | Description |
|------|-------|---------------|-------------|
| GEAR_1 | 0x01 | 0~100μA | Lowest gear, suitable for sensitive skin |
| GEAR_2 | 0x02 | 100~200μA | Low gear |
| GEAR_3 | 0x03 | 200~300μA | Medium-low gear |
| GEAR_4 | 0x04 | 300~400μA | Medium gear |
| GEAR_5 | 0x05 | 400~500μA | Medium-high gear |
| GEAR_6 | 0x06 | 500~600μA | High gear |
| GEAR_7 | 0x07 | 600~700μA | Highest gear |

### Current Gear Adjust Command Format

```
Head1    Head2    Length  DeviceID  Type   Status  CmdCode  Channel  Gear   CRC16
0x55     0xAA     0x07    00 48    0x44   0x02    0x09     CH      Gear   CRC
```

**Data Field Description**:
- Channel: 0x00=CH0, 0x01=CH1
- GearLevel: 0x01~0x07 corresponds to GEAR_1~GEAR_7

**Usage Example**:
```c
// Set channel 0 to gear 4 (medium, 300~400μA)
uint8_t cmd[2] = {0};
cmd[0] = 0x00;  // Channel: CH0
cmd[1] = 0x04;  // Gear: GEAR_4

Protocol_Send_Command(2, FRAME_TYPE_ACT, FRAME_STATE_ASK, cmd);
```

### Query Commands (FRAME_TYPE_GET = 0x45)

| Code | Function | Data Field | Description |
|------|----------|------------|-------------|
| 0x01 | Query version | [] | Get firmware version |
| 0x02 | Query status | [] | Get device running status |
| 0x03 | Query LOD | [Channel] | Get lead-off detection status |
| 0x04 | Query SCD | [Channel] | Get short-circuit detection status |
| 0x05 | Query current | [Channel] | Get current current value |

---

## Data Structure Definitions

### Receive Buffer

```c
#define MAX_DATA_LENGTH 512

typedef struct {
    uint8_t rxBuffer[MAX_DATA_LENGTH];  // Circular queue buffer
    volatile rt_uint16_t rx_index;      // Data index
    volatile rt_uint16_t head;          // Queue head pointer (read position)
    volatile rt_uint16_t tail;          // Queue tail pointer (write position)
    rt_mutex_t lock;                    // Mutex lock
} xUsart_Structure;
```

### Command Buffer

```c
#define CMD_BUFFER_SIZE 64

typedef struct {
    uint8_t buffer[CMD_BUFFER_SIZE];    // Command data buffer
    uint8_t length;                     // Data length
    uint8_t type;                       // Command type
    uint8_t status;                     // Command status
    uint16_t crc_received;              // Received CRC value
    uint16_t crc_calculated;            // Calculated CRC value
} CMD_Buffer_Structure;
```

---

## API Functions

### Initialization

```c
/**
 * @brief  Initialize serial protocol stack
 * @param  None
 * @return 0=success, -1=failure
 */
int Protocol_Init(void);
```

### Receive Parsing

```c
/**
 * @brief  Parse command from receive buffer
 * @param  dev: Serial device handle
 * @param  USART_ID: Serial port ID
 * @return CMD_TRUE=parsed successfully, CMD_ERROR=parse failed
 */
uint8_t Protocol_Get_Command(rt_device_t dev, uint8_t USART_ID);
```

### Send Command

```c
/**
 * @brief  Send command to host
 * @param  DataLen: Data field length
 * @param  CmdType: Command type (FRAME_TYPE_ACT/FRAME_TYPE_GET)
 * @param  CmdStatus: Command status (FRAME_STATE_ASK/FRAME_STATE_ACK)
 * @param  DataBuf: Data field pointer
 * @return None
 */
void Protocol_Send_Command(uint8_t DataLen, uint8_t CmdType, 
                           uint8_t CmdStatus, uint8_t* DataBuf);
```

### Command Processing

```c
/**
 * @brief  Process received command
 * @param  dev: Serial device handle
 * @param  CmdBuf: Command buffer pointer
 * @return None
 */
void Protocol_Operation(rt_device_t dev, uint8_t* CmdBuf);
```

---

## Usage Examples

### Send Waveform Output Command

```c
// Output sine wave on channel 0
uint8_t cmd[5] = {0};
cmd[0] = 0x01;              // Command: Channel waveform output
cmd[1] = 0x00;              // Channel: CH0
cmd[2] = 0x00;              // Waveform: Sine
cmd[3] = 0x40;              // Points: 64
cmd[4] = 0x04;              // Current level

Protocol_Send_Command(5, FRAME_TYPE_ACT, FRAME_STATE_ASK, cmd);
```

### Send Query Command

```c
// Query chip 1 status
uint8_t cmd[2] = {0};
cmd[0] = 0x08;              // Command: Read status
cmd[1] = 0x00;              // Chip ID: CHIP_1

Protocol_Send_Command(2, FRAME_TYPE_GET, FRAME_STATE_ASK, cmd);
```

### Receive Processing Example

```c
void Protocol_Operation(rt_device_t dev, uint8_t* CmdBuf)
{
    switch(*(CmdBuf + 5))  // CMD_TYPE
    {
        case FRAME_TYPE_ACT:
        {
            switch(*(CmdBuf + 7))  // Command code
            {
                case 0x01:  // Channel waveform output
                {
                    uint8_t channel = *(CmdBuf + 8);
                    uint8_t waveform = *(CmdBuf + 9);
                    uint8_t pointNum = *(CmdBuf + 10);
                    uint8_t current = *(CmdBuf + 11);
                    
                    // Execute waveform output
                    nnc6521_preloaded_waveform(
                        NNC6521_CHIP_1,
                        channel,
                        waveform,
                        pointNum,
                        current,
                        100, 100, 1000, 50
                    );
                    
                    // Send acknowledgment
                    uint8_t ack[2] = {0x01, 0x00};  // Success
                    Protocol_Send_Command(2, FRAME_TYPE_ACT, FRAME_STATE_ACK, ack);
                } break;
                
                // ... other command handlers
            }
        } break;
        
        case FRAME_TYPE_GET:
        {
            switch(*(CmdBuf + 7))  // Command code
            {
                case 0x01:  // Query version
                {
                    uint8_t ack[2] = {0x01, DEVICE_VERSION};
                    Protocol_Send_Command(2, FRAME_TYPE_GET, FRAME_STATE_ACK, ack);
                } break;
                
                // ... other query handlers
            }
        } break;
    }
}
```

---

## Important Notes

1. **CRC Checksum Scope**: From LENGTH field to the last byte of DATA[]
2. **Frame Timeout**: Recommended 100ms frame timeout, reset parser state machine on timeout
3. **Buffer Management**: Use circular queue + mutex lock for concurrent access protection
4. **Command Response**: All action commands must return acknowledgment frame
5. **Error Handling**: Discard entire frame and log error on CRC verification failure

---

## Revision History

| Version | Date | Description |
|---------|------|-------------|
| 1.0 | 2026-06-16 | Initial version, referencing WorkItem-djm-GL-01 architecture |
