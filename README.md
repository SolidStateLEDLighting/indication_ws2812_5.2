# Indication 5.2 Overview
The indication class controls an external RGB LED.  That indicator is an addressable WS2812 LED with it's control pin connected to GPIO 48 (for the DevKitM and possibly DevKitC).

* **We can make the LED flash to represent 1 or 2 numbers with "blinks" of color (up to 13 cycles)**  
* **We can set the intensity of these colors**  
* **We can adjust the on and off time which effectively controls the speed of the blinking**  

Colors may be of a single color (Red, Green, Blue) or composed of a combination color (Yellow, Cyan, Purple, White).

**Received Message Types:**

1) Task Notification (32 bit number) is used to set the color output intensity.  
2) A Command (32 bit number) can be sent to a Queue and is used to trigger the output code and speed of the blinking.  

Typically, the user would set the intensity to an appropriate level for the hardware and then use commands to trigger output codes.  The Queue depth is typically set to 3 and output codes will follow each other as needed.

## Setting Output Intensity:  

Intensity is held by an 8 bit values.  We have the ability to easily send four 8 bit values in a task notification (32 bits) but, our hardware only make use of the lowest 3 bytes for RGB color intensities.

bits  7-0  set Red   intensity
bits 15-8  set Green intensity
bits 23-16 set Blue  intensity


## Setting the Code Number(s) or Setting LED States:  

**32 Bit Format:**
MSB byte 1 \<colorA/cycles\>  byte 2 \<colorB/cycles\>  byte 3 \<time on\>  byte 4 \<time off\> LSB


**Color/Cycles byte 1**
This byte can either indicate the First color and cycles OR it can deliver a State Change (special command) to any of the color outputs (ON, OFF, AUTO)

1st byte format for FirstColor is   MSBit  0x<Colors><Cycles>  LSBit  
\<Colors\>   0x1 = ColorA, 0x2 = ColorB, 0x4 = ColorC (3 bits in use here)  
\<Cycles\>   9 possible flashes - 0x01 though 0x0E (1 through 9) 

Special Command Codes can alternatively occupy the Cycles byte: 0x00 = ON State, 0x0E = AUTO State, 0x0F = OFF State (4 bits in use here).

> [!NOTE]  
>Special Command codes do apply to the states of all LEDs in a combination color.  

> [!WARNING]  
>If you use the Special Command Codes of 0x00 (ON State) or 0x0F (OFF State), you will need to call the Special Command of 0x0E (AUTO State) to return to normal blinking activity.  



**Color/Cycles byte 2**  
Second byte functions only as the Second possible Color/Cycles indication.  



**On_Time byte 3**  
The values of on_time indicates how long and LED color will flash for.



**Off_Time byte 4**  
 Off_time is the time between flashes.  These are shared between both possible color sequences.  NOTE:  Time between codes is always double off_time.


## Command Examples: 

>Example1: Red, 1 flash, Green, 2 flashes long on-time long off-time  
>ColorA 0x10, Cycles 0x01, ColorB 0x20 , Cycles 0x02, On-Time 0x20, Off-Time 0x30  
>0x10000000 | 0x01000000 | 0x00200000 | 0x00020000 | 0x00002000 | 0x00000030  
>0x11222030 is our 32 bit value

>Example2: Blue, 3 flashes, NoColor, NoFlashes  short on-time moderate off-time  
>ColorA 0x40, Cycles 0x03, ColorB 0x00 , Cycles 0x00, On-Time 0x01, Off-Time 0x15  
>0x40000000 | 0x03000000 | 0x00000000 | 0x00000000 | 0x00000100 | 0x00000015  
>0x43000115 is our 32 bit value

>Example3: Turn Red On continously  **(state change)**
>Red Color - Cycles - NoColor - Cycles - On-Time - Off-Time  
>0x1 -------- F ------- 0 --------- 0 ----- 00 ------ 00  
>0x10000000 

>Example4: All Colors On continously  **(state change)**
>All Colors - Cycles - NoColor - Cycles - On-Time - Off-Time  
>0x7 -------- F ------ 0 --------- 0 ----- 00 ------ 00  
>0x70000000 

>Example5: All Colors OFF completely  **(state change)**
>All Colors - Cycles - NoColor - Cycles - On-Time - Off-Time  
>0x7 -------- 0 ------ 0 --------- 0 ----- 00 ------ 00  
>0x7F000000  

>Example6: Return All Colors to normal operation  **(state change)**
>All Colors - Cycles - NoColor - Cycles On-Time - Off-Time  
>0x7 -------- E ------ 0 --------- 0 ----- 00 ------ 00  
>0x7E000000  

>Example7: Blue 1 flash, Red 3 flahses  
>Blue - Cycles - Red - Cycles - On-Time  Off-Time  
>0x4 -------- 1 ------ 1 --------- 3 ----- 09 ------ 12  
>0x41130912  

PLEASE CALL THE INDICATION SERVICE LIKE THIS:

int32_t val = 0x22420919; // Color1 is Green 2 cycles Color2 is Blue 2 cycles. Off time 09 and On time 19 (25 dec)  

xQueueSendToBack(queHandleCmdRequest, &val, 30);


## Notification Examples: 

**Remember, a notification command only sets the LEDs brightness level.**  

int32_t brightnessLevel = 0;  
brightnessLevel = (uint32_t)IND_NOTIFY::NFY_SET_A_COLOR_BRIGHTNESS; // First we set the target color bit  
brightnessLevel |= 20;                                          // Supply the brightness value  

while (!xTaskNotify(taskHandleIndRun, brightnessLevel, eSetValueWithoutOverwrite))  
     vTaskDelay(pdMS_TO_TICKS(50));  


brightnessSetting |= (uint32_t)IND_NOTIFY::NFY_SET_A_COLOR_BRIGHTNESS; // Red
brightnessSetting |= (uint32_t)IND_NOTIFY::NFY_SET_C_COLOR_BRIGHTNESS; // Blue
brightnessLevel |= 50;                                             // Supply the brightness value  

while (!xTaskNotify(taskHandleIndRun, brightnessLevel, eSetValueWithoutOverwrite))  
     vTaskDelay(pdMS_TO_TICKS(50));




## Abstractions  
[Indication Abstraction](./docs/ind_abstractions.md)

## Block Diagrams  
[Indication Block Diagrams](./docs/ind_sequences.md)

## Flowcharts  
[Indication Flowcharts](./docs/ind_flowcharts.md)

## Sequence Diagrams  
[Indication Sequence Diagrams](./docs/ind_sequences.md)

## State Transition Diagrams  
[Indication State Models](./docs/ind_state_models.md)