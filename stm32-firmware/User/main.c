#include "stm32f10x.h"

#include "AD7124.h"
#include "Delay.h"
#include "Key.h"
#include "LCD.h"
#include "PressureProtocol.h"
#include "Serial.h"

#include "Special.h"
#include "font7x10.h"
#include "font_digital.h"

#include <string.h>

#define FIRMWARE_VERSION       "1.0.5"
#define DEVICE_ID              "STM32F103RC"
#define ADC_MODEL              "AD7124-8"
#define TEMPERATURE_CHANNEL_ENABLED 1u
#define ADC_FILTER_LENGTH      10u
#define ADC_READ_TIMEOUT_MS    1000u
#define LCD_REFRESH_PERIOD_MS  500u
#define TEMPERATURE_STALE_MS   2000u
#define ADC_RECOVERY_DELAY_MS  1000u
#define STATUS_REPORT_PERIOD_MS 1000u

typedef enum
{
    DISPLAY_KPA = 1,
    DISPLAY_MBAR,
    DISPLAY_MMHG
} DisplayMode;

static volatile uint32_t g_uptime_ms = 0u;

static uint32_t s_status_flags = 0u;
static uint32_t s_sequence = 0u;
static uint32_t s_pressure_raw = 0u;
static uint32_t s_temperature_raw = 0u;
static uint32_t s_pressure_filtered_raw = 0u;
static uint32_t s_pressure_filter[ADC_FILTER_LENGTH];
static uint8_t s_pressure_filter_index = 0u;
static uint8_t s_pressure_filter_count = 0u;
static uint8_t s_pressure_fresh = 0u;
static uint8_t s_has_pressure = 0u;
#if TEMPERATURE_CHANNEL_ENABLED
static uint8_t s_has_temperature = 0u;
#endif
static uint8_t s_pressure_sensor_fault = 0u;
static uint8_t s_temperature_sensor_fault = 0u;
static uint8_t s_adc_sensor_fault = 0u;
static uint8_t s_consecutive_adc_failures = 0u;
#if TEMPERATURE_CHANNEL_ENABLED
static uint32_t s_last_temperature_ms = 0u;
#endif
static uint32_t s_last_lcd_refresh_ms = 0u;
static uint32_t s_last_adc_recovery_ms = 0u;
static uint32_t s_last_status_report_ms = 0u;
static float s_pressure_kpa = 0.0f;
#if TEMPERATURE_CHANNEL_ENABLED
static float s_temperature_c = 0.0f;
#endif
static DisplayMode s_display_mode = DISPLAY_KPA;
static const char *s_reset_reason = "UNKNOWN";

static void SerialSendUInt32(uint32_t value)
{
    char digits[10];
    uint8_t count = 0u;

    do
    {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while ((value != 0u) && (count < sizeof(digits)));

    while (count != 0u)
        Serial_SendByte((uint8_t)digits[--count]);
}

void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        ++g_uptime_ms;
    }
}

static uint32_t UptimeMs(void)
{
    return g_uptime_ms;
}

static void SystemTime_Init(void)
{
    TIM_TimeBaseInitTypeDef timer;
    NVIC_InitTypeDef nvic;
    RCC_ClocksTypeDef clocks;
    uint32_t timer_clock_hz;
    uint32_t prescaler_divisor;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    RCC_GetClocksFreq(&clocks);
    timer_clock_hz = clocks.PCLK1_Frequency;
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1)
        timer_clock_hz *= 2u;

    /* Feed TIM2 with 1 MHz, then divide by 1000 for a true 1 ms tick. */
    prescaler_divisor = timer_clock_hz / 1000000u;
    if (prescaler_divisor == 0u)
        prescaler_divisor = 1u;
    timer.TIM_Prescaler = (uint16_t)(prescaler_divisor - 1u);
    timer.TIM_Period = 1000u - 1u;
    timer.TIM_ClockDivision = TIM_CKD_DIV1;
    timer.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &timer);
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    nvic.NVIC_IRQChannel = TIM2_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 0u;
    nvic.NVIC_IRQChannelSubPriority = 0u;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
    TIM_Cmd(TIM2, ENABLE);
}

