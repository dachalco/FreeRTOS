/*
 * FreeRTOS Kernel V10.3.0
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

/******************************************************************************
 * This project provides two demo applications.  A simple blinky style project,
 * and a more comprehensive test and demo application.  The
 * mainCREATE_SIMPLE_BLINKY_DEMO_ONLY setting is used to select between the two.
 * The simply blinky demo is implemented and described in main_blinky.c.  The
 * more comprehensive test and demo application is implemented and described in
 * main_full.c.
 *
 * This file implements the code that is not demo specific, including the
 * hardware setup and FreeRTOS hook functions.
 *
 *******************************************************************************
 * NOTE: Windows will not be running the FreeRTOS demo threads continuously, so
 * do not expect to get real time behaviour from the FreeRTOS Windows port, or
 * this demo application.  Also, the timing information in the FreeRTOS+Trace
 * logs have no meaningful units.  See the documentation page for the Windows
 * port for further information:
 * http://www.freertos.org/FreeRTOS-Windows-Simulator-Emulator-for-Visual-Studio-and-Eclipse-MingW.html
 *

 *
 *******************************************************************************
 */

/* Standard includes. */
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>


/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "list.h"


/* This demo uses heap_5.c, and these constants define the sizes of the regions
that make up the total heap.  heap_5 is only used for test and example purposes
as this demo could easily create one large heap region instead of multiple
smaller heap regions - in which case heap_4.c would be the more appropriate
choice.  See http://www.freertos.org/a00111.html for an explanation. */
#define mainREGION_1_SIZE	8201
#define mainREGION_2_SIZE	29905
#define mainREGION_3_SIZE	7607
static void  prvInitialiseHeap( void );

/*
 * Prototypes for the standard FreeRTOS application hook (callback) functions
 * implemented within this file.  See http://www.freertos.org/a00016.html .
 */
void vApplicationMallocFailedHook( void );
void vApplicationIdleHook( void );
void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName );
void vApplicationTickHook( void );
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize );

static SemaphoreHandle_t xSharedSem;

/*-----------------------------------------------------------*/

void vWaitForSemaphore( void * pvArgs)
{
	printf("[Task][Sem-wait] Waiting for xSharedSem\n");
	while (pdTRUE != xSemaphoreTake(xSharedSem, portMAX_DELAY));
	printf("[Task][Sem-wait] xSharedSem received\n");
	xSemaphoreGive(xSharedSem);

	while (1)
	{
		// Intentionally stay busy so calls to vTaskGetCurrentBlocker get eStatus = eNotBlocked
	}

	vTaskDelete(NULL);
}

void vWaitForNotification( void * pvArgs )
{
	printf("[Task][notify-wait] Waiting for notification\n");
	while (pdPASS != xTaskNotifyWait(0u, 0u, NULL, portMAX_DELAY));
	printf("[Task][notify-wait] notification received\n");

	while (1)
	{
		// Intentionally stay busy so calls to vTaskGetCurrentBlocker get eStatus = eNotBlocked
	}

	vTaskDelete(NULL);
}

void vAlwaysDelay( void * pvArgs)
{
	const TickType_t delay = 1000;
	while (1)
	{
		printf("[Task][sleeper] Waking up at Tick=%d\n", xTaskGetTickCount() + delay);
		vTaskDelay(delay);
	}

	vTaskDelete(NULL);
}

