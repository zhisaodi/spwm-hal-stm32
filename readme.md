# spwm库

这个库涉及的功能

| **SPWM初始化**     | `spwm_init`                    | 初始化SPWM控制器句柄，配置定时器基础参数（ARR）、死区时间（若使用互补输出），并自动生成正弦表（如果用户未提供自定义表）。 | 计算 `index_increment`，**支持互补输出和死区时间插入**。     |
| ------------------ | ------------------------------ | ------------------------------------------------------------ | ------------------------------------------------------------ |
| **输出控制**       | `spwm_start` `spwm_stop`       | 启动或停止定时器的PWM输出通道以及互补输出通道（如果使能），并控制底层定时器的计数和中断。 | 调用 `HAL_TIM_PWM_Start`, `HAL_TIMEx_PWMN_Start` 等HAL库函数。 |
| **实时参数调节**   | `spwm_set_frequency`           | **动态调整**输出正弦波的基波频率。通过改变查表步长 (`index_increment`) 来实现，无需重新生成整个正弦表。 | 实现频率的**平滑调整**。                                     |
|                    | `spwm_set_amplitude`           | **动态调整**输出正弦波的电压幅值（调制比）。通过一个幅度系数 (`amplitude`) 在运行时缩放占空比。 | 幅度范围通常被限制在 `[0.0, 1.0]` 之间，对应0%至100%调制。   |
|                    | `spwm_set_switching_freq`      | **动态调整**PWM的载波频率（开关频率）。通过改变定时器的ARR值并**重新生成正弦表**来实现。 | 注意：改变开关频率会影响输出波形的精度和频谱特性。           |
|                    | `spwm_set_dead_time`           | **动态设置**死区时间（Dead Time）。用于互补输出模式，防止桥式电路的上下管直通短路。 | 直接配置定时器的死区时间寄存器 (`BDTR`)。                    |
| **核心运行时逻辑** | `spwm_update` (需在中断中调用) | 这是最核心的函数，**必须在定时器中断回调中定期调用**。它根据当前频率和幅度，计算下一个PWM脉冲的占空比，并写入定时器的比较寄存器 (`CCR`)。 | 使用浮点索引 (`index_float`) 和步进 (`index_increment`) 实现**高分辨率且平滑的**波形合成，避免查表时的量化误差。 |

## 使用方法

- 首先要声明一个结构体

  ```c
  spwm_handle_t spwm_handle;
  ```

  - 然后要配置spwm属性

```//配置spwm初始化属性
//配置spwm初始化属性
spwm_config_t spwm_config = {
        .htim = &htim1,
        .channel = TIM_CHANNEL_1,
        .timer_freq = 72000000,    // 72MHz
        .switching_freq = 50000,   // 50kHz开关频率，即方波频率，也是载波频率
        .sine_freq = 50,           // 50Hz正弦波
        .sin_table = NULL,         // 自动生成正弦表
        .sin_table_size = 0,       // 自动确定大小
        .dead_time = 10,           // 10个时钟周期的死区时间
        .complementary = true,     // 使用互补输出
    };
```



- 初始化spwm

```c
	// 初始化SPWM
	if (spwm_init(&spwm_handle, &spwm_config) != 0) {
			// 初始化失败处理
			Error_Handler();
	}
```

- 启动spwm

```c
spwm_start(&spwm_handle);
```

- 中断里更新表即可

```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim == spwm_handle.config.htim) {
        // 在定时器中断中更新SPWM
        spwm_update(&spwm_handle);
    }
}
```

然后一个互补的spwm波就成功生成了

接下来是cubemx的具体配置方法

> # 配置 CubeMX 用于 SPWM 库
>
> 基于我设计的 SPWM 库，以下是配置 STM32CubeMX 的详细步骤。我将以 STM32F103 系列为例，但原理适用于大多数 STM32 系列。
>
> ## 1. 定时器配置 (TIM)
>
> ### 选择定时器
>
> 1. 打开 CubeMX 并选择您的 STM32 型号
> 2. 在 "Pinout & Configuration" 选项卡中，选择一个高级定时器（如 TIM1 或 TIM8）或通用定时器（如 TIM2、TIM3、TIM4）
> 3. 高级定时器支持互补输出和死区插入，更适合电机控制和逆变器应用
>
> ### 配置定时器参数
>
> 1. 选择 "Internal Clock" 作为时钟源
> 2. 配置预分频器 (PSC) 和自动重载值 (ARR):
>    - 计算 ARR 值: `ARR = (定时器时钟频率) / (期望的开关频率) - 1`
>    - 例如: 72MHz 时钟, 20kHz 开关频率 → `ARR = 72000000 / 20000 - 1 = 3599`
> 3. 设置 PWM 模式:
>    - 选择 "PWM Mode 1" 或 "PWM Mode 2"
>    - 设置通道为输出
> 4. 如果使用互补输出:
>    - 启用互补通道 (CHxN)
>    - 设置死区时间 (Dead Time): 根据您的开关器件特性设置适当值
>
> ### 示例配置 (TIM1, 20kHz PWM)
>
> - Clock Source: Internal Clock
> - Channel1: PWM Generation CH1
> - Channel1N: PWM Generation CH1N (如果使用互补输出)
> - Prescaler (PSC): 0
> - Counter Period (ARR): 3599
> - Pulse: 0 (初始占空比)
> - Dead Time: 根据需求设置 (如 100ns)
>
> ## 2. 时钟配置
>
> 1. 进入 "Clock Configuration" 选项卡
> 2. 配置系统时钟为最大可用频率 (如 72MHz 对于 STM32F103)
> 3. 确保定时器时钟源已正确配置:
>    - APB1 定时器时钟 (通常为系统时钟)
>    - APB2 定时器时钟 (通常为系统时钟)
>
> ## 3. GPIO 配置
>
> 1. 在 "Pinout" 视图中，检查定时器通道对应的引脚是否已自动配置为 Alternate Function
> 2. 如果使用互补输出，确保互补通道的引脚也已配置
> 3. 根据需要配置引脚参数:
>    - Output mode: Push-Pull
>    - Maximum output speed: High (对于高频 PWM)
>
> ## 4. NVIC 配置 (可选)
>
> 如果计划使用定时器中断更新 PWM 值:
>
> 1. 在 "NVIC Configuration" 中启用定时器的更新中断 (Update interrupt)
> 2. 设置适当的中断优先级
> 3. 强烈建议使用这个，即用载波溢出中断作为调制代码，只需要勾选Update interrupt和break interrupt即可
>
> ## 5. 生成代码
>
> 1. 进入 "Project Manager" 选项卡
> 2. 设置项目名称和位置
> 3. 选择 IDE (如 MDK-ARM, STM32CubeIDE)
> 4. 在 "Code Generator" 中:
>    - 选择 "Copy only necessary library files"
>    - 选择 "Generate peripheral initialization as pair of '.c/.h' files"
> 5. 点击 "Generate Code"