static void Watchdog_Init(void)
{
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    /* About eight seconds at the nominal 40 kHz LSI clock. */
    IWDG_SetPrescaler(IWDG_Prescaler_256);
    IWDG_SetReload(1250u);
    IWDG_ReloadCounter();
    IWDG_Enable();
}

static void DetectResetReason(void)
{
    if (RCC_GetFlagStatus(RCC_FLAG_IWDGRST) != RESET)
    {
        s_reset_reason = "IWDG";
        s_status_flags |= PS_STATUS_WATCHDOG_RESET;
    }
    else if (RCC_GetFlagStatus(RCC_FLAG_WWDGRST) != RESET)
    {
        s_reset_reason = "WWDG";
        s_status_flags |= PS_STATUS_WATCHDOG_RESET;
    }
    else if (RCC_GetFlagStatus(RCC_FLAG_SFTRST) != RESET)
        s_reset_reason = "SOFTWARE";
    else if (RCC_GetFlagStatus(RCC_FLAG_PORRST) != RESET)
        s_reset_reason = "POR";
    else if (RCC_GetFlagStatus(RCC_FLAG_PINRST) != RESET)
        s_reset_reason = "PIN";
    else if (RCC_GetFlagStatus(RCC_FLAG_LPWRRST) != RESET)
        s_reset_reason = "LOW_POWER";

    RCC_ClearFlag();
}

static uint32_t MovingAveragePressure(uint32_t value)
{
    uint64_t sum = 0u;
    uint8_t index;

    s_pressure_filter[s_pressure_filter_index] = value;
    s_pressure_filter_index = (uint8_t)((s_pressure_filter_index + 1u) % ADC_FILTER_LENGTH);
    if (s_pressure_filter_count < ADC_FILTER_LENGTH)
        ++s_pressure_filter_count;

    for (index = 0u; index < s_pressure_filter_count; ++index)
        sum += s_pressure_filter[index];
    return (uint32_t)(sum / s_pressure_filter_count);
}

static float PressureKPaFromRaw(uint32_t raw_code)
{
    float raw = (float)raw_code;
    /* CAL-Q2-UP-20260824-R1; Horner form matches PressureOS exactly. */
    return (2.895347824842157e-13f * raw
            + 2.085601271357902e-4f) * raw
        - 1.023969598250119e2f;
}

#if TEMPERATURE_CHANNEL_ENABLED
static float Pt100TemperatureCFromRaw(uint32_t raw_code)
{
    float raw = (float)raw_code;
    float voltage = ((raw / 8388608.0f) - 1.0f) / 4.0f * 0.66f;
    float resistance = voltage / 200.0f * 1000000.0f;
    return (resistance - 100.0f) / 0.385f;
}
#endif

static void RefreshSensorFaultFlag(void)
{
    if (s_pressure_sensor_fault || s_temperature_sensor_fault || s_adc_sensor_fault)
        s_status_flags |= PS_STATUS_SENSOR_FAULT;
    else
        s_status_flags &= ~PS_STATUS_SENSOR_FAULT;
}

static void ProcessPressureSample(uint32_t raw_code, uint8_t force_invalid)
{
    s_pressure_raw = raw_code;
    s_pressure_fresh = 1u;
    s_status_flags &= ~(PS_STATUS_PRESSURE_INVALID
                        | PS_STATUS_PRESSURE_HIGH
                        | PS_STATUS_PRESSURE_LOW);
    s_pressure_sensor_fault = 0u;

    if (force_invalid || (raw_code <= 1u) || (raw_code >= 0x00FFFFFEu))
    {
        s_status_flags |= PS_STATUS_PRESSURE_INVALID;
        s_pressure_sensor_fault = 1u;
        RefreshSensorFaultFlag();
        return;
    }

    s_pressure_filtered_raw = MovingAveragePressure(raw_code);
    s_pressure_kpa = PressureKPaFromRaw(s_pressure_filtered_raw);
    s_has_pressure = 1u;
    if (s_pressure_kpa > 600.0f)
        s_status_flags |= PS_STATUS_PRESSURE_HIGH;
    else if (s_pressure_kpa < -100.0f)
        s_status_flags |= PS_STATUS_PRESSURE_LOW;
    RefreshSensorFaultFlag();
}

