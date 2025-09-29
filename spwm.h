// spwm.h
#ifndef __SPWM_H__
#define __SPWM_H__

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define M_PI 3.1415

// SPWM ���ýṹ��
typedef struct {
    TIM_HandleTypeDef *htim;      // ʹ�õĶ�ʱ�����
    uint32_t channel;             // ��ʱ��ͨ��
    uint32_t timer_freq;          // ��ʱ��ʱ��Ƶ��(Hz)
    uint32_t switching_freq;      // ����Ƶ��(Hz)
    uint32_t sine_freq;           // ���Ҳ�Ƶ��(Hz)
    uint16_t *sin_table;          // ���Ҳ���
    uint16_t sin_table_size;      // ���ұ��С
    uint16_t dead_time;           // ����ʱ��(ʱ������)
    bool complementary;           // �Ƿ�ʹ�û������
    TIM_HandleTypeDef *htim_comp; // ������ʱ��(��ѡ)
    uint32_t channel_comp;        // ����ͨ��(��ѡ)
} spwm_config_t;

// SPWM ���
typedef struct {
    spwm_config_t config;
    volatile uint32_t index;      // ��������
    bool enabled;
    float amplitude;               // ����ϵ��(0.0-1.0)
    float index_float;             // ��������
    float index_increment;         // ��������
} spwm_handle_t;

// ��ʼ��
int spwm_init(spwm_handle_t *handle, const spwm_config_t *config);

// ����/ֹͣ
void spwm_start(spwm_handle_t *handle);
void spwm_stop(spwm_handle_t *handle);

// ��������
void spwm_set_frequency(spwm_handle_t *handle, uint32_t sine_freq);
void spwm_set_amplitude(spwm_handle_t *handle, float amplitude);
void spwm_set_switching_freq(spwm_handle_t *handle, uint32_t switching_freq);

// ���º���(�ڶ�ʱ���ж��е���)
void spwm_update(spwm_handle_t *handle);

// ��������
uint16_t* spwm_generate_sin_table(uint16_t size, uint16_t max_value);
void spwm_set_dead_time(spwm_handle_t *handle, uint16_t dead_time);

#endif /* __SPWM_H__ */

