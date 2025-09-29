// spwm.h
#ifndef __SPWM_H__
#define __SPWM_H__

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define M_PI 3.1415

// SPWM 配置结构体
typedef struct {
    TIM_HandleTypeDef *htim;      // 使用的定时器句柄
    uint32_t channel;             // 定时器通道
    uint32_t timer_freq;          // 定时器时钟频率(Hz)
    uint32_t switching_freq;      // 开关频率(Hz)
    uint32_t sine_freq;           // 正弦波频率(Hz)
    uint16_t *sin_table;          // 正弦波表
    uint16_t sin_table_size;      // 正弦表大小
    uint16_t dead_time;           // 死区时间(时钟周期)
    bool complementary;           // 是否使用互补输出
    TIM_HandleTypeDef *htim_comp; // 互补定时器(可选)
    uint32_t channel_comp;        // 互补通道(可选)
} spwm_config_t;

// SPWM 句柄
typedef struct {
    spwm_config_t config;
    volatile uint32_t index;      // 整数索引
    bool enabled;
    float amplitude;               // 幅度系数(0.0-1.0)
    float index_float;             // 浮点索引
    float index_increment;         // 索引增量
} spwm_handle_t;

// 初始化
int spwm_init(spwm_handle_t *handle, const spwm_config_t *config);

// 启动/停止
void spwm_start(spwm_handle_t *handle);
void spwm_stop(spwm_handle_t *handle);

// 参数设置
void spwm_set_frequency(spwm_handle_t *handle, uint32_t sine_freq);
void spwm_set_amplitude(spwm_handle_t *handle, float amplitude);
void spwm_set_switching_freq(spwm_handle_t *handle, uint32_t switching_freq);

// 更新函数(在定时器中断中调用)
void spwm_update(spwm_handle_t *handle);

// 辅助函数
uint16_t* spwm_generate_sin_table(uint16_t size, uint16_t max_value);
void spwm_set_dead_time(spwm_handle_t *handle, uint16_t dead_time);

#endif /* __SPWM_H__ */