static void ProcessTemperatureSample(uint32_t raw_code, uint8_t force_invalid)
{
#if TEMPERATURE_CHANNEL_ENABLED
    float temperature;

    s_temperature_raw = raw_code;
    s_last_temperature_ms = UptimeMs();
    s_status_flags &= ~(PS_STATUS_TEMPERATURE_INVALID | PS_STATUS_TEMPERATURE_STALE);
    s_temperature_sensor_fault = 0u;

    temperature = Pt100TemperatureCFromRaw(raw_code);
    if (force_invalid || (raw_code <= 1u) || (raw_code >= 0x00FFFFFEu)
        || (temperature < -80.0f) || (temperature > 180.0f))
    {
        s_status_flags |= PS_STATUS_TEMPERATURE_INVALID;
        s_temperature_sensor_fault = 1u;
        RefreshSensorFaultFlag();
        return;
    }

    s_temperature_c = temperature;
    s_has_temperature = 1u;
    RefreshSensorFaultFlag();
#else
    (void)raw_code;
    (void)force_invalid;
    /* A temperature conversion in pressure-only mode means the ADC channel
       enable map no longer matches the verified configuration. */
    s_status_flags |= PS_STATUS_CONFIG_INVALID;
#endif
}

static void UpdateTemperatureFreshness(void)
{
#if TEMPERATURE_CHANNEL_ENABLED
    if (!s_has_temperature || ((uint32_t)(UptimeMs() - s_last_temperature_ms) > TEMPERATURE_STALE_MS))
        s_status_flags |= PS_STATUS_TEMPERATURE_STALE;
    else
        s_status_flags &= ~PS_STATUS_TEMPERATURE_STALE;
#else
    s_temperature_sensor_fault = 0u;
    s_status_flags &= ~(PS_STATUS_TEMPERATURE_INVALID | PS_STATUS_TEMPERATURE_STALE);
    RefreshSensorFaultFlag();
#endif
}

static void SendDeviceInfo(void)
{
    PressureProtocol_SendInfo(s_sequence, UptimeMs(), FIRMWARE_VERSION,
                              DEVICE_ID, ADC_MODEL, s_reset_reason, s_status_flags);
}

static void HandleCommands(void)
{
    PressureProtocol_Command command;

    while (PressureProtocol_PollCommand(&command))
    {
        if ((strcmp(command.command, "PING") == 0) && (command.argument_count == 0u))
            PressureProtocol_SendAck(command.command_id, "PING", "00000000", s_status_flags);
        else if ((strcmp(command.command, "GET_INFO") == 0) && (command.argument_count == 0u))
        {
            PressureProtocol_SendAck(command.command_id, "GET_INFO", "INFO", s_status_flags);
            SendDeviceInfo();
        }
        else if (strcmp(command.command, "ZERO") == 0)
        {
            /* ZERO stays disabled until the physical reference and safety rules are frozen. */
            PressureProtocol_SendNack(command.command_id, "ZERO", "E_NOT_SAFE", s_status_flags);
        }
        else
            PressureProtocol_SendNack(command.command_id, command.command,
                                      "E_UNKNOWN_CMD", s_status_flags);
    }
}

static void UpdateDisplayMode(void)
{
    uint8_t key = read_key();
    if (key == 0x01u)
        s_display_mode = DISPLAY_KPA;
    else if (key == 0x02u)
        s_display_mode = DISPLAY_MBAR;
    else if (key == 0x04u)
        s_display_mode = DISPLAY_MMHG;
}

