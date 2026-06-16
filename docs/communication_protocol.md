# NNC6521 微电流美容仪通讯协议架构

## 概述

本文档描述微电流美容仪项目的通讯协议架构，参考 WorkItem-djm-GL-01 项目的 message.c/message.h 设计。

---

## 协议帧格式

### 帧结构

```
+----------+----------+--------+----------+----------+--------+--------+----------+----------+--------+
| 帧头1    | 帧头2    | 长度   | 设备ID_H | 设备ID_L | 类型   | 状态   | 数据域   | CRC16_H  | CRC16_L |
| (1 Byte) | (1 Byte) | (1 B)  | (1 Byte) | (1 Byte) | (1 B)  | (1 B)  | (N Bytes)| (1 Byte) | (1 Byte)|
| 0x55     | 0xAA     | Len    | 0x00     | 0x48     | Type   | Status | Data[]   | CRC_H    | CRC_L   |
+----------+----------+--------+----------+----------+--------+--------+----------+----------+--------+
```

### 帧字段定义

| 字段 | 偏移 | 长度 | 说明 |
|------|------|------|------|
| FRAME_HEAD1 | 0 | 1 | 帧头1，固定值 0x55 |
| FRAME_HEAD2 | 1 | 1 | 帧头2，固定值 0xAA |
| LENGTH | 2 | 1 | 数据长度（不含帧头和CRC） |
| DEVICE_ID_H | 3 | 1 | 设备ID高字节，固定值 0x00 |
| DEVICE_ID_L | 4 | 1 | 设备ID低字节，固定值 0x48 |
| CMD_TYPE | 5 | 1 | 命令类型 |
| CMD_STATUS | 6 | 1 | 命令状态 |
| DATA[] | 7 | N | 数据域（N = LENGTH - 4） |
| CRC16_H | 7+N | 1 | CRC16校验高字节 |
| CRC16_L | 8+N | 1 | CRC16校验低字节 |

---

## 命令类型定义

```c
#define FRAME_TYPE_ACT      0x44    // 动作命令（控制指令）
#define FRAME_TYPE_GET      0x45    // 参数获取（查询指令）
```

---

## 帧状态定义

```c
#define FRAME_STATE_ASK     0x02    // 上位请求（Android → STM32）
#define FRAME_STATE_ACK     0x01    // 下位应答（STM32 → Android）
```

---

## 设备ID定义

```c
#define DEVICE_ID_H         0x00    // 设备ID高字节
#define DEVICE_ID_L         0x48    // 设备ID低字节
#define DEVICE_VERSION      0x01    // 固件版本号
```

---

## CRC16 校验算法

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

**校验范围**：从 LENGTH 字段到 DATA[] 最后一个字节

---

## 解析状态机

### 状态定义

```c
typedef enum {
    Decode_Step_0 = 0,  // 等待帧头1 (0x55)
    Decode_Step_1,      // 等待帧头2 (0xAA)
    Decode_Step_2,      // 接收长度
    Decode_Step_3,      // 验证设备ID_H
    Decode_Step_4,      // 验证设备ID_L
    Decode_Step_5,      // 接收数据域
    Decode_Step_6,      // 接收CRC16_H
    Decode_Step_7       // 接收CRC16_L
} DecodeStep_StructType;
```

### 解析流程图

```
┌─────────────┐
│  Step 0     │ ← 等待 0x55
│  等待帧头1   │
└──────┬──────┘
       │ 收到 0x55
       ▼
┌─────────────┐
│  Step 1     │ ← 等待 0xAA
│  等待帧头2   │
└──────┬──────┘
       │ 收到 0xAA
       ▼
┌─────────────┐
│  Step 2     │ ← 接收长度字节
│  接收长度    │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  Step 3     │ ← 验证 0x00
│  验证ID_H   │
└──────┬──────┘
       │ 匹配
       ▼
┌─────────────┐
│  Step 4     │ ← 验证 0x48
│  验证ID_L   │
└──────┬──────┘
       │ 匹配
       ▼
┌─────────────┐
│  Step 5     │ ← 接收 LENGTH-4 字节数据
│  接收数据域  │
└──────┬──────┘
       │ 数据接收完成
       ▼
┌─────────────┐
│  Step 6     │ ← 接收CRC高字节
│  接收CRC_H  │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  Step 7     │ ← 接收CRC低字节
│  接收CRC_L  │
└──────┬──────┘
       │ CRC校验
       ▼
   ┌───────┐
   │ 验证   │ → 通过：执行命令
   │ CRC    │ → 失败：丢弃帧
   └───────┘
```

