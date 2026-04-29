#include "logger.h"
#include "main.h"

static LogEvent_t a1_buffer[LOG_A1_BUFFER_SIZE];
static uint16_t a1_head = 0, a1_tail = 0, a1_count = 0;
static bool a1_overflow = false;

static LogEvent_t gen_buffer[LOG_GENERAL_BUFFER_SIZE];
static uint16_t gen_head = 0, gen_tail = 0, gen_count = 0;

void Logger_Init(void) {
    a1_head = a1_tail = a1_count = 0;
    gen_head = gen_tail = gen_count = 0;
    a1_overflow = false;
}

void Logger_Log(LogTier_t tier, uint16_t event_code, uint32_t data) {
    LogEvent_t event;
    event.timestamp_ms = HAL_GetTick();
    event.tier = tier;
    event.event_code = event_code;
    event.data = data;

    if (tier == LOG_TIER_A1) {
        if (a1_count >= LOG_A1_BUFFER_SIZE) {
            a1_overflow = true;
            return;
        }

        a1_buffer[a1_head] = event;
        a1_head = (a1_head + 1) % LOG_A1_BUFFER_SIZE;
        a1_count++;
    } else {
        if (gen_count >= LOG_GENERAL_BUFFER_SIZE) {
            gen_tail = (gen_tail + 1) % LOG_GENERAL_BUFFER_SIZE;
            gen_count--;
        }

        gen_buffer[gen_head] = event;
        gen_head = (gen_head + 1) % LOG_GENERAL_BUFFER_SIZE;
        gen_count++;
    }
}

bool Logger_A1_Overflowed(void) {
    return a1_overflow;
}

uint16_t Logger_Drain(LogEvent_t *out_buf, uint16_t max_count) {
    uint16_t count = 0;

    while (a1_count > 0 && count < max_count) {
        out_buf[count++] = a1_buffer[a1_tail];
        a1_tail = (a1_tail + 1) % LOG_A1_BUFFER_SIZE;
        a1_count--;
    }
    while (gen_count > 0 && count < max_count) {
        out_buf[count++] = gen_buffer[gen_tail];
        gen_tail = (gen_tail + 1) % LOG_GENERAL_BUFFER_SIZE;
        gen_count--;
    }
    return count;
}

static const char* TierToString(LogTier_t tier)
{
    switch (tier) {
        case LOG_TIER_A1: return "A1";
        case LOG_TIER_A2: return "A2";
        case LOG_TIER_A3: return "A3";
        case LOG_TIER_B:  return "B";
        case LOG_TIER_C:  return "C";
        default:          return "?";
    }
}

void Logger_Process(void) {
    // Simulate upload by printing and clearing

    while (a1_count > 0) {
        LogEvent_t e = a1_buffer[a1_tail];
        a1_tail = (a1_tail + 1) % LOG_A1_BUFFER_SIZE;
        a1_count--;

        printf("[A1] %lu | %u | %lu\r\n",
               e.timestamp_ms, e.event_code, e.data);
    }

    while (gen_count > 0) {
        LogEvent_t e = gen_buffer[gen_tail];
        gen_tail = (gen_tail + 1) % LOG_GENERAL_BUFFER_SIZE;
        gen_count--;


        printf("[%s] %lu | %u | %lu\r\n",
               TierToString(e.tier),
               e.timestamp_ms,
               e.event_code,
               e.data);
    }
}