static void RefreshLcdIfDue(void)
{
    uint32_t now = UptimeMs();
    int32_t display_value;

    if ((uint32_t)(now - s_last_lcd_refresh_ms) < LCD_REFRESH_PERIOD_MS)
        return;
    s_last_lcd_refresh_ms = now;

    LCD_PixelMode = LCD_PSET;
    /* The panel is 400 x 240; never write beyond row 239. */
    LCD_FillRect(10, 40, 380, 230);
    LCD_PixelMode = LCD_PRES;

    if (!s_has_pressure)
    {
        LCD_PutStr(55, 65, "Waiting for ADC data", &Special);
        if ((s_status_flags & PS_STATUS_CONFIG_INVALID) != 0u)
            LCD_PutStr(70, 115, "ADC CONFIG ERROR", &Special);
        else if ((s_status_flags & PS_STATUS_ADC_DRDY_TIMEOUT) != 0u)
            LCD_PutStr(65, 115, "ADC DRDY TIMEOUT", &Special);
        else if ((s_status_flags & PS_STATUS_ADC_SPI_ERROR) != 0u)
            LCD_PutStr(80, 115, "ADC SPI ERROR", &Special);
        else if ((s_status_flags & PS_STATUS_PRESSURE_INVALID) != 0u)
            LCD_PutStr(65, 115, "PRESSURE INVALID", &Special);
        else
            LCD_PutStr(90, 115, "INITIALIZING", &Special);
        LCD_PutStr(35, 175, "Check sensor and SPI", &Special);
        SMLCD_Flush();
        return;
    }

    if (s_display_mode == DISPLAY_KPA)
    {
        display_value = (int32_t)(s_pressure_kpa * 100.0f);
        LCD_PutIntF(100, 60, display_value, 2, &Font7x10);
        LCD_PutStr(250, 60, "kPa", &Font7x10);
        LCD_PutStr(10, 130, "ADC:", &Special);
        LCD_PutInt(80, 130, (int32_t)s_pressure_filtered_raw, &Special);
        LCD_PutStr(10, 170, "Ambient:", &Special);
        if (s_has_temperature)
        {
            display_value = (int32_t)(s_temperature_c * 10.0f);
            LCD_PutIntF(150, 170, display_value, 1, &Special);
            LCD_PutStr(225, 170, "C", &Special);
        }
    }
    else if (s_display_mode == DISPLAY_MBAR)
    {
        display_value = (int32_t)(s_pressure_kpa * 100.0f);
        LCD_PutIntF(100, 60, display_value, 1, &Font7x10);
        LCD_PutStr(250, 60, "mbar", &Font7x10);
    }
    else
    {
        display_value = (int32_t)(s_pressure_kpa * 750.06168f);
        LCD_PutIntF(70, 60, display_value, 2, &Font7x10);
        LCD_PutStr(250, 60, "mmHg", &Font7x10);
    }

    if ((s_status_flags & (PS_STATUS_ADC_DRDY_TIMEOUT
                           | PS_STATUS_ADC_SPI_ERROR
                           | PS_STATUS_CONFIG_INVALID
                           | PS_STATUS_SENSOR_FAULT)) != 0u)
        LCD_PutStr(10, 215, "ADC WARNING", &Special);
    SMLCD_Flush();
}

static void InitializeLcd(void)
{
    Key_Init();
    LCD_PixelMode = LCD_PRES;
    LCD_GPIO_Init();
    LCD_SPI2_Init();
    Delay_ms(50);
    LCD_Clear();
    LCD_EXTCOMIN_PWM_Init();
    Delay_ms(50);
    LCD_Clear();
    Delay_ms(50);
    LCD_vRAM_Clear();
    LCD_PutStr(90, 0, "PressureOS 1.0.5", &Special);
    LCD_PutStr(75, 75, "LCD / UART READY", &Special);
    LCD_PutStr(60, 125, "System clock:", &Special);
    LCD_PutInt(205, 125, (int32_t)SystemCoreClock, &Special);
    LCD_PutStr(320, 125, "Hz", &Special);
    SMLCD_Flush();
}

static void ShowBootStage(const char *stage)
{
    LCD_PixelMode = LCD_PSET;
    LCD_FillRect(10, 165, 380, 225);
    LCD_PixelMode = LCD_PRES;
    LCD_PutStr(55, 180, stage, &Special);
    SMLCD_Flush();
}

static void ReportStatusIfDue(void)
{
    uint32_t now = UptimeMs();

    if (s_has_pressure
        || ((uint32_t)(now - s_last_status_report_ms) < STATUS_REPORT_PERIOD_MS))
        return;

    s_last_status_report_ms = now;
    SendDeviceInfo();
}

static void TryRecoverAdc(void)
{
    uint32_t now = UptimeMs();

    if ((s_consecutive_adc_failures < 3u)
        || ((uint32_t)(now - s_last_adc_recovery_ms) < ADC_RECOVERY_DELAY_MS))
        return;

    s_last_adc_recovery_ms = now;
    IWDG_ReloadCounter();
    if (AD7124_8_SoftReset() && AD7124_8_Init())
    {
        s_consecutive_adc_failures = 0u;
        s_status_flags &= ~(PS_STATUS_ADC_DRDY_TIMEOUT
                            | PS_STATUS_ADC_SPI_ERROR
                            | PS_STATUS_CONFIG_INVALID);
    }
    else
        s_status_flags |= PS_STATUS_CONFIG_INVALID;
}