---

## 命令定义

### 动作命令 (FRAME_TYPE_ACT = 0x44)

| 命令码 | 功能 | 数据域 | 说明 |
|--------|------|--------|------|
| 0x01 | 通道波形输出 | [Channel, WaveType, PointNum, ...] | 指定通道输出波形 |
| 0x02 | 停止输出 | [Channel] | 停止指定通道波形 |
| 0x03 | 设置电流 | [Channel, Current_H, Current_L] | 设置通道电流强度 |
| 0x04 | 设置频率 | [Channel, Freq_H, Freq_L] | 设置波形频率 |
| 0x05 | 使能LOD | [Enable] | 使能/禁用脱落检测 |
| 0x06 | 使能SCD | [Enable] | 使能/禁用短路检测 |
| 0x07 | 复位芯片 | [ChipID] | 复位指定芯片 |
| 0x08 | 读取状态 | [ChipID] | 读取芯片状态 |
| 0x09 | 电流挡位调节 | [Channel, GearLevel] | 调节电流输出档位 |
| 0x0A | 自定义波形 | [Channel, PointNum, Data...] | 下发自定义波形数据 |
| 0x0B | 幅度调节 | [Channel, Amplitude_H, Amplitude_L] | 实时调节幅度 |

### 电流挡位定义

| 档位 | 值 | 电流范围 | 说明 |
|------|-----|----------|------|
| GEAR_1 | 0x01 | 0~100μA | 最低档位，适合敏感肌肤 |
| GEAR_2 | 0x02 | 100~200μA | 低档位 |
| GEAR_3 | 0x03 | 200~300μA | 中低档位 |
| GEAR_4 | 0x04 | 300~400μA | 中档位 |
| GEAR_5 | 0x05 | 400~500μA | 中高档位 |
| GEAR_6 | 0x06 | 500~600μA | 高档位 |
| GEAR_7 | 0x07 | 600~700μA | 最高档位 |

### 电流挡位调节命令格式

```
帧头1    帧头2    长度   设备ID   类型   状态   命令码   通道   挡位   CRC16
0x55     0xAA     0x07   00 48   0x44   0x02   0x09    CH    Gear   CRC
```

**数据域说明**：
- Channel: 0x00=CH0, 0x01=CH1
- GearLevel: 0x01~0x07 对应 GEAR_1~GEAR_7

**使用示例**：
```c
// 设置通道0为档位4（中档，300~400μA）
uint8_t cmd[2] = {0};
cmd[0] = 0x00;  // 通道：CH0
cmd[1] = 0x04;  // 挡位：GEAR_4

Protocol_Send_Command(2, FRAME_TYPE_ACT, FRAME_STATE_ASK, cmd);
```

### 查询命令 (FRAME_TYPE_GET = 0x45)

| 命令码 | 功能 | 数据域 | 说明 |
|--------|------|--------|------|
| 0x01 | 查询版本 | [] | 获取固件版本号 |
| 0x02 | 查询状态 | [] | 获取设备运行状态 |
| 0x03 | 查询LOD | [Channel] | 获取脱落检测状态 |
| 0x04 | 查询SCD | [Channel] | 获取短路检测状态 |
| 0x05 | 查询电流 | [Channel] | 获取当前电流值 |

---

## 数据结构定义

### 接收缓冲区

```c
#define MAX_DATA_LENGTH 512

typedef struct {
    uint8_t rxBuffer[MAX_DATA_LENGTH];  // 循环队列缓冲区
    volatile rt_uint16_t rx_index;      // 数据索引
    volatile rt_uint16_t head;          // 队列头指针（读位置）
    volatile rt_uint16_t tail;          // 队列尾指针（写位置）
    rt_mutex_t lock;                    // 互斥锁
} xUsart_Structure;
```

### 命令缓冲区

```c
#define CMD_BUFFER_SIZE 64

typedef struct {
    uint8_t buffer[CMD_BUFFER_SIZE];    // 命令数据缓冲区
    uint8_t length;                     // 数据长度
    uint8_t type;                       // 命令类型
    uint8_t status;                     // 命令状态
    uint16_t crc_received;              // 接收的CRC值
    uint16_t crc_calculated;            // 计算的CRC值
} CMD_Buffer_Structure;
```

