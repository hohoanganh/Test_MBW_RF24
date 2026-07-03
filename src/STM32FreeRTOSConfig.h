/*
 * STM32FreeRTOSConfig.h - Cau hinh FreeRTOS RIENG cho MBW RF24 RS485 2.0
 * (STM32L151C8T6, 10KB RAM). File nay THAY THE HOAN TOAN
 * FreeRTOSConfig_Default.h cua thu vien STM32duino FreeRTOS (co che chinh
 * thong: FreeRTOSConfig.h cua thu vien uu tien __has_include
 * "STM32FreeRTOSConfig.h" truoc file default).
 *
 * 2026-07-03 LY DO PHAI TAO FILE NAY: FreeRTOSConfig_Default.h dinh nghia
 * configUSE_TIMERS=1 (va nhieu macro khac) KHONG co guard #ifndef, nen moi
 * flag "-D configUSE_TIMERS=0" trong platformio.ini deu bi thu vien GHI DE
 * nguoc lai -> timers.c van tao Timer task -> undefined reference
 * vApplicationGetTimerTaskMemory + ton ~800-1000 byte RAM vo ich.
 * LUU Y: build_flags "-I src" trong platformio.ini la BAT BUOC de thu vien
 * nhin thay file nay khi compile timers.c/tasks.c/... (PlatformIO khong tu
 * them src/ cua project vao include path khi build thu vien).
 *
 * Khac biet so voi FreeRTOSConfig_Default.h:
 *   - configUSE_TIMERS            1 -> 0  (khong dung xTimer; tiet kiem RAM
 *                                          timer task + timer queue)
 *   - configMINIMAL_STACK_SIZE    tu linker symbol -> hang so 128 (hang so
 *                                          compile-time, dung duoc cho mang tinh)
 *   - configUSE_TRACE_FACILITY    1 -> 0  (khong dung trace)
 *   - configQUEUE_REGISTRY_SIZE   8 -> 0  (registry chi cho debugger, ~64B)
 *   - configSUPPORT_STATIC_ALLOCATION = 1 (dinh nghia ngay tai day thay vi
 *                                          phu thuoc -D trong platformio.ini)
 */

#ifndef STM32_FREERTOS_CONFIG_H
#define STM32_FREERTOS_CONFIG_H

/* Ensure stdint is only used by the compiler, and not the assembler. */
#if defined(__ICCARM__) || defined(__CC_ARM) || defined(__GNUC__)
 #include <stdint.h>
 extern uint32_t SystemCoreClock;
#endif

#define configMAX_PRIORITIES              (7)

extern char _end;            /* Defined in the linker script */
extern char _estack;         /* Defined in the linker script */
extern char _Min_Stack_Size; /* Defined in the linker script */

/* Hang so compile-time (128 word = 512 byte) thay vi linker symbol /8 nhu
 * default (khong phai integral constant expression, khong dung cho mang tinh
 * duoc). Voi cap phat tinh toan bo, macro nay hau nhu chi con dung lam moc
 * tham khao. */
#define configMINIMAL_STACK_SIZE          ((uint16_t)128)
#define configTOTAL_HEAP_SIZE             ((size_t)((uint32_t)&_estack - (uint32_t)&_Min_Stack_Size - (uint32_t)&_end))
#define configISR_STACK_SIZE_WORDS        ((uint32_t)&_Min_Stack_Size/4)

/* Cap phat tinh: 3 task app + idle dung xTaskCreateStatic/callback (xem
 * main.cpp, rtos_glue.cpp). Giu ca dynamic (newlib heap) cho ben trong
 * kernel/thu vien neu can. */
#define configSUPPORT_STATIC_ALLOCATION   1
#define configSUPPORT_DYNAMIC_ALLOCATION  1

#define configUSE_PREEMPTION              1
#define configUSE_IDLE_HOOK               1
#define configUSE_TICK_HOOK               1
#define configCPU_CLOCK_HZ                (SystemCoreClock)
#define configTICK_RATE_HZ                ((TickType_t)1000)
#define configMAX_TASK_NAME_LEN           (16)
#define configUSE_TRACE_FACILITY          0
#define configUSE_16_BIT_TICKS            0
#define configIDLE_SHOULD_YIELD           1
#define configUSE_MUTEXES                 1
#define configQUEUE_REGISTRY_SIZE         0
/* 2026-07-03: BAT muc 2 (kiem tra pattern 16 byte cuoi stack moi lan switch
 * context) de debug treo sau khi giam stack cac task. Hook bao loi bang LED:
 * xem vApplicationStackOverflowHook trong main.cpp. Khi da chay on dinh va
 * xac nhan high-water-mark du du (lenh "rtos stat"), co the ha ve 0 de giam
 * chut overhead moi lan chuyen task. */
#define configCHECK_FOR_STACK_OVERFLOW    2
#define configUSE_RECURSIVE_MUTEXES       1
#define configUSE_MALLOC_FAILED_HOOK      0
#define configUSE_APPLICATION_TASK_TAG    0
#define configUSE_COUNTING_SEMAPHORES     1
#define configGENERATE_RUN_TIME_STATS     0
/* 2026-07-03: TAT reent rieng tung task (default bat) - tiet kiem ~96B/TCB
 * x4 task. AN TOAN vi sprintf/sscanf (newlib stdio, thu dung reent) CHI duoc
 * goi tu task CLI (hal.cpp); RS485/RF/idle chi dung Print class cua Arduino
 * (khong qua newlib stdio). NEU sau nay them sprintf/sscanf/strtok/errno vao
 * task khac -> phai bat lai = 1. */
#define configUSE_NEWLIB_REENTRANT        0

#define configENABLE_MPU                  0
#define configENABLE_FPU                  1
#define configENABLE_TRUSTZONE            0

/* Co-routine definitions. */
#define configUSE_CO_ROUTINES             0
#define configMAX_CO_ROUTINE_PRIORITIES  (2)

/* Software timer definitions - TAT (khong dung xTimer trong firmware nay). */
#define configUSE_TIMERS                  0

/* Set the following definitions to 1 to include the API function, or zero
to exclude the API function. */
#define INCLUDE_vTaskPrioritySet          1
#define INCLUDE_uxTaskPriorityGet         1
#define INCLUDE_vTaskDelete               1
#define INCLUDE_vTaskCleanUpResources     1
#define INCLUDE_vTaskSuspend              1
#define INCLUDE_vTaskDelayUntil           1
#define INCLUDE_vTaskDelay                1
#define INCLUDE_xTaskGetSchedulerState    1
#define INCLUDE_uxTaskGetStackHighWaterMark 1  /* "rtos stat" dung */
#define INCLUDE_xTaskGetIdleTaskHandle    1    /* "rtos stat" dung */

/* Cortex-M specific definitions. */
#ifdef __NVIC_PRIO_BITS
 #define configPRIO_BITS                  __NVIC_PRIO_BITS
#else
 #define configPRIO_BITS                  4
#endif

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY   0xf
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY   ( 14 << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY  ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

#define configASSERT( x ) if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }

/* Map FreeRTOS port interrupt handlers to CMSIS standard names. */
#define vPortSVCHandler    SVC_Handler
#define xPortPendSVHandler PendSV_Handler

#endif /* STM32_FREERTOS_CONFIG_H */
