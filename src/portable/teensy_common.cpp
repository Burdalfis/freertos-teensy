/*
 * This file is part of the FreeRTOS port to Teensy boards.
 * Copyright (c) 2020 Timo Sandmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file    teensy_common.cpp
 * @brief   FreeRTOS support implementations for Teensy boards with newlib 3
 * @author  Timo Sandmann
 * @date    10.10.2020
 */

#define _DEFAULT_SOURCE
#include <cstring>
#include <unistd.h>
#include <malloc.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/lock.h>
#include <unwind.h>
#include <tuple>

#include "avr/pgmspace.h"
#include "teensy.h"
#include "event_responder_support.h"

#if defined(__has_include) && __has_include("freertos_time.h")
#include "freertos_time.h"
#endif
#include "semphr.h"


static constexpr bool DEBUG { false };

using namespace arduino;

extern "C" {
asm(".global _printf_float"); /**< printf supporting floating point values */

extern unsigned long _heap_end;
extern unsigned long _estack;
extern unsigned long _ebss;

extern volatile uint32_t systick_millis_count;
extern volatile uint32_t systick_cycle_count;
extern uint32_t set_arm_clock(uint32_t frequency);
uint8_t* _g_current_heap_end { reinterpret_cast<uint8_t*>(&_ebss) };


uint8_t get_debug_led_pin() __attribute__((weak)) FLASHMEM;
uint8_t get_debug_led_pin() {
    return LED_BUILTIN;
}

FLASHMEM void serial_puts(const char* str) {
    ::Serial.println(str);
    ::Serial.flush();
}

FLASHMEM _Unwind_Reason_Code trace_fcn(_Unwind_Context* ctx, void* depth) {
    int* p_depth { static_cast<int*>(depth) };
    ::Serial.print(PSTR("\t#"));
    ::Serial.print(*p_depth);
    ::Serial.print(PSTR(": pc at 0x"));
    ::Serial.println(_Unwind_GetIP(ctx), 16);
    (*p_depth)++;
    return _URC_NO_REASON;
}

/**
 * @brief Print assert message and blink one short pulse every two seconds
 * @param[in] file: Filename as C-string
 * @param[in] line: Line number
 * @param[in] func: Function name as C-string
 * @param[in] expr: Expression that failed as C-string
 */
FLASHMEM void assert_blink(const char* file, int line, const char* func, const char* expr) {
    ::Serial.println();
    ::Serial.print(PSTR("ASSERT in ["));
    ::Serial.print(file);
    ::Serial.print(':');
    ::Serial.print(line, 10);
    ::Serial.println(PSTR("]"));
    ::Serial.print('\t');
    ::Serial.print(func);
    ::Serial.print(PSTR("(): "));
    ::Serial.println(expr);
    ::Serial.println();

    ::Serial.println(PSTR("Stack trace:"));
    int depth { 0 };
    _Unwind_Backtrace(&trace_fcn, &depth);
    ::Serial.flush();

    freertos::error_blink(1);
}

FLASHMEM void mcu_shutdown() {
    freertos::error_blink(0);
}
} // extern C