int main(void)
{
    AD7124_Sample sample;
    AD7124_Result result;

    /* Detect an HSE failure/fallback before using any clock-based delay. */
    SystemCoreClockUpdate();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    DetectResetReason();
    SystemTime_Init();
    Serial_Init();
    Serial_SendString("\r\nPRESSUREOS_BOOT,1.0.5\r\n");
    PressureProtocol_Init();
    Serial_SendString("BOOT_STAGE,LCD_BEGIN\r\n");
    InitializeLcd();
    Serial_SendString("BOOT_STAGE,LCD_OK\r\n");
    Serial_SendString("CLOCK_HZ,");
    SerialSendUInt32(SystemCoreClock);
    Serial_SendString("\r\n");

    Serial_SendString("BOOT_STAGE,ADC_BEGIN\r\n");
    ShowBootStage("ADC INITIALIZING");
    AD7124_SPI_Config();
    if (!AD7124_8_SoftReset() || !AD7124_8_Init())
    {
        s_status_flags |= PS_STATUS_CONFIG_INVALID;
        Serial_SendString("ADC_INIT,ERROR\r\n");
        ShowBootStage("ADC CONFIG ERROR");
    }
    else
    {
        Serial_SendString("ADC_INIT,OK\r\n");
        ShowBootStage("ADC READY");
    }

    Watchdog_Init();
    UpdateTemperatureFreshness();
    s_last_lcd_refresh_ms = UptimeMs() - LCD_REFRESH_PERIOD_MS;
    s_last_status_report_ms = UptimeMs() - STATUS_REPORT_PERIOD_MS;
    RefreshLcdIfDue();
    SendDeviceInfo();

    while (1)
    {
        IWDG_ReloadCounter();
        HandleCommands();
        UpdateDisplayMode();
        UpdateTemperatureFreshness();
        RefreshLcdIfDue();
        ReportStatusIfDue();

        result = AD7124_8_ReadSample(&sample, ADC_READ_TIMEOUT_MS);
        if (result == AD7124_RESULT_DRDY_TIMEOUT)
        {
            s_status_flags |= PS_STATUS_ADC_DRDY_TIMEOUT;
            if (s_consecutive_adc_failures < 255u)
                ++s_consecutive_adc_failures;
            TryRecoverAdc();
            RefreshLcdIfDue();
            ReportStatusIfDue();
            continue;
        }
        if (result == AD7124_RESULT_SPI_ERROR)
        {
            s_status_flags |= PS_STATUS_ADC_SPI_ERROR;
            if (s_consecutive_adc_failures < 255u)
                ++s_consecutive_adc_failures;
            TryRecoverAdc();
            RefreshLcdIfDue();
            ReportStatusIfDue();
            continue;
        }

        s_consecutive_adc_failures = 0u;
        s_status_flags &= ~(PS_STATUS_ADC_DRDY_TIMEOUT | PS_STATUS_ADC_SPI_ERROR);
        s_adc_sensor_fault = (result == AD7124_RESULT_ADC_ERROR) ? 1u : 0u;
        RefreshSensorFaultFlag();

        if (sample.channel == AD7124_CHANNEL_PRESSURE)
            ProcessPressureSample(sample.raw, result == AD7124_RESULT_ADC_ERROR);
        else if (sample.channel == AD7124_CHANNEL_TEMPERATURE)
            ProcessTemperatureSample(sample.raw, result == AD7124_RESULT_ADC_ERROR);
        else
            s_status_flags |= PS_STATUS_CONFIG_INVALID;

        UpdateTemperatureFreshness();
        /*
         * Pressure is the primary measurement.  Do not suppress every frame
         * merely because the temperature channel is missing or stale; its
         * validity remains explicit in status_flags.
         */
        if (s_pressure_fresh)
        {
            ++s_sequence;
            PressureProtocol_SendMeasurement(s_sequence, UptimeMs(),
                                             s_pressure_raw, s_temperature_raw,
                                             s_status_flags);
            s_pressure_fresh = 0u;
        }

        HandleCommands();
        RefreshLcdIfDue();
    }
}