/* Task state management differs when INCLUDE_vTaskSuspend == 1 and when it does not. Therefore, recompile and test for goth 
INCLUDE_vTaskSuspend == 1 and when it does not. */
void prvTest( void * pvArgs )
{
	TaskBlockedStatus_t xStatus;
	TaskHandle_t xHandle;
	eTaskState state;

#if INCLUDE_vTaskSuspend == 1
	printf("Testing without suspension...\n");
#else
	printf("Testing without suspension...\n");
#endif
	printf("Note: Task state management differs when INCLUDE_vTaskSuspend == 1 and when it does not. \n"
		"      Therefore, recompile and test with INCLUDE_vTaskSuspend == 1 and != 1.\n");

	/*-----------------------------------------------------------------------------*/
	/* TEST CASE: eBlockForEvent - Can use semaphore since it internally uses queue*/
	/* eTake the sem then create a task that only blocks for it, ensure correct blocked status */
	while (pdTRUE != xSemaphoreTake(xSharedSem, portMAX_DELAY)); 
	configASSERT(pdPASS == xTaskCreate(vWaitForSemaphore,
										"SemWait",
										configMINIMAL_STACK_SIZE,
										tskIDLE_PRIORITY + 1,
										NULL,
										&xHandle));

	/* Let enough time pass so the waiting thread has time to request the sem, then verify that it is blocked*/
	vTaskDelay(100);
	vTaskGetCurrentBlocker(xHandle, &xStatus);
	state = eTaskGetState(xHandle);
	configASSERT(state == eBlocked && xStatus.eStatus == eBlockedForEvent);
	configASSERT((SemaphoreHandle_t)listGET_LIST_OWNER(xStatus.pxEventList) == xSharedSem);

	/* Give the semaphore, thereby unblocking the task and reverify state */
	xSemaphoreGive(xSharedSem);
	vTaskDelay(100);
	vTaskGetCurrentBlocker(xHandle, &xStatus);
	state = eTaskGetState(xHandle);
	configASSERT(state != eBlocked && xStatus.eStatus == eNotBlocked);
	vTaskDelete(xHandle);

	/*-----------------------------------------------------------------------------*/
	/* TEST CASE: eBlockForNotification */
	configASSERT(pdPASS == xTaskCreate(vWaitForNotification,
		"NoteWait",
		configMINIMAL_STACK_SIZE,
		tskIDLE_PRIORITY + 1,
		NULL,
		&xHandle));

	/* Let enough time pass so the waiting thread has time to request the notificication, then verify that it is blocked*/
	vTaskDelay(100);
	vTaskGetCurrentBlocker(xHandle, &xStatus);
	state = eTaskGetState(xHandle);
	configASSERT(state == eBlocked && xStatus.eStatus == eBlockedForNotification);

	/* Give the semaphore, thereby unblocking the task and reverify state */
	xTaskNotify(xHandle, 0xDC, eNoAction);
	vTaskDelay(100);
	vTaskGetCurrentBlocker(xHandle, &xStatus);
	state = eTaskGetState(xHandle);
	configASSERT(state != eBlocked && xStatus.eStatus == eNotBlocked);
	vTaskDelete(xHandle);

	/*-----------------------------------------------------------------------------*/
	/* TEST CASE: eBlockedForTime */
	configASSERT(pdPASS == xTaskCreate(vAlwaysDelay,
		"Sleeper",
		configMINIMAL_STACK_SIZE,
		tskIDLE_PRIORITY + 1,
		NULL,
		&xHandle));

	/* Delay long enough for the sleeper task to call its delay*/
	vTaskDelay(100);
	vTaskGetCurrentBlocker(xHandle, &xStatus);
	state = eTaskGetState(xHandle);
	configASSERT(state == eBlocked && xStatus.eStatus == eBlockedForTime);
	printf("[Task][tester] sleeper task waking up @ tick=%d\n", xStatus.xUntilTick);
	vTaskDelete(xHandle);

	/*-----------------------------------------------------------------------------*/
	/* TEST CASE: eNotBlocked - Already covered in other tests, but here we verify when tasks calls on itself */
	vTaskGetCurrentBlocker(xTaskGetCurrentTaskHandle(), &xStatus);
	configASSERT(xStatus.eStatus == eNotBlocked);

	printf("Pass.\n");
	vTaskDelete(NULL);
}

/*-----------------------------------------------------------*/
int main( void )
{
TaskHandle_t xHandle;
	
	prvInitialiseHeap();
	
	printf("\nStarting...\n");	
	/* Initialize shared semaphore that will be used to block test threads */
	vSemaphoreCreateBinary(xSharedSem);

	configASSERT(pdPASS == xTaskCreate(prvTest,
		"test",
		configMINIMAL_STACK_SIZE,
		NULL,
		tskIDLE_PRIORITY + 1,
		&xHandle));

	vTaskStartScheduler();


	printf("\nDone!\n");

	return 0;
}
/*-----------------------------------------------------------*/
void vApplicationMallocFailedHook( void )
{
	/* vApplicationMallocFailedHook() will only be called if
	configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
	function that will get called if a call to pvPortMalloc() fails.
	pvPortMalloc() is called internally by the kernel whenever a task, queue,
	timer or semaphore is created.  It is also called by various parts of the
	demo application.  If heap_1.c, heap_2.c or heap_4.c is being used, then the
	size of the	heap available to pvPortMalloc() is defined by
	configTOTAL_HEAP_SIZE in FreeRTOSConfig.h, and the xPortGetFreeHeapSize()
	API function can be used to query the size of free heap space that remains
	(although it does not provide information on how the remaining heap might be
	fragmented).  See http://www.freertos.org/a00111.html for more
	information. */
	vAssertCalled( __LINE__, __FILE__ );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook(void)
{
	/* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
	to 1 in FreeRTOSConfig.h.  It will be called on each iteration of the idle
	task.  It is essential that code added to this hook function never attempts
	to block in any way (for example, call xQueueReceive() with a block time
	specified, or call vTaskDelay()).  If application tasks make use of the
	vTaskDelete() API function to delete themselves then it is also important
	that vApplicationIdleHook() is permitted to return to its calling function,
	because it is the responsibility of the idle task to clean up memory
	allocated by the kernel to any task that has since deleted itself. */

}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected.  This function is
	provided as an example only as stack overflow checking does not function
	when running the FreeRTOS Windows port. */
	vAssertCalled( __LINE__, __FILE__ );
}
/*-----------------------------------------------------------*/

