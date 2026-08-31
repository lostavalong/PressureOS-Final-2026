#include "PressureProtocol.h"
#include "Serial.h"

#include <stdlib.h>
#include <string.h>

#define PROTOCOL_MAX_FRAME_LENGTH   128u
#define PROTOCOL_PAYLOAD_LENGTH     112u
#define PROTOCOL_MAX_FIELDS         8u

static char s_receive_frame[PROTOCOL_MAX_FRAME_LENGTH + 1u];
static uint16_t s_receive_length = 0u;
static uint8_t s_collecting = 0u;

typedef struct
{
    char data[PROTOCOL_PAYLOAD_LENGTH];
    uint16_t length;
    uint8_t valid;
} PayloadBuilder;

static char HexDigit(uint8_t value)
{
    value &= 0x0Fu;
    return (value < 10u) ? (char)('0' + value) : (char)('A' + value - 10u);
}

static void BuilderInit(PayloadBuilder *builder)
{
    builder->length = 0u;
    builder->valid = 1u;
}

static void BuilderAppendChar(PayloadBuilder *builder, char value)
{
    if (!builder->valid)
        return;
    if (builder->length >= (PROTOCOL_PAYLOAD_LENGTH - 1u))
    {
        builder->valid = 0u;
        return;
    }
    builder->data[builder->length++] = value;
}

static void BuilderAppendText(PayloadBuilder *builder, const char *text)
{
    if (text == 0)
    {
        builder->valid = 0u;
        return;
    }
    while (*text != '\0')
        BuilderAppendChar(builder, *text++);
}