namespace freertos {
FLASHMEM void error_blink(const uint8_t n) {
    ::vTaskSuspendAll();
    const uint8_t debug_led_pin { get_debug_led_pin() };
    ::pinMode(debug_led_pin, OUTPUT);
    ::set_arm_clock(16'000'000UL);

    while (true) {
        for (uint8_t i {}; i < n; ++i) {
            ::digitalWriteFast(debug_led_pin, true);
            delay_ms(300UL);
            ::digitalWriteFast(debug_led_pin, false);
            delay_ms(300UL);
        }
        delay_ms(2'000UL);
    }
}

FLASHMEM void print_ram_usage() {
    const auto info1 { ram1_usage() };
    const auto info2 { ram2_usage() };

    ::Serial.print(PSTR("RAM1 size: "));
    ::Serial.print(std::get<5>(info1) / 1'024UL, 10);
    ::Serial.print(PSTR(" KB, free RAM1: "));
    ::Serial.print(std::get<0>(info1) / 1'024UL, 10);
    ::Serial.print(PSTR(" KB, data used: "));
    ::Serial.print((std::get<1>(info1)) / 1'024UL, 10);
    ::Serial.print(PSTR(" KB, bss used: "));
    ::Serial.print(std::get<2>(info1) / 1'024UL, 10);
    ::Serial.print(PSTR(" KB, used heap: "));
    ::Serial.print(std::get<3>(info1) / 1'024UL, 10);
    ::Serial.print(PSTR(" KB, system free: "));
    ::Serial.print(std::get<4>(info1) / 1'024UL, 10);
    ::Serial.println(PSTR(" KB"));

    ::Serial.print(PSTR("RAM2 size: "));
    ::Serial.print(std::get<1>(info2) / 1'024UL, 10);
    ::Serial.print(PSTR(" KB, free RAM2: "));
    ::Serial.print(std::get<0>(info2) / 1'024UL, 10);
    ::Serial.print(PSTR(" KB, used RAM2: "));
    ::Serial.print((std::get<1>(info2) - std::get<0>(info2)) / 1'024UL, 10);

    ::Serial.println(PSTR(" KB"));
}
} // namespace freertos

extern "C" {
void startup_late_hook() FLASHMEM;
void startup_late_hook() {
    if (DEBUG) {
        printf_debug(PSTR("startup_late_hook()\n"));
    }
    ::vTaskSuspendAll();
    if (DEBUG) {
        printf_debug(PSTR("startup_late_hook() done.\n"));
    }
}

void setup_systick_with_timer_events() {}

void event_responder_set_pend_sv() {
    if (freertos::g_event_responder_task) {
        ::xTaskNotifyIndexed(freertos::g_event_responder_task, 1, 0, eNoAction);
    }
}

FLASHMEM void yield() {
    if (freertos::g_yield_task) {
        ::xTaskNotifyIndexed(freertos::g_yield_task, 1, 0, eNoAction);
    } else {
        freertos::yield();
    }
}

#if configUSE_IDLE_HOOK == 1
void vApplicationIdleHook() {}
#endif // configUSE_IDLE_HOOK

void vApplicationStackOverflowHook(TaskHandle_t, char*) FLASHMEM;

void vApplicationStackOverflowHook(TaskHandle_t, char* task_name) {
    static char taskname[configMAX_TASK_NAME_LEN + 1];

    std::memcpy(taskname, task_name, configMAX_TASK_NAME_LEN);
    ::serial_puts(PSTR("STACK OVERFLOW: "));
    ::serial_puts(taskname);

    freertos::error_blink(3);
}

#ifdef PLATFORMIO
#if configUSE_MALLOC_FAILED_HOOK == 1
static FLASHMEM void vApplicationMallocFailedHook() {
    freertos::error_blink(2);
}
#endif // configUSE_MALLOC_FAILED_HOOK

// FIXME: needs update of teensy cores library
void* _sbrk(ptrdiff_t incr) { // FIXME: flashmem?
    static_assert(portSTACK_GROWTH == -1, "Stack growth down assumed");

    if (DEBUG) {
        printf_debug(PSTR("_sbrk(%u)\r\n"), incr);
        printf_debug(PSTR("current_heap_end=0x%x\r\n"), reinterpret_cast<uintptr_t>(_g_current_heap_end));
        printf_debug(PSTR("_ebss=0x%x\r\n"), reinterpret_cast<uintptr_t>(&_ebss));
        printf_debug(PSTR("_estack=0x%x\r\n"), reinterpret_cast<uintptr_t>(&_estack));
    }

    void* previous_heap_end { _g_current_heap_end };

    if ((reinterpret_cast<uintptr_t>(_g_current_heap_end) + incr >= reinterpret_cast<uintptr_t>(&_estack) - 8'192U)
        || (reinterpret_cast<uintptr_t>(_g_current_heap_end) + incr < reinterpret_cast<uintptr_t>(&_ebss))) {
        printf_debug(PSTR("_sbrk(%u): no mem available.\r\n"), incr);
#if configUSE_MALLOC_FAILED_HOOK == 1
        ::vApplicationMallocFailedHook();
#else
        _impure_ptr->_errno = ENOMEM;
#endif
        return reinterpret_cast<void*>(-1); // the malloc-family routine that called sbrk will return 0
    }

    _g_current_heap_end += incr;
    return previous_heap_end;
}
#endif // ! PLATFORMIO

static UBaseType_t int_nesting {};
static UBaseType_t int_prio {};

void __malloc_lock(struct _reent*) {
    configASSERT(!xPortIsInsideInterrupt()); // no mallocs inside ISRs
    int_prio = ::ulPortRaiseBASEPRI();
    ++int_nesting;
};

void __malloc_unlock(struct _reent*) {
    --int_nesting;
    if (!int_nesting) {
        ::vPortSetBASEPRI(int_prio);
    }
};

// newlib also requires implementing locks for the application's environment memory space,
// accessed by newlib's setenv() and getenv() functions.
// As these are trivial functions, momentarily suspend task switching (rather than semaphore).
void __env_lock() { // FIXME: check
    ::vTaskSuspendAll();
};

void __env_unlock() { // FIXME: check
    ::xTaskResumeAll();
};

int _getpid() {
    return reinterpret_cast<int>(::xTaskGetCurrentTaskHandle());
}

FLASHMEM int _gettimeofday(timeval* tv, void*) {
    const auto now_us { freertos::get_us() };
#if defined(__has_include) && __has_include("freertos_time.h")
    const auto off { free_rtos_std::wall_clock::get_offset() };
#else
    const timeval off { 0, 0 };
#endif
    const timeval now { static_cast<time_t>(now_us / 1'000'000UL), static_cast<suseconds_t>(now_us % 1'000'000UL) };

    timeradd(&off, &now, tv);
    return 0;
}

FLASHMEM size_t xPortGetFreeHeapSize() {
    const struct mallinfo mi = ::mallinfo();
    return mi.fordblks;
}

#if configGENERATE_RUN_TIME_STATS == 1
uint64_t freertos_get_us() {
    return freertos::get_us();
}
#endif // configGENERATE_RUN_TIME_STATS

#if configSUPPORT_STATIC_ALLOCATION == 1
FLASHMEM void vApplicationGetIdleTaskMemory(StaticTask_t** ppxIdleTaskTCBBuffer, StackType_t** ppxIdleTaskStackBuffer, uint32_t* pulIdleTaskStackSize) {
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE] __attribute__((used, aligned(8)));

    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

#if configUSE_TIMERS == 1
FLASHMEM void vApplicationGetTimerTaskMemory(StaticTask_t** ppxTimerTaskTCBBuffer, StackType_t** ppxTimerTaskStackBuffer, uint32_t* pulTimerTaskStackSize) {
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH] __attribute__((used, aligned(8)));

    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
#endif // configUSE_TIMERS


StaticSemaphore_t __lock___sinit_recursive_mutex;
StaticSemaphore_t __lock___sfp_recursive_mutex;
StaticSemaphore_t __lock___atexit_recursive_mutex;
StaticSemaphore_t __lock___at_quick_exit_mutex;
StaticSemaphore_t __lock___env_recursive_mutex;
StaticSemaphore_t __lock___tz_mutex;
StaticSemaphore_t __lock___dd_hash_mutex;
StaticSemaphore_t __lock___arc4random_mutex;
static bool __locks_initialized {};

FLASHMEM void init_retarget_locks() {
    ::xSemaphoreCreateRecursiveMutexStatic(&__lock___sinit_recursive_mutex);
    ::xSemaphoreCreateRecursiveMutexStatic(&__lock___sfp_recursive_mutex);
    ::xSemaphoreCreateRecursiveMutexStatic(&__lock___atexit_recursive_mutex);
    ::xSemaphoreCreateMutexStatic(&__lock___at_quick_exit_mutex);
    ::xSemaphoreCreateRecursiveMutexStatic(&__lock___env_recursive_mutex);
    ::xSemaphoreCreateMutexStatic(&__lock___tz_mutex);
    ::xSemaphoreCreateMutexStatic(&__lock___dd_hash_mutex);
    ::xSemaphoreCreateMutexStatic(&__lock___arc4random_mutex);
    __locks_initialized = true;
}
#else // configSUPPORT_STATIC_ALLOCATION == 0
#warning "untested!"
SemaphoreHandle_t __lock___sinit_recursive_mutex;
SemaphoreHandle_t __lock___sfp_recursive_mutex;
SemaphoreHandle_t __lock___atexit_recursive_mutex;
SemaphoreHandle_t __lock___at_quick_exit_mutex;
SemaphoreHandle_t __lock___env_recursive_mutex;
SemaphoreHandle_t __lock___tz_mutex;
SemaphoreHandle_t __lock___dd_hash_mutex;
SemaphoreHandle_t __lock___arc4random_mutex;

FLASHMEM void init_retarget_locks() {
    __lock___sinit_recursive_mutex = ::xSemaphoreCreateRecursiveMutex();
    __lock___sfp_recursive_mutex = ::xSemaphoreCreateRecursiveMutex();
    __lock___atexit_recursive_mutex = ::xSemaphoreCreateRecursiveMutex();
    __lock___at_quick_exit_mutex = ::xSemaphoreCreateMutex();
    __lock___env_recursive_mutex = ::xSemaphoreCreateRecursiveMutex();
    __lock___tz_mutex = ::xSemaphoreCreateMutex();
    __lock___dd_hash_mutex = ::xSemaphoreCreateMutex();
    __lock___arc4random_mutex = ::xSemaphoreCreateMutex();
    __locks_initialized = true;
}
#endif // configSUPPORT_STATIC_ALLOCATION

void __retarget_lock_init(_LOCK_T* lock_ptr) {
    auto ptr { reinterpret_cast<QueueHandle_t*>(lock_ptr) };
    *ptr = ::xSemaphoreCreateMutex();
}

void __retarget_lock_init_recursive(_LOCK_T* lock_ptr) {
    auto ptr { reinterpret_cast<QueueHandle_t*>(lock_ptr) };
    *ptr = ::xSemaphoreCreateRecursiveMutex();
}

void __retarget_lock_close(_LOCK_T lock) {
    if (__locks_initialized) {
        ::vSemaphoreDelete(reinterpret_cast<QueueHandle_t>(lock));
    }
}

void __retarget_lock_close_recursive(_LOCK_T lock) {
    if (__locks_initialized) {
        ::vSemaphoreDelete(reinterpret_cast<QueueHandle_t>(lock));
    }
}

void __retarget_lock_acquire(_LOCK_T lock) {
    if (__locks_initialized) {
        ::xSemaphoreTake(reinterpret_cast<QueueHandle_t>(lock), portMAX_DELAY);
    }
}

void __retarget_lock_acquire_recursive(_LOCK_T lock) {
    if (__locks_initialized) {
        ::xSemaphoreTakeRecursive(reinterpret_cast<QueueHandle_t>(lock), portMAX_DELAY);
    }
}

int __retarget_lock_try_acquire(_LOCK_T lock) {
    if (__locks_initialized) {
        return ::xSemaphoreTake(reinterpret_cast<QueueHandle_t>(lock), 0);
    }
    return 0;
}

int __retarget_lock_try_acquire_recursive(_LOCK_T lock) {
    if (__locks_initialized) {
        return ::xSemaphoreTakeRecursive(reinterpret_cast<QueueHandle_t>(lock), 0);
    }
    return 0;
}

void __retarget_lock_release(_LOCK_T lock) {
    if (__locks_initialized) {
        ::xSemaphoreGive(reinterpret_cast<QueueHandle_t>(lock));
    }
}

void __retarget_lock_release_recursive(_LOCK_T lock) {
    if (__locks_initialized) {
        ::xSemaphoreGiveRecursive(reinterpret_cast<QueueHandle_t>(lock));
    }
}
} // extern C
