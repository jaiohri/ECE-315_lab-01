        /*
    * Lab 1, Part 3 - Seven-Segment Display & Keypad
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

    QueueHandle_t xKeypadDisplayQueue;
    QueueHandle_t xButtonsRGBQueue; 

    typedef struct {
        u8 current_key;
        u8 previous_key;
    } KeypadState_t;



    /*****************************************************************************/

    // Function prototypes
    void InitializeKeypad();
    void InitializePeripherals();
    static void vKeypadTask( void *pvParameters );
    static void vRGBTask( void *pvParameters );
    static void vButtonsTask( void *pvParameters );
    static void vDisplayTask( void *pvParameters );
    u32 SSD_decode(u8 key_value, u8 cathode);

    /*****************************************************************************/

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
        // Initialize keypad
        InitializeKeypad();

        // Initialize peripherals
        InitializePeripherals();

        xil_printf("Initialization Complete, System Ready!\n");

        // Create queues
        xKeypadDisplayQueue = xQueueCreate(1, sizeof(KeypadState_t));
        if (xKeypadDisplayQueue == NULL) {
            xil_printf("ERROR: Failed to create keypad display queue \r\n");
            return 1;
        }

        xButtonsRGBQueue = xQueueCreate(1, sizeof(u32));
        if (xButtonsRGBQueue == NULL) {
            xil_printf("ERROR: Failed to create buttons RGB queue \r\n");
            return 1;
        }

        // Create Tasks
        xTaskCreate(vKeypadTask, "Keypad", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
        xTaskCreate(vButtonsTask, "Buttons", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
        xTaskCreate(vRGBTask, "RGB", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
        xTaskCreate(vDisplayTask, "Display", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);

        vTaskStartScheduler();
        while(1);
        return 0;
    }


   static void vButtonsTask(void *pvParameters)
{
    u32 button_val;
    static u32 prev_button_val = 0;
    
    while (1) {
        button_val = XGpio_DiscreteRead(&PUSH_BUTTONInst, 1);
        
        if (button_val != prev_button_val) {
            xQueueOverwrite(xButtonsRGBQueue, &button_val);
            if (button_val != 0) {
                xil_printf("Button: 0x%02X\r\n", button_val);
            }
            prev_button_val = button_val;
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
    /*****************************************************************************/

    static void vRGBTask(void *pvParameters)
    {
        const uint8_t color = RGB_CYAN;
        const TickType_t xPeriod = 25;
        TickType_t xOnDelay = 0;
        TickType_t xOffDelay = xPeriod - xOnDelay;
        u32 button_val;
        static u32 prev_button_val = 0;

        while (1){
            if (xQueueReceive(xButtonsRGBQueue, &button_val, 0) == pdTRUE){
                if (button_val != prev_button_val) {
                    if (button_val == 0x08 && xOnDelay < xPeriod) {

                        xOnDelay++;
                        xil_printf("xOnDelay: %d, xOffDelay: %d\n", xOnDelay, xPeriod - xOnDelay);
                    } else if (button_val == 0x01 && xOnDelay > 0) {
                        xOnDelay--;
                        xil_printf("xOnDelay: %d, xOffDelay: %d\n", xOnDelay, xPeriod - xOnDelay);
                    }
                    prev_button_val = button_val;
                }
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


    static void vDisplayTask( void *pvParameters ) {
        KeypadState_t data = {0, 0};
        KeypadState_t new_data;
        const TickType_t xDelay = pdMS_TO_TICKS(10);
        
        while (1) {
            if (xQueueReceive(xKeypadDisplayQueue, &new_data, 0) == pdTRUE) {
                data = new_data;
            }
            
            XGpio_DiscreteWrite(&SSDInst, 1, SSD_decode(data.current_key, 1));
            vTaskDelay(xDelay);
            
            XGpio_DiscreteWrite(&SSDInst, 1, SSD_decode(data.previous_key, 0));
            vTaskDelay(xDelay);
        }
    }

    

    static void vKeypadTask( void *pvParameters ) {
        u16 keystate;
        XStatus status, previous_status = KYPD_NO_KEY;
        u8 new_key, current_key = 'x', previous_key = 'x';
        KeypadState_t keypad_data;
        const TickType_t xDelay = pdMS_TO_TICKS(50);

        xil_printf("Pmod KYPD app started. Press any key on the Keypad.\r\n");

        while (1) {
            // Capture state of the keypad
            keystate = KYPD_getKeyStates(&KYPDInst);

            // Determine which single key is pressed, if any
            // if a key is pressed, store the value of the new key in new_key
            status = KYPD_getKeyPressed(&KYPDInst, keystate, &new_key);
            // Print key detect if a new key is pressed or if status has changed
            if (status == KYPD_SINGLE_KEY && previous_status == KYPD_NO_KEY) {
                xil_printf("Key Pressed: %c\r\n", (char) new_key);

                previous_key = current_key;
                current_key = new_key;

                keypad_data.current_key = current_key;
                keypad_data.previous_key = previous_key;
                xQueueOverwrite(xKeypadDisplayQueue, &keypad_data);

            } else if (status == KYPD_MULTI_KEY && status != previous_status) {
                xil_printf("Error: Multiple keys pressed\r\n");
            }

            // display the value of `status` each time it changes
            if (status != previous_status) {
                xil_printf("Status: %d\r\n", status);
            }
            
            previous_status = status;
            vTaskDelay(xDelay);
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