static void BuilderAppendUInt32(PayloadBuilder *builder, uint32_t value)
{
    char digits[10];
    uint8_t count = 0u;

    do
    {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while ((value != 0u) && (count < sizeof(digits)));

    while (count > 0u)
        BuilderAppendChar(builder, digits[--count]);
}

static void BuilderAppendHex32(PayloadBuilder *builder, uint32_t value)
{
    int8_t shift;
    for (shift = 28; shift >= 0; shift -= 4)
        BuilderAppendChar(builder, HexDigit((uint8_t)(value >> shift)));
}

static uint8_t SendBuiltPayload(const PayloadBuilder *builder)
{
    uint16_t crc;
    int8_t shift;

    if ((builder == 0) || !builder->valid || (builder->length == 0u))
        return 0u;

    crc = PressureProtocol_Crc16CcittFalse((const uint8_t *)builder->data,
                                           builder->length);
    Serial_SendByte('@');
    Serial_SendArray((const uint8_t *)builder->data, builder->length);
    Serial_SendByte('*');
    for (shift = 12; shift >= 0; shift -= 4)
        Serial_SendByte((uint8_t)HexDigit((uint8_t)(crc >> shift)));
    Serial_SendString("\r\n");
    return 1u;
}

static int HexValue(char value)
{
    if ((value >= '0') && (value <= '9'))
        return value - '0';
    if ((value >= 'A') && (value <= 'F'))
        return value - 'A' + 10;
    if ((value >= 'a') && (value <= 'f'))
        return value - 'a' + 10;
    return -1;
}

static uint8_t ParseHex16(const char *text, uint16_t *value)
{
    uint8_t index;
    uint16_t result = 0u;

    for (index = 0u; index < 4u; ++index)
    {
        int digit = HexValue(text[index]);
        if (digit < 0)
            return 0u;
        result = (uint16_t)((result << 4) | (uint16_t)digit);
    }
    *value = result;
    return 1u;
}

static uint8_t ParseCommandFrame(PressureProtocol_Command *command)
{
    char *star;
    char *fields[PROTOCOL_MAX_FIELDS];
    char *cursor;
    char *end;
    uint8_t field_count = 0u;
    uint8_t index;
    uint16_t received_crc;
    uint16_t calculated_crc;
    unsigned long command_id;

    if ((command == 0) || (s_receive_length < 12u) || (s_receive_frame[0] != '@'))
        return 0u;

    s_receive_frame[s_receive_length] = '\0';
    star = strrchr(s_receive_frame, '*');
    if ((star == 0) || (strlen(star + 1) != 4u)
        || !ParseHex16(star + 1, &received_crc))
        return 0u;

    calculated_crc = PressureProtocol_Crc16CcittFalse(
        (const uint8_t *)(s_receive_frame + 1),
        (uint16_t)(star - (s_receive_frame + 1)));
    if (calculated_crc != received_crc)
        return 0u;

    *star = '\0';
    cursor = s_receive_frame + 1;
    fields[field_count++] = cursor;
    while ((*cursor != '\0') && (field_count < PROTOCOL_MAX_FIELDS))
    {
        if (*cursor == ',')
        {
            *cursor = '\0';
            fields[field_count++] = cursor + 1;
        }
        ++cursor;
    }

    if ((field_count < 4u) || (strcmp(fields[0], "PS1") != 0)
        || (strcmp(fields[1], "C") != 0))
        return 0u;

    command_id = strtoul(fields[2], &end, 10);
    if ((fields[2][0] == '\0') || (*end != '\0'))
        return 0u;

    command->command_id = (uint32_t)command_id;
    command->argument_count = (uint8_t)(field_count - 4u);
    for (index = 0u; index < PRESSURE_PROTOCOL_COMMAND_LENGTH - 1u; ++index)
    {
        char value = fields[3][index];
        if (value == '\0')
            break;
        if ((value >= 'a') && (value <= 'z'))
            value = (char)(value - ('a' - 'A'));
        command->command[index] = value;
    }
    command->command[index] = '\0';
    return command->command[0] != '\0';
}

void PressureProtocol_Init(void)
{
    s_receive_length = 0u;
    s_collecting = 0u;
}

uint16_t PressureProtocol_Crc16CcittFalse(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFFu;
    uint16_t index;
    uint8_t bit;

    for (index = 0u; index < length; ++index)
    {
        crc ^= (uint16_t)data[index] << 8;
        for (bit = 0u; bit < 8u; ++bit)
        {
            if ((crc & 0x8000u) != 0u)
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            else
                crc <<= 1;
        }
    }
    return crc;
}

uint8_t PressureProtocol_PollCommand(PressureProtocol_Command *command)
{
    uint8_t value;

    if (command == 0)
        return 0u;

    if (Serial_TakeRxOverflow() || Serial_TakeRxError())
    {
        s_receive_length = 0u;
        s_collecting = 0u;
    }

    while (Serial_ReceiveByte_NonBlocking(&value))
    {
        if (value == '@')
        {
            s_collecting = 1u;
            s_receive_length = 0u;
            s_receive_frame[s_receive_length++] = '@';
            continue;
        }
        if (!s_collecting)
            continue;
        if (value == '\r')
            continue;
        if (value == '\n')
        {
            uint8_t parsed = ParseCommandFrame(command);
            s_receive_length = 0u;
            s_collecting = 0u;
            if (parsed)
                return 1u;
            continue;
        }
        if (s_receive_length >= PROTOCOL_MAX_FRAME_LENGTH)
        {
            s_receive_length = 0u;
            s_collecting = 0u;
            continue;
        }
        s_receive_frame[s_receive_length++] = (char)value;
    }
    return 0u;
}

uint8_t PressureProtocol_SendMeasurement(uint32_t sequence,
                                         uint32_t uptime_ms,
                                         uint32_t pressure_raw,
                                         uint32_t temperature_raw,
                                         uint32_t status_flags)
{
    PayloadBuilder builder;

    BuilderInit(&builder);
    BuilderAppendText(&builder, "PS1,M,");
    BuilderAppendUInt32(&builder, sequence);
    BuilderAppendChar(&builder, ',');
    BuilderAppendUInt32(&builder, uptime_ms);
    BuilderAppendChar(&builder, ',');
    BuilderAppendUInt32(&builder, pressure_raw);
    BuilderAppendChar(&builder, ',');
    BuilderAppendUInt32(&builder, temperature_raw);
    BuilderAppendChar(&builder, ',');
    BuilderAppendHex32(&builder, status_flags);
    return SendBuiltPayload(&builder);
}

uint8_t PressureProtocol_SendInfo(uint32_t sequence,
                                  uint32_t uptime_ms,
                                  const char *firmware_version,
                                  const char *device_id,
                                  const char *adc_model,
                                  const char *reset_reason,
                                  uint32_t status_flags)
{
    PayloadBuilder builder;

    BuilderInit(&builder);
    BuilderAppendText(&builder, "PS1,I,");
    BuilderAppendUInt32(&builder, sequence);
    BuilderAppendChar(&builder, ',');
    BuilderAppendUInt32(&builder, uptime_ms);
    BuilderAppendChar(&builder, ',');
    BuilderAppendText(&builder, firmware_version);
    BuilderAppendChar(&builder, ',');
    BuilderAppendText(&builder, device_id);
    BuilderAppendChar(&builder, ',');
    BuilderAppendText(&builder, adc_model);
    BuilderAppendChar(&builder, ',');
    BuilderAppendText(&builder, reset_reason);
    BuilderAppendChar(&builder, ',');
    BuilderAppendHex32(&builder, status_flags);
    return SendBuiltPayload(&builder);
}

uint8_t PressureProtocol_SendAck(uint32_t command_id,
                                 const char *command,
                                 const char *result,
                                 uint32_t status_flags)
{
    PayloadBuilder builder;

    BuilderInit(&builder);
    BuilderAppendText(&builder, "PS1,A,");
    BuilderAppendUInt32(&builder, command_id);
    BuilderAppendChar(&builder, ',');
    BuilderAppendText(&builder, command);
    BuilderAppendText(&builder, ",OK,");
    BuilderAppendText(&builder, result);
    BuilderAppendChar(&builder, ',');
    BuilderAppendHex32(&builder, status_flags);
    return SendBuiltPayload(&builder);
}

uint8_t PressureProtocol_SendNack(uint32_t command_id,
                                  const char *command,
                                  const char *error_code,
                                  uint32_t status_flags)
{
    PayloadBuilder builder;

    BuilderInit(&builder);
    BuilderAppendText(&builder, "PS1,N,");
    BuilderAppendUInt32(&builder, command_id);
    BuilderAppendChar(&builder, ',');
    BuilderAppendText(&builder, command);
    BuilderAppendChar(&builder, ',');
    BuilderAppendText(&builder, error_code);
    BuilderAppendChar(&builder, ',');
    BuilderAppendHex32(&builder, status_flags);
    return SendBuiltPayload(&builder);
}
