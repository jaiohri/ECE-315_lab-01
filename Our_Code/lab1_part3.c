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
    } KeypadData_t;

    typedef struct {
        u32 button_value; 
    } ButtonData_t; 



    /*****************************************************************************/

    // Function prototypes
    void InitializeKeypad();
    static void vKeypadTask( void *pvParameters );
    static void vRGBTask1( void *pvParameters ); //part2
    // static void vRGBTask1(void *pvParameters);
    static void vButtonsTask( void *pvParameters );
    static void vDisplayTask( void *pvParameters );
    u32 SSD_decode(u8 key_value, u8 cathode);

    /*****************************************************************************/

    // Custom function to initialize our SSD

    void InitializeSSD() {
        int status;

        // 1. Initialize the GPIO driver
        status = XGpio_Initialize(&SSDInst, SSD_DEVICE_ID);
        if (status != XST_SUCCESS) {
            xil_printf("SSD Initialization Failed!\r\n");
            return;
        }

        // 2. Set the Direction
        // We use channel 1; there's only one channel defined in xparameters.h
        XGpio_SetDataDirection(&SSDInst, 1, 0x00); 
    }


    void InitializeRGB_LED() {
        int status;

        // 1. Initialize the GPIO driver
        status = XGpio_Initialize(&RGB_LEDInst, RGB_LED_DEVICE_ID);
        if (status != XST_SUCCESS) {
            xil_printf("RGB LED Initialization Failed!\r\n");
            return;
        }

        // 2. Set the Direction
        // We use channel 1; there's only one channel defined in xparameters.h
        XGpio_SetDataDirection(&RGB_LEDInst, 2, 0x00); 
    }

    void InitializePush_Button() {
        int status;

        // 1. Initialize the GPIO driver
        status = XGpio_Initialize(&PUSH_BUTTONInst, XPAR_GPIO_INPUTS_BASEADDR);
        if (status != XST_SUCCESS) {
            xil_printf("Push Button Initialization Failed!\r\n");
            return;
        }

        // 2. Set the Direction
        // We use channel 1; there's only one channel defined in xparameters.h
        XGpio_SetDataDirection(&PUSH_BUTTONInst, 1, 0x00); // channel 1 from Vivado Block Diagram for 'button'
    }

    int main(void) {
        // Initialize Hardware
        InitializeKeypad();
        InitializeSSD();
        InitializeRGB_LED();
        InitializePush_Button();

        xil_printf("Initialization Complete, System Ready!\n");

        // Create Queues
        xKeypadDisplayQueue = xQueueCreate(1, sizeof(KeypadData_t));
        // FIX: Corrected typo 'xDeypad...'
        if (xKeypadDisplayQueue == NULL) {
            xil_printf("ERROR: Failed to create keypad display queue \r\n");
            return 1;
        }

        xButtonsRGBQueue = xQueueCreate(1, sizeof(ButtonData_t));
        // FIX: Corrected 'xil_print' to 'xil_printf'
        if (xButtonsRGBQueue == NULL) {
            xil_printf("ERROR: Failed to create buttons-RGB queue \r\n");
            return 1;
        }

        // Create Tasks
        xTaskCreate(vKeypadTask, "Keypad", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
        
        // FIX: Added the missing vButtonsTask
        xTaskCreate(vButtonsTask, "Buttons", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);

        xTaskCreate(vRGBTask1, "RGB", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
        
        // FIX: Only create this ONCE
        xTaskCreate(vDisplayTask, "Display", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);

        vTaskStartScheduler();
        while(1);
        return 0;
    }


   static void vButtonsTask(void *pvParameters)
{
    u32 button_value;
    static u32 prev_button_value = 0;
    ButtonData_t button_data;
    
    xil_printf("Button Task started\r\n");
    xil_printf("Button 8: Increase brightness\r\n");
    xil_printf("Button 1: Decrease brightness\r\n");
    
    while (1) {
        button_value = XGpio_DiscreteRead(&PUSH_BUTTONInst, 1);
        
        // Only send if button state changed
        if (button_value != prev_button_value) {
            button_data.button_value = button_value;
            
            // Step 3.7: Send button data to RGB task
            if (xQueueOverwrite(xButtonsRGBQueue, &button_data) != pdTRUE) {
                xil_printf("[Buttons] ERROR: Queue send failed\r\n");
            } else if (button_value != 0) {  // Only log when a button is pressed
                xil_printf("Buttons Pressed: 0x%02X\r\n", button_value);
            }
            
            prev_button_value = button_value;
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));  // Check buttons every 50ms
    }
}
    /*****************************************************************************/

    // RGB Task - ONLY controls RGB LED, receives from buttons queue
    static void vRGBTask1(void *pvParameters){
    const uint8_t color = RGB_CYAN;
    ButtonData_t received_data;
    static u32 prev_button_value = 0;
    
    // Define the missing variables
    const TickType_t xPeriod = 20; 
    static TickType_t xON = 0;   
    TickType_t xOFF = 20;

    xil_printf("[RGB] Task started\r\n");

    while (1) {
        // FIX: Change timeout to 0 (Non-blocking)
        // If we wait here, the PWM stops and the LED flickers!
        if (xQueueReceive(xButtonsRGBQueue, &received_data, 0) == pdTRUE) {
            
            if (received_data.button_value != prev_button_value) {
                // Button 8: Increase Brightness
                if (received_data.button_value == 0x08 && xON < xPeriod) {
                    xON++; 
                    xil_printf("xON: %d\r\n", xON);
                } 
                // Button 1: Decrease Brightness
                else if (received_data.button_value == 0x01 && xON > 0) {
                    xON--;
                    xil_printf("xON: %d\r\n", xON);
                }
                prev_button_value = received_data.button_value;
            }
        }

        // Calculate OFF time
        xOFF = xPeriod - xON;

        // --- Software PWM Logic ---
        
        // 1. Turn ON
        if (xON > 0) {
            XGpio_DiscreteWrite(&RGB_LEDInst, 2, color); // Note: Ensure channel is correct (usually 1)
            vTaskDelay(xON);
        }

        // 2. Turn OFF
        if (xOFF > 0) {
            XGpio_DiscreteWrite(&RGB_LEDInst, 2, 0);
            vTaskDelay(xOFF);
        }
    }
}


    static void vDisplayTask( void *pvParameters ) {
        KeypadData_t current_data = {0, 0}; // Default/Safe values
        KeypadData_t received_data;
        const TickType_t xDelay = pdMS_TO_TICKS(10); 
        
        xil_printf("[Display] Task started\r\n");
        
        while (1) {
            // 1. Check for NEW data, but do NOT wait/block
            if (xQueueReceive(xKeypadDisplayQueue, &received_data, 0) == pdTRUE) {
                current_data = received_data; // Update local state
            }
            
            // 2. ALWAYS refresh the display using current_data
            // Right Digit
            XGpio_DiscreteWrite(&SSDInst, 1, SSD_decode(current_data.current_key, 1)); 
            vTaskDelay(xDelay);
            
            // Left Digit
            XGpio_DiscreteWrite(&SSDInst, 1, SSD_decode(current_data.previous_key, 0));
            vTaskDelay(xDelay);
        }
    }

    

    static void vKeypadTask( void *pvParameters ) {
    u16 keystate;
    XStatus status, previous_status = KYPD_NO_KEY;
    u8 new_key, current_key = 'x', previous_key = 'x';
    
    // Structure to hold data for the queue
    KeypadData_t keypad_data;

    // 50ms is fast enough for human input but saves CPU compared to 10ms
    const TickType_t xDelay = pdMS_TO_TICKS(50); 

    xil_printf("Pmod KYPD app started. Press any key on the Keypad.\r\n");

    while (1) {
        // 1. Capture state of the keypad
        keystate = KYPD_getKeyStates(&KYPDInst);
        status = KYPD_getKeyPressed(&KYPDInst, keystate, &new_key);

        // 2. Check for Valid Single Key Press (Rising Edge)
        if (status == KYPD_SINGLE_KEY && previous_status == KYPD_NO_KEY) {
            xil_printf("Key Pressed: %c\r\n", (char) new_key);

            // Shift the keys
            previous_key = current_key;
            current_key = new_key;

            // Prepare the data packet
            keypad_data.current_key = current_key;
            keypad_data.previous_key = previous_key;

            // Send to Queue (Overwrites old data so display always shows latest)
            if (xQueueOverwrite(xKeypadDisplayQueue, &keypad_data) != pdTRUE) {
                xil_printf("[Keypad] ERROR: Queue send failed\r\n");
            } else {
                xil_printf("[Keypad] Sent to display: current='%c', previous='%c'\r\n", 
                           current_key, previous_key);
            }

        } else if (status == KYPD_MULTI_KEY && status != previous_status) {
            xil_printf("Error: Multiple keys pressed\r\n");
        }

        // 3. Print status changes (Idle -> Pressed -> Idle)
        if (status != previous_status) {
            xil_printf("Status: %d\r\n", status);
        }
        
        previous_status = status;

        // 4. Delay to allow other tasks to run
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