---

## API 函数

### 初始化

```c
/**
 * @brief  初始化串口协议栈
 * @param  None
 * @return 0=成功, -1=失败
 */
int Protocol_Init(void);
```

### 接收解析

```c
/**
 * @brief  从接收缓冲区解析命令
 * @param  dev: 串口设备句柄
 * @param  USART_ID: 串口ID
 * @return CMD_TRUE=解析成功, CMD_ERROR=解析失败
 */
uint8_t Protocol_Get_Command(rt_device_t dev, uint8_t USART_ID);
```

### 发送命令

```c
/**
 * @brief  发送命令到上位机
 * @param  DataLen: 数据域长度
 * @param  CmdType: 命令类型 (FRAME_TYPE_ACT/FRAME_TYPE_GET)
 * @param  CmdStatus: 命令状态 (FRAME_STATE_ASK/FRAME_STATE_ACK)
 * @param  DataBuf: 数据域指针
 * @return None
 */
void Protocol_Send_Command(uint8_t DataLen, uint8_t CmdType, 
                           uint8_t CmdStatus, uint8_t* DataBuf);
```

### 命令处理

```c
/**
 * @brief  处理接收到的命令
 * @param  dev: 串口设备句柄
 * @param  CmdBuf: 命令缓冲区指针
 * @return None
 */
void Protocol_Operation(rt_device_t dev, uint8_t* CmdBuf);
```

---

## 使用示例

### 发送波形输出命令

```c
// 输出正弦波到通道0
uint8_t cmd[5] = {0};
cmd[0] = 0x01;              // 命令码：通道波形输出
cmd[1] = 0x00;              // 通道：CH0
cmd[2] = 0x00;              // 波形类型：正弦波
cmd[3] = 0x40;              // 点数：64
cmd[4] = 0x04;              // 电流档位

Protocol_Send_Command(5, FRAME_TYPE_ACT, FRAME_STATE_ASK, cmd);
```

### 发送查询命令

```c
// 查询芯片1状态
uint8_t cmd[2] = {0};
cmd[0] = 0x08;              // 命令码：读取状态
cmd[1] = 0x00;              // 芯片ID：CHIP_1

Protocol_Send_Command(2, FRAME_TYPE_GET, FRAME_STATE_ASK, cmd);
```

### 接收处理示例

```c
void Protocol_Operation(rt_device_t dev, uint8_t* CmdBuf)
{
    switch(*(CmdBuf + 5))  // CMD_TYPE
    {
        case FRAME_TYPE_ACT:
        {
            switch(*(CmdBuf + 7))  // 命令码
            {
                case 0x01:  // 通道波形输出
                {
                    uint8_t channel = *(CmdBuf + 8);
                    uint8_t waveform = *(CmdBuf + 9);
                    uint8_t pointNum = *(CmdBuf + 10);
                    uint8_t current = *(CmdBuf + 11);
                    
                    // 执行波形输出
                    nnc6521_preloaded_waveform(
                        NNC6521_CHIP_1,
                        channel,
                        waveform,
                        pointNum,
                        current,
                        100, 100, 1000, 50
                    );
                    
                    // 应答
                    uint8_t ack[2] = {0x01, 0x00};  // 成功
                    Protocol_Send_Command(2, FRAME_TYPE_ACT, FRAME_STATE_ACK, ack);
                } break;
                
                // ... 其他命令处理
            }
        } break;
        
        case FRAME_TYPE_GET:
        {
            switch(*(CmdBuf + 7))  // 命令码
            {
                case 0x01:  // 查询版本
                {
                    uint8_t ack[2] = {0x01, DEVICE_VERSION};
                    Protocol_Send_Command(2, FRAME_TYPE_GET, FRAME_STATE_ACK, ack);
                } break;
                
                // ... 其他查询处理
            }
        } break;
    }
}
```

---

## 注意事项

1. **CRC校验范围**：从 LENGTH 字段开始到 DATA[] 最后一个字节
2. **帧超时**：建议设置 100ms 帧超时，超时后重置解析状态机
3. **缓冲区管理**：使用循环队列 + 互斥锁保护并发访问
4. **命令应答**：所有动作命令必须返回应答帧
5. **错误处理**：CRC校验失败时丢弃整帧并记录错误

---

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0 | 2026-06-16 | 初始版本，参考 WorkItem-djm-GL-01 架构 |
