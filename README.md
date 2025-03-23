# ProtoFlow 项目

## 特性
- 轻量级协议栈（<2KB Flash）
- 支持动态长度数据包（最大 256 字节）
- 可配置帧头帧尾（默认 0xAA55/0x55AA）
- 支持 CRC16 校验（可选启用）
- 状态机驱动解析（9 种解析状态）
- 全中断驱动设计（零阻塞）
- 自动重同步机制
- 多通信方式支持：不仅限于串口，可扩展到 SPI、I2C、CAN 等
- 适用于多种数据通信场景

## 快速开始

### 1. 添加文件到工程
```bash
# 复制以下文件到 STM32 工程目录
protoflow.h protoflow.c
```

### 2. 协议配置（serial_protocol.h）
```c
// 帧结构配置
#define FRAME_HEADER      0xAA55      // 2 字节帧头
#define FRAME_END         0x55AA      // 2 字节帧尾
#define MAX_DATA_LENGTH   256         // 最大数据长度
#define USE_CRC16         1           // 启用 CRC16 校验（0-禁用 1-启用）

// 硬件抽象声明（用户必须实现）
void user_transmit(uint8_t *data, uint16_t len);  // 通信发送函数（支持多种通信方式）
```

### 3. 示例代码集成（以串口为例）
```c
// main.c
#include "serial_protocol.h"

int main(void) {
    // HAL 初始化
    MX_USART1_UART_Init();
    
    protoflow_init();  // 协议栈初始化
    
    while(1) {
        // 主循环处理
    }
}

// 串口接收中断回调
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if(huart->Instance == USART1) {
        uint8_t data = huart->Instance->DR;
        parse_byte(data);  // 字节解析
    }
}
```

## API 说明

### 数据打包发送
```c
/**
  * @brief  打包并发送数据
  * @param  cmd    : 命令字（1 字节）
  * @param  data   : 有效载荷数据指针
  * @param  len    : 数据长度（0~MAX_DATA_LENGTH）
  * @retval 实际发送的数据包长度
  */
uint16_t pack_data_transmit(uint8_t cmd, uint8_t *data, uint16_t len);
```

### 数据解析
```c
/**
  * @brief  解析接收字节（将接收到的字节传入，解析后在 user_package_handler 中使用数据包）
  * @param  byte : 接收到的字节
  */
void parse_byte(uint8_t byte);
```

### 回调函数（用户实现）
```c
/**
  * @brief  完整数据包回调
  * @param  cmd  : 接收到的命令字
  * @param  data : 数据缓冲区指针
  * @param  len  : 有效数据长度
  */
void user_package_handler(uint8_t cmd, uint8_t *data, uint16_t len);
```

## 典型应用场景

### 数据发送（以串口为例）
```c
// 发送温湿度传感器数据
void send_sensor_data(float temp, float humidity) {
    uint8_t payload[4];
    
    // 转换为 16 位整型（0.1℃ 精度）
    uint16_t temp_raw = (uint16_t)(temp * 10);
    uint16_t humi_raw = (uint16_t)(humidity * 10);
    
    payload[0] = temp_raw >> 8;
    payload[1] = temp_raw & 0xFF;
    payload[2] = humi_raw >> 8;
    payload[3] = humi_raw & 0xFF;
    
    pack_data_transmit(0x01, payload, sizeof(payload));  // 自动发送
}
```

### 数据接收处理
```c
// 接收控制指令（示例：PWM 控制）
void user_package_handler(uint8_t cmd, uint8_t *data, uint16_t len) {
    switch(cmd) {
    case 0xA1:  // 电机控制
        if(len == 4) {
            uint16_t speed = (data[0] << 8) | data[1];
            uint16_t duration = (data[2] << 8) | data[3];
            set_motor(speed, duration);
        }
        break;
        
    case 0xA2:  // LED 亮度调节
        if(len == 2) {
            uint16_t brightness = (data[0] << 8) | data[1];
            set_led_brightness(brightness);
        }
        break;
    }
}
```

## 移植指南

### 必须实现的硬件接口
ProtoFlow 支持多种通信方式，用户需根据具体硬件实现发送函数。例如：

串口：
```c
void user_transmit(uint8_t *data, uint16_t len) {
    HAL_UART_Transmit(&huart1, data, len, 100);
}
```

SPI：
```c
void user_transmit(uint8_t *data, uint16_t len) {
    HAL_SPI_Transmit(&hspi1, data, len, 100);
}
```

I2C：
```c
void user_transmit(uint8_t *data, uint16_t len) {
    HAL_I2C_Master_Transmit(&hi2c1, DEVICE_ADDR, data, len, 100);
}
```

### 配置步骤
1. 在 protoflow.h 中配置协议参数（帧头、帧尾、最大数据长度、CRC16 等）
2. 根据通信方式实现 user_transmit 发送函数
3. 实现 user_package_handler 数据回调函数
4. 在通信接收中断中调用 parse_byte()（如串口接收中断）
5. 调用 protoflow_init() 初始化协议栈

### 注意事项
- 帧结构：Header(2) + Length(2) + Cmd(1) + Data(n) + CRC16(2) + End(2)
- 数据长度字段：包含命令字（1 字节）+ 数据长度
- CRC16：启用时数据包增加 2 字节校验码
- DMA 建议：使用 DMA 传输时，保持缓冲区有效直到发送完成
