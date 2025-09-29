#include "spwm.h"
#include <math.h>
#include <stdlib.h>

// �������ұ�
uint16_t* spwm_generate_sin_table(uint16_t size, uint16_t max_value) {
    uint16_t *table = (uint16_t*)malloc(size * sizeof(uint16_t));
    if (table == NULL) return NULL;
    
    for (uint16_t i = 0; i < size; i++) {
        float angle = 2 * M_PI * i / size;
        table[i] = (uint16_t)((max_value / 2.0f) * (1 + sinf(angle)));
    }
    
    return table;
}

// SPWM ��ʼ��
int spwm_init(spwm_handle_t *handle, const spwm_config_t *config) {
    if (handle == NULL || config == NULL) return -1;
    
    // ��������
    handle->config = *config;
    
    // �������ұ�(���δ�ṩ)
    if (handle->config.sin_table == NULL) {
        // ʵ����С��С(���ݿ���Ƶ��/����Ƶ�ʼ��㣬����4��)
				// ��֤����4����ÿ���ڣ���������1024
				uint16_t min_size = (handle->config.switching_freq / handle->config.sine_freq) / 4;
				if (min_size < 64) min_size = 64;
				if (min_size > 1024) min_size = 1024;
        
        handle->config.sin_table_size = min_size;
        
        // �������ֵ(ARR/2)
        uint32_t arr_value = handle->config.timer_freq / handle->config.switching_freq;
        uint16_t max_value = arr_value / 2;
        
        handle->config.sin_table = spwm_generate_sin_table(min_size, max_value);
        if (handle->config.sin_table == NULL) return -2;
    }
    
    // ��ʼ����ʱ������
    uint32_t arr_value = handle->config.timer_freq / handle->config.switching_freq;
    handle->config.htim->Instance->ARR = arr_value;
    
    // ��������ʱ��(���ʹ�û������)
    if (handle->config.complementary && handle->config.dead_time > 0) {
        handle->config.htim->Instance->BDTR |= (handle->config.dead_time << 0) | TIM_BDTR_DTG_0;
        handle->config.htim->Instance->BDTR |= TIM_BDTR_MOE;
    }
    
    // ��ʼ��״̬����
    handle->index = 0;
    handle->enabled = false;
    handle->amplitude = 1.0f;
    handle->index_float = 0.0f;
    handle->index_increment = (float)handle->config.sin_table_size * handle->config.sine_freq / handle->config.switching_freq;
    
    return 0;
}

// SPWM
void spwm_start(spwm_handle_t *handle) {
    if (handle == NULL) return;
    
    // ������ʱ���������������ж�
    HAL_TIM_Base_Start_IT(handle->config.htim);
    
 
    HAL_TIM_PWM_Start(handle->config.htim, handle->config.channel);
    
    if (handle->config.complementary) {
        HAL_TIMEx_PWMN_Start(handle->config.htim, handle->config.channel);
    }
    
    handle->enabled = true;
}

// ֹͣSPWM
void spwm_stop(spwm_handle_t *handle) {
    if (handle == NULL) return;
    
    // ֹͣ��ʱ��PWM���
    HAL_TIM_PWM_Stop(handle->config.htim, handle->config.channel);
    
    if (handle->config.complementary) {
        HAL_TIMEx_PWMN_Stop(handle->config.htim, handle->config.channel);
    }
    
    handle->enabled = false;
}

// �������Ҳ�Ƶ��
void spwm_set_frequency(spwm_handle_t *handle, uint32_t sine_freq) {
    if (handle == NULL || sine_freq == 0) return;
    
    handle->config.sine_freq = sine_freq;
    handle->index_increment = (float)handle->config.sin_table_size * handle->config.sine_freq / handle->config.switching_freq;
}

// ���÷���
void spwm_set_amplitude(spwm_handle_t *handle, float amplitude) {
    if (handle == NULL) return;
    
    // ���Ʒ��ȷ�Χ
    if (amplitude < 0.0f) amplitude = 0.0f;
    if (amplitude > 1.0f) amplitude = 1.0f;
    
    handle->amplitude = amplitude;
}

// ���ÿ���Ƶ��
void spwm_set_switching_freq(spwm_handle_t *handle, uint32_t switching_freq) {
    if (handle == NULL || switching_freq == 0) return;
    
    handle->config.switching_freq = switching_freq;
    
    // ���¼���ARRֵ
    uint32_t arr_value = handle->config.timer_freq / switching_freq;
    handle->config.htim->Instance->ARR = arr_value;
    
    // �����������ұ�
    if (handle->config.sin_table != NULL) {
        free(handle->config.sin_table);
    }
    
    uint16_t max_value = arr_value / 2;
    handle->config.sin_table = spwm_generate_sin_table(handle->config.sin_table_size, max_value);
    
    // ������������
    handle->index_increment = (float)handle->config.sin_table_size * handle->config.sine_freq / handle->config.switching_freq;
}

// ��������ʱ��
void spwm_set_dead_time(spwm_handle_t *handle, uint16_t dead_time) {
    if (handle == NULL) return;
    
    handle->config.dead_time = dead_time;
    
    if (handle->config.complementary) {
        handle->config.htim->Instance->BDTR &= ~0xFF;
        handle->config.htim->Instance->BDTR |= (dead_time << 0) | TIM_BDTR_DTG_0;
    }
}
//spwm���ݸ��£����ڶ�ʱ���ж���
void spwm_update(spwm_handle_t *handle) {
    if (handle == NULL || !handle->enabled) return;
    
    
    handle->index_float += handle->index_increment;
    if (handle->index_float >= handle->config.sin_table_size) {
        handle->index_float -= handle->config.sin_table_size;
    }
    handle->index = (uint16_t)handle->index_float;  

		
    uint16_t duty_cycle = (uint16_t)(handle->config.sin_table[handle->index] * handle->amplitude);
    
    // �������޸������������ռ�ձ�д�붨ʱ���ȽϼĴ��� ������
    __HAL_TIM_SET_COMPARE(handle->config.htim, handle->config.channel, duty_cycle);
    
    // ���������SPWM������Ҫ��������ͨ��
    // __HAL_TIM_SET_COMPARE(handle->config.htim, TIM_CHANNEL_2, duty_cycle_for_phase_b);
    // __HAL_TIM_SET_COMPARE(handle->config.htim, TIM_CHANNEL_3, duty_cycle_for_phase_c);
}
