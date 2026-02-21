        /*
    * Lab 1, Part 2 - Seven-Segment Display & Keypad
    *
    * ECE-315 WINTER 2025 - COMPUTER INTERFACING
    * Created on: February 5, 2021
    * Modified on: July 26, 2023
    * Modified on: January 20, 2025
    * Author(s):  Shyama Gandhi, Antonio Andara Lara
    *
    * Summary:
    * 1) Declare & initialize the 7-seg display (SSD).
    * 2) Use xDelay to alternate between two digits fast enough to prevent flicker.
    * 3) Output pressed keypad digits on both SSD digits: current_key on right, previous_key on left.
    * 4) Print status changes and experiment with xDelay to find minimum flicker-free frequency.
    *
    * Deliverables:
    * - Demonstrate correct display of current and previous keys with no flicker.
    * - Print to the SDK terminal every time that theh variable `status` changes.
    */


    // Include FreeRTOS Libraries
    #include <FreeRTOS.h>
    #include <task.h>
    #include <queue.h>

    // Include xilinx Libraries
    #include <xparameters.h>
    #include <xgpio.h>
    #include <xscugic.h>
    #include <xil_exception.h>
    #include <xil_printf.h>
    #include <sleep.h>
    #include <xil_cache.h>

    // Other miscellaneous libraries
    #include "pmodkypd.h"
    #include "rgb_led.h"


    // Device ID declarations
    #define KYPD_DEVICE_ID   	XPAR_GPIO_KYPD_BASEADDR
    /*************************** Enter your code here ****************************/
    // TODO: Define the seven-segment display (SSD) base address.

    #define SSD_DEVICE_ID      XPAR_GPIO_SSD_BASEADDR // from part 1

    #define RGB_LED_DEVICE_ID   XPAR_GPIO_LEDS_BASEADDR

    #define PUSH_BUTTON_DEVICE_ID       XPAR_GPIO_INPUTS_BASEADDR

    /*****************************************************************************/

    // keypad key table
    #define DEFAULT_KEYTABLE 	"0FED789C456B123A"

    // Declaring the devices
    PmodKYPD 	KYPDInst;

    /*************************** Enter your code here ****************************/
    // TODO: Declare the seven-segment display peripheral here.

    XGpio    SSDInst;

    XGpio    RGB_LEDInst; 

    XGpio    PUSH_BUTTONInst;

    /*****************************************************************************/

    // Function prototypes
    void InitializeKeypad();
    void InitializePeripherals();
    static void vKeypadTask( void *pvParameters );

    //PART 2
    static void vRGBTask( void *pvParameters );

    u32 SSD_decode(u8 key_value, u8 cathode);

    /*****************************************************************************/

    static void vRGBTask(void *pvParameters)
    {
        const uint8_t color = RGB_CYAN;
        const TickType_t xPeriod = 25;
        TickType_t xOnDelay = 0;
        TickType_t xOffDelay = xPeriod - xOnDelay;
        u32 button_value;

        while (1){
            button_value = XGpio_DiscreteRead(&PUSH_BUTTONInst, 1);
           
            if (button_value == 0x08 && xOnDelay < xPeriod) {
                vTaskDelay(100);
                xOnDelay++;
                xil_printf("xOnDelay: %d, xOffDelay: %d\n", xOnDelay, xPeriod - xOnDelay);
            } else if (button_value == 0x01 && xOnDelay > 0) {
                vTaskDelay(100);
                xOnDelay--;
                xil_printf("xOnDelay: %d, xOffDelay: %d\n", xOnDelay, xPeriod - xOnDelay);
            }

            xOffDelay = xPeriod - xOnDelay;
            /* LED on for xOnDelay ticks */
            XGpio_DiscreteWrite(&RGB_LEDInst, RGB_CHANNEL, color);
            if (xOnDelay == 0) {
                XGpio_DiscreteWrite(&RGB_LEDInst, RGB_CHANNEL, 0);
            }
            vTaskDelay(xOnDelay);

            /* LED off for xOffDelay ticks */
            XGpio_DiscreteWrite(&RGB_LEDInst, RGB_CHANNEL, 0);
            vTaskDelay(xOffDelay);
        }
    }


    void InitializePeripherals(void)
    {
        // 1. Initialize SSD
        XGpio_Initialize(&SSDInst, SSD_DEVICE_ID);
        XGpio_SetDataDirection(&SSDInst, 1, 0x00);

        // 2. Initialize RGB LED
        XGpio_Initialize(&RGB_LEDInst, RGB_LED_DEVICE_ID);
        XGpio_SetDataDirection(&RGB_LEDInst, 2, 0x00);

        // 3. Initialize Push Button
        XGpio_Initialize(&PUSH_BUTTONInst, PUSH_BUTTON_DEVICE_ID);
        XGpio_SetDataDirection(&PUSH_BUTTONInst, 1, 0x0F);  // channel 1, configure as inputs
    }

    int main(void)
    {
        int status;

        // Initialize keypad
        InitializeKeypad();

    /*************************** Enter your code here ****************************/
        // TODO: Initialize SSD and set the GPIO direction to output.

        InitializePeripherals();


        
    /*****************************************************************************/

        xil_printf("Initialization Complete, System Ready!\n");

        xTaskCreate(vKeypadTask,					/* The function that implements the task. */
                    "main task", 				/* Text name for the task, provided to assist debugging only. */
                    configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
                    NULL, 						/* The task parameter is not used, so set to NULL. */
                    tskIDLE_PRIORITY,			/* The task runs at the idle priority. */
                    NULL);

        // Create RGB Task
        xTaskCreate(vRGBTask, "RGB", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);

        vTaskStartScheduler();
        while(1);
        return 0;
    }

    

    static void vKeypadTask( void *pvParameters )
    {
        u16 keystate;
        XStatus status, previous_status = KYPD_NO_KEY;
        u8 new_key, current_key = 'x', previous_key = 'x';
        u32 ssd_value=0;

    /*************************** Enter your code here ****************************/
        // TODO: Define a constant of type TickType_t named 'xDelay' and initialize
        //       it with a value of 100.

    const TickType_t xDelay = pdMS_TO_TICKS(10); 

    /*****************************************************************************/

        xil_printf("Pmod KYPD app started. Press any key on the Keypad.\r\n");
        while (1){
            // Capture state of the keypad
            keystate = KYPD_getKeyStates(&KYPDInst);

            // Determine which single key is pressed, if any
            // if a key is pressed, store the value of the new key in new_key
            status = KYPD_getKeyPressed(&KYPDInst, keystate, &new_key);
            // Print key detect if a new key is pressed or if status has changed
            if (status == KYPD_SINGLE_KEY && previous_status == KYPD_NO_KEY){
                xil_printf("Key Pressed: %c\r\n", (char) new_key);
    /*************************** Enter your code here ****************************/
                // TODO: update value of previous_key and current_key
            
            previous_key = current_key;
            current_key = new_key;



    /*****************************************************************************/
            } else if (status == KYPD_MULTI_KEY && status != previous_status){
                xil_printf("Error: Multiple keys pressed\r\n");
            }
            
    /*************************** Enter your code here ****************************/
            // TODO: display the value of `status` each time it changes
            if (status != previous_status) {
                xil_printf("Status: %d\r\n", status);
            }
    /*****************************************************************************/
            previous_status = status;

    /*************************** Enter your code here ****************************/
            /* TODO: Decode the current and previous keys using the `SSD_decode` function.
            * Write each decoded value to the seven-segment display, one at a time,
            * using the `XGpio_DiscreteWrite` function.
            * Add a delay between updates for persistence of vision using `vTaskDelay`.
            */

            // Show current key on right SSD
            u32 right_ssd = SSD_decode(current_key, 1);
            XGpio_DiscreteWrite(&SSDInst, 1, right_ssd);
            vTaskDelay(xDelay);

            // Show previous key on left SSD
            u32 left_ssd = SSD_decode(previous_key, 0);
            XGpio_DiscreteWrite(&SSDInst, 1, left_ssd);
            vTaskDelay(xDelay);

                    


    /*****************************************************************************/
        }
    }



    void InitializeKeypad()
    {
        KYPD_begin(&KYPDInst, KYPD_DEVICE_ID);
        KYPD_loadKeyTable(&KYPDInst, (u8*) DEFAULT_KEYTABLE);
    }


    // This function is hard coded to translate key value codes to their binary representation
    u32 SSD_decode(u8 key_value, u8 cathode)
    {
        u32 result;

        // key_value represents the code of the pressed key
        switch(key_value){ // Handles the coding of the 7-seg display
            case 48: result = 0b00111111; break; // 0
            case 49: result = 0b00110000; break; // 1
            case 50: result = 0b01011011; break; // 2
            case 51: result = 0b01111001; break; // 3
            case 52: result = 0b01110100; break; // 4
            case 53: result = 0b01101101; break; // 5
            case 54: result = 0b01101111; break; // 6
            case 55: result = 0b00111000; break; // 7
            case 56: result = 0b01111111; break; // 8
            case 57: result = 0b01111100; break; // 9
            case 65: result = 0b01111110; break; // A
            case 66: result = 0b01100111; break; // B
            case 67: result = 0b00001111; break; // C
            case 68: result = 0b01110011; break; // D
            case 69: result = 0b01001111; break; // E
            case 70: result = 0b01001110; break; // F
            default: result = 0b00000000; break; // default case - all seven segments are OFF
        }

        // cathode handles which display is active (left or right)
        // by setting the MSB to 1 or 0
        if(cathode==0){
                return result;
        } else {
                return result | 0b10000000;
        }
    }
