#ifndef __PRESSURE_PROTOCOL_H
#define __PRESSURE_PROTOCOL_H

#include "stm32f10x.h"

#define PS_STATUS_ADC_DRDY_TIMEOUT   0x00000001u
#define PS_STATUS_ADC_SPI_ERROR      0x00000002u
#define PS_STATUS_PRESSURE_INVALID   0x00000004u
#define PS_STATUS_TEMPERATURE_INVALID 0x00000008u
#define PS_STATUS_PRESSURE_HIGH      0x00000010u
#define PS_STATUS_PRESSURE_LOW       0x00000020u
#define PS_STATUS_TEMPERATURE_STALE  0x00000040u
#define PS_STATUS_SENSOR_FAULT       0x00000080u
#define PS_STATUS_SAFETY_LATCHED     0x00000100u
#define PS_STATUS_WATCHDOG_RESET     0x00000200u
#define PS_STATUS_CONFIG_INVALID     0x00000400u

#define PRESSURE_PROTOCOL_COMMAND_LENGTH 16u

typedef struct
{
    uint32_t command_id;
    char command[PRESSURE_PROTOCOL_COMMAND_LENGTH];
    uint8_t argument_count;
} PressureProtocol_Command;

void PressureProtocol_Init(void);
uint16_t PressureProtocol_Crc16CcittFalse(const uint8_t *data, uint16_t length);
uint8_t PressureProtocol_PollCommand(PressureProtocol_Command *command);

uint8_t PressureProtocol_SendMeasurement(uint32_t sequence,
                                         uint32_t uptime_ms,
                                         uint32_t pressure_raw,
                                         uint32_t temperature_raw,
                                         uint32_t status_flags);
uint8_t PressureProtocol_SendInfo(uint32_t sequence,
                                  uint32_t uptime_ms,
                                  const char *firmware_version,
                                  const char *device_id,
                                  const char *adc_model,
                                  const char *reset_reason,
                                  uint32_t status_flags);
uint8_t PressureProtocol_SendAck(uint32_t command_id,
                                 const char *command,
                                 const char *result,
                                 uint32_t status_flags);
uint8_t PressureProtocol_SendNack(uint32_t command_id,
                                  const char *command,
                                  const char *error_code,
                                  uint32_t status_flags);

#endif
