#include "spwm.h"
#include <math.h>
#include <stdlib.h>

// 生成正弦表
uint16_t* spwm_generate_sin_table(uint16_t size, uint16_t max_value) {
    uint16_t *table = (uint16_t*)malloc(size * sizeof(uint16_t));
    if (table == NULL) return NULL;
    
    for (uint16_t i = 0; i < size; i++) {
        float angle = 2 * M_PI * i / size;
        table[i] = (uint16_t)((max_value / 2.0f) * (1 + sinf(angle)));
    }
    
    return table;
}

// SPWM 初始化
int spwm_init(spwm_handle_t *handle, const spwm_config_t *config) {
    if (handle == NULL || config == NULL) return -1;
    
    // 保存配置
    handle->config = *config;
    
    // 生成正弦表(如果未提供)
    if (handle->config.sin_table == NULL) {
        // 实际最小大小(根据开关频率/正弦频率计算，至少4倍)
				// 保证至少4个点每周期，但不超过1024
				uint16_t min_size = (handle->config.switching_freq / handle->config.sine_freq) / 4;
				if (min_size < 64) min_size = 64;
				if (min_size > 1024) min_size = 1024;
        
        handle->config.sin_table_size = min_size;
        
        // 计算最大值(ARR/2)
        uint32_t arr_value = handle->config.timer_freq / handle->config.switching_freq;
        uint16_t max_value = arr_value / 2;
        
        handle->config.sin_table = spwm_generate_sin_table(min_size, max_value);
        if (handle->config.sin_table == NULL) return -2;
    }
    
    // 初始化定时器周期
    uint32_t arr_value = handle->config.timer_freq / handle->config.switching_freq;
    handle->config.htim->Instance->ARR = arr_value;
    
    // 配置死区时间(如果使用互补输出)
    if (handle->config.complementary && handle->config.dead_time > 0) {
        handle->config.htim->Instance->BDTR |= (handle->config.dead_time << 0) | TIM_BDTR_DTG_0;
        handle->config.htim->Instance->BDTR |= TIM_BDTR_MOE;
    }
    
    // 初始化状态变量
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
    
    // 启动定时器基础计数器和中断
    HAL_TIM_Base_Start_IT(handle->config.htim);
    
 
    HAL_TIM_PWM_Start(handle->config.htim, handle->config.channel);
    
    if (handle->config.complementary) {
        HAL_TIMEx_PWMN_Start(handle->config.htim, handle->config.channel);
    }
    
    handle->enabled = true;
}

// 停止SPWM
void spwm_stop(spwm_handle_t *handle) {
    if (handle == NULL) return;
    
    // 停止定时器PWM输出
    HAL_TIM_PWM_Stop(handle->config.htim, handle->config.channel);
    
    if (handle->config.complementary) {
        HAL_TIMEx_PWMN_Stop(handle->config.htim, handle->config.channel);
    }
    
    handle->enabled = false;
}

// 设置正弦波频率
void spwm_set_frequency(spwm_handle_t *handle, uint32_t sine_freq) {
    if (handle == NULL || sine_freq == 0) return;
    
    handle->config.sine_freq = sine_freq;
    handle->index_increment = (float)handle->config.sin_table_size * handle->config.sine_freq / handle->config.switching_freq;
}

// 设置幅度
void spwm_set_amplitude(spwm_handle_t *handle, float amplitude) {
    if (handle == NULL) return;
    
    // 限制幅度范围
    if (amplitude < 0.0f) amplitude = 0.0f;
    if (amplitude > 1.0f) amplitude = 1.0f;
    
    handle->amplitude = amplitude;
}

// 设置开关频率
void spwm_set_switching_freq(spwm_handle_t *handle, uint32_t switching_freq) {
    if (handle == NULL || switching_freq == 0) return;
    
    handle->config.switching_freq = switching_freq;
    
    // 重新计算ARR值
    uint32_t arr_value = handle->config.timer_freq / switching_freq;
    handle->config.htim->Instance->ARR = arr_value;
    
    // 重新生成正弦表
    if (handle->config.sin_table != NULL) {
        free(handle->config.sin_table);
    }
    
    uint16_t max_value = arr_value / 2;
    handle->config.sin_table = spwm_generate_sin_table(handle->config.sin_table_size, max_value);
    
    // 更新索引增量
    handle->index_increment = (float)handle->config.sin_table_size * handle->config.sine_freq / handle->config.switching_freq;
}

// 设置死区时间
void spwm_set_dead_time(spwm_handle_t *handle, uint16_t dead_time) {
    if (handle == NULL) return;
    
    handle->config.dead_time = dead_time;
    
    if (handle->config.complementary) {
        handle->config.htim->Instance->BDTR &= ~0xFF;
        handle->config.htim->Instance->BDTR |= (dead_time << 0) | TIM_BDTR_DTG_0;
    }
}
//spwm数据更新，放在定时器中断里
void spwm_update(spwm_handle_t *handle) {
    if (handle == NULL || !handle->enabled) return;
    
    
    handle->index_float += handle->index_increment;
    if (handle->index_float >= handle->config.sin_table_size) {
        handle->index_float -= handle->config.sin_table_size;
    }
    handle->index = (uint16_t)handle->index_float;  

		
    uint16_t duty_cycle = (uint16_t)(handle->config.sin_table[handle->index] * handle->amplitude);
    
    // ！！！修复：将计算出的占空比写入定时器比较寄存器 ！！！
    __HAL_TIM_SET_COMPARE(handle->config.htim, handle->config.channel, duty_cycle);
    
    // 如果是三相SPWM，还需要设置其他通道
    // __HAL_TIM_SET_COMPARE(handle->config.htim, TIM_CHANNEL_2, duty_cycle_for_phase_b);
    // __HAL_TIM_SET_COMPARE(handle->config.htim, TIM_CHANNEL_3, duty_cycle_for_phase_c);
}
