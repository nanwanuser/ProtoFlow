/**
  ******************************************************************************
  * @file    serial_protocol.h
  * @brief   Serial communication protocol library 
  * @version V1.1.0
  ******************************************************************************
  */

#pragma once
#include <stdint.h>

// 用户可配置参数 ---------------------------------------------------------------
#define FRAME_HEADER      0xAA55      // 帧头（需设置2字节）
#define FRAME_END         0x55AA      // 帧尾（需设置2字节）
#define MAX_DATA_LENGTH   256         // 最大数据长度(建议根据实际情况，减少栈大小)
#define USE_CRC16         1           // 使用CRC16校验（0-禁用 1-启用）

// 类型定义 --------------------------------------------------------------------
typedef enum {
    PKG_OK = 0,         // 数据包正确
    PKG_HEADER_ERR,     // 帧头错误
    PKG_LENGTH_ERR,     // 长度错误
    PKG_CRC_ERR,        // CRC校验错误
    PKG_END_ERR         // 帧尾错误
} PkgStatus;

// 用户必须实现的硬件抽象函数 ----------------------------------------------------
// 串口发送函数（用户需实现）
void user_uart_transmit(uint8_t *data, uint16_t len);

// 数据包接收回调（用户需处理）
void user_package_handler(uint8_t cmd, uint8_t *data, uint16_t len);

// 库函数接口 ------------------------------------------------------------------
void serial_protocol_init(void);
uint16_t pack_data_transmit(uint8_t cmd, uint8_t *data, uint16_t len);
void parse_byte(uint8_t byte);
