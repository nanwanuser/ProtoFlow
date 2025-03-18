/**
  ******************************************************************************
  * @file    serial_protocol.c
  * @brief   Serial communication protocol implementation
  ******************************************************************************
  */

#include "serial_protocol.h"
#include <string.h>

// 解析状态机状态定义
typedef enum {
    STATE_WAIT_HEADER1,
    STATE_WAIT_HEADER2,
    STATE_WAIT_LENGTH,
    STATE_WAIT_LENGTH_LOW,  // 新增长度低字节等待状态
    STATE_WAIT_CMD,
    STATE_READ_DATA,
    STATE_WAIT_CRC1,
    STATE_WAIT_CRC2,
    STATE_WAIT_END1,
    STATE_WAIT_END2
} ParseState;

// 解析上下文结构体
typedef struct {
    ParseState state;
    uint16_t data_index;
    uint16_t pkg_length;
    uint8_t cmd;
    uint8_t data[MAX_DATA_LENGTH];
    uint16_t calc_crc;
    uint16_t recv_crc;
} ParseContext;

static ParseContext ctx;

// CRC16-CCITT计算（多项式0x1021）
static uint16_t crc16(uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    while(len--) {
        crc ^= (uint16_t)(*data++) << 8;
        for(int i=0; i<8; i++) {
            if(crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

void serial_protocol_init(void) {
    memset(&ctx, 0, sizeof(ctx));
    ctx.state = STATE_WAIT_HEADER1;
}

uint16_t pack_data_transmit(uint8_t cmd, uint8_t *data, uint16_t len) {
    static uint8_t buffer[MAX_DATA_LENGTH + 9]; // 足够容纳整个数据包
    uint16_t index = 0;

    // 帧头（2字节）
    buffer[index++] = (FRAME_HEADER >> 8) & 0xFF;
    buffer[index++] = FRAME_HEADER & 0xFF;

    // 长度字段（2字节，数据长度 + CMD）
    uint16_t total_len = len + 1; // +1表示CMD占用的1字节
    buffer[index++] = (total_len >> 8) & 0xFF;
    buffer[index++] = total_len & 0xFF;

    // 命令（1字节）
    buffer[index++] = cmd;

    // 数据载荷（len字节）
    if (data && len > 0) {
        memcpy(&buffer[index], data, len);
        index += len;
    }

#if USE_CRC16
    // CRC16校验（2字节）
    uint16_t crc = crc16(&buffer[2], index - 2); // 从长度字段开始计算
    buffer[index++] = (crc >> 8) & 0xFF;
    buffer[index++] = crc & 0xFF;
#endif

    // 帧尾（2字节）
    buffer[index++] = (FRAME_END >> 8) & 0xFF;
    buffer[index++] = FRAME_END & 0xFF;

    // 调用用户实现的串口发送函数
    user_uart_transmit(buffer, index);

    return index; // 返回实际发送的字节数
}

void parse_byte(uint8_t byte) {
    static uint8_t header_buf[2];
    
    switch(ctx.state) {
    case STATE_WAIT_HEADER1:
        if(byte == ((FRAME_HEADER >> 8) & 0xFF)) {
            ctx.state = STATE_WAIT_HEADER2;
        }
        break;
        
    case STATE_WAIT_HEADER2:
        if(byte == (FRAME_HEADER & 0xFF)) {
            ctx.state = STATE_WAIT_LENGTH;
            ctx.calc_crc = 0xFFFF; // 重置CRC
        } else {
            ctx.state = STATE_WAIT_HEADER1;
        }
        break;
        
    case STATE_WAIT_LENGTH:
        ctx.pkg_length = byte << 8;
        ctx.state = STATE_WAIT_LENGTH_LOW;
        break;
        
    case STATE_WAIT_LENGTH_LOW:
        ctx.pkg_length |= byte;
        if(ctx.pkg_length > MAX_DATA_LENGTH + 3) { // +3 for cmd + crc
            ctx.state = STATE_WAIT_HEADER1;
            return;
        }
        ctx.state = STATE_WAIT_CMD;
        break;
        
    case STATE_WAIT_CMD:
        ctx.cmd = byte;
        ctx.data_index = 0;
        ctx.state = STATE_READ_DATA;
        break;
        
    case STATE_READ_DATA:
        ctx.data[ctx.data_index++] = byte;
        if(ctx.data_index >= ctx.pkg_length - 1) { // -1 for cmd
            ctx.state = STATE_WAIT_CRC1;
        }
        break;
        
    case STATE_WAIT_CRC1:
        ctx.recv_crc = byte << 8;
        ctx.state = STATE_WAIT_CRC2;
        break;
        
    case STATE_WAIT_CRC2:
        ctx.recv_crc |= byte;
#if USE_CRC16
        if(ctx.calc_crc != ctx.recv_crc) {
            // CRC错误处理
            ctx.state = STATE_WAIT_HEADER1;
            return;
        }
#endif
        ctx.state = STATE_WAIT_END1;
        break;
        
    case STATE_WAIT_END1:
        if(byte == ((FRAME_END >> 8) & 0xFF)) {
            ctx.state = STATE_WAIT_END2;
        } else {
            ctx.state = STATE_WAIT_HEADER1;
        }
        break;
        
    case STATE_WAIT_END2:
        if(byte == (FRAME_END & 0xFF)) {
            // 完整数据包接收成功
            user_package_handler(ctx.cmd, ctx.data, ctx.data_index);
        }
        ctx.state = STATE_WAIT_HEADER1;
        break;
        
    default:
        ctx.state = STATE_WAIT_HEADER1;
        break;
    }

#if USE_CRC16
    // 更新CRC计算（从长度字段开始）
    if(ctx.state >= STATE_WAIT_LENGTH && ctx.state <= STATE_WAIT_CRC2) {
        ctx.calc_crc ^= (uint16_t)byte << 8;
        for(int i=0; i<8; i++) {
            if(ctx.calc_crc & 0x8000)
                ctx.calc_crc = (ctx.calc_crc << 1) ^ 0x1021;
            else
                ctx.calc_crc <<= 1;
        }
    }
#endif
}