void vApplicationTickHook( void )
{
	/* This function will be called by each tick interrupt if
	configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h.  User code can be
	added here, but the tick hook is called from an interrupt context, so
	code must not attempt to block, and only the interrupt safe FreeRTOS API
	functions can be used (those that end in FromISR()). */
}
/*-----------------------------------------------------------*/

void vApplicationDaemonTaskStartupHook( void )
{
	/* This function will be called once only, when the daemon task starts to
	execute	(sometimes called the timer task).  This is useful if the
	application includes initialisation code that would benefit from executing
	after the scheduler has been started. */
}
/*-----------------------------------------------------------*/

void vAssertCalled( unsigned long ulLine, const char * const pcFileName )
{
static BaseType_t xPrinted = pdFALSE;

	/* Called if an assertion passed to configASSERT() fails.  See
	http://www.freertos.org/a00110.html#configASSERT for more information. */

	/* Parameters are not used. */
	( void ) ulLine;
	( void ) pcFileName;

	printf( "ASSERT! Line %ld, file %s, GetLastError() %ld\r\n", ulLine, pcFileName, GetLastError() );
	printf("Fail.\n");
	exit(1);
}
/*-----------------------------------------------------------*/


static void  prvInitialiseHeap( void )
{
/* The Windows demo could create one large heap region, in which case it would
be appropriate to use heap_4.  However, purely for demonstration purposes,
heap_5 is used instead, so start by defining some heap regions.  No
initialisation is required when any other heap implementation is used.  See
http://www.freertos.org/a00111.html for more information.

The xHeapRegions structure requires the regions to be defined in start address
order, so this just creates one big array, then populates the structure with
offsets into the array - with gaps in between and messy alignment just for test
purposes. */
static uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
volatile uint32_t ulAdditionalOffset = 19; /* Just to prevent 'condition is always true' warnings in configASSERT(). */
const HeapRegion_t xHeapRegions[] =
{
	/* Start address with dummy offsets						Size */
	{ ucHeap + 1,											mainREGION_1_SIZE },
	{ ucHeap + 15 + mainREGION_1_SIZE,						mainREGION_2_SIZE },
	{ ucHeap + 19 + mainREGION_1_SIZE + mainREGION_2_SIZE,	mainREGION_3_SIZE },
	{ NULL, 0 }
};

	/* Sanity check that the sizes and offsets defined actually fit into the
	array. */
	configASSERT( ( ulAdditionalOffset + mainREGION_1_SIZE + mainREGION_2_SIZE + mainREGION_3_SIZE ) < configTOTAL_HEAP_SIZE );

	/* Prevent compiler warnings when configASSERT() is not defined. */
	( void ) ulAdditionalOffset;

	vPortDefineHeapRegions( xHeapRegions );
}
/*-----------------------------------------------------------*/

/* configUSE_STATIC_ALLOCATION is set to 1, so the application must provide an
implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
used by the Idle task. */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
/* If the buffers to be provided to the Idle task are declared inside this
function then they must be declared static - otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

	/* Pass out a pointer to the StaticTask_t structure in which the Idle task's
	state will be stored. */
	*ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

	/* Pass out the array that will be used as the Idle task's stack. */
	*ppxIdleTaskStackBuffer = uxIdleTaskStack;

	/* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
	Note that, as the array is necessarily of type StackType_t,
	configMINIMAL_STACK_SIZE is specified in words, not bytes. */
	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
