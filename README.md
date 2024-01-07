The indication class controls an extrernal RGB LED.  That LED is an addressable WS2812 unit with a single pin located on GPIO 48.

* **We can make the LED flash to represent 1 or 2 numbers with "blinks" of color (up to 13 cycles)**  
* **We can set the intensity of these colors**  
* **We can adjust the on and off time which effectively controls the speed of the blinking**  

Colors may be of a single color or composed of a combination color.

**Received Message Types:**

1) Task Notification is used to set the color output intensity.  
2) A 32 bit command sent to a Queue is used to trigger the output code and speed of the blinking.  

Typically, the user would set the intensity to an appropriate level for the hardware and then use commands to trigger output codes.  The Queue depth is typically set to 3 and output codes will follow each other.

## Setting Output Intensity

Intensity is held by an 8 bit values.  We have the ability to easily send four 8 bit values in a task notification (32 bits) but, our hardware only make use of the lowest 3 bytes for RGB color intensities.

bits  7-0  set Red   intensity
bits 15-8  set Green intensity
bits 23-16 set Blue  intensity

## Setting the Code Number(s)

**32 Bit Format:**
MSB byte 1 \<colorA/cycles\>  byte 2 \<colorB/cycles\>  byte 3 \<color time on\>  byte 4 \<dark time off\> LSB

1st byte format for FirstColor is   MSBit  0x<Colors><Cycles>  LSBit  
\<Colors\>   0x1 = ColorA, 0x2 = ColorB, 0x4 = ColorC (3 bits in use here)  
\<Cycles\>   13 possible flashes - 0x01 though 0x0E (1 through 13) Special Command Codes: 0x00 = ON State, 0x0E = AUTO State, 0x0F = OFF State (4 bits in use here)  NOTE: Special Command codes apply to the states of all LEDs in a combination color.  

> [!WARNING]  
>If you use the Speical Command Codes of 0x00 (ON State) or 0x0F (OFF State), you will need to call the Speical Command of 0x0E (AUTO State) to return to normal activity.  

Second byte is the Second Color/Cycles.  
Third byte is on_time -- how long the color is on  
Fourth byte is off_time  -- the time the LEDs are off between cycles.  

The values of on_time and off_time are shared between both possible color sequences.  


>Example1: Red, 1 flash, Green, 2 flashes long on-time long off-time  
>ColorA 0x10, Cycles 0x01, ColorB 0x20 , Cycles 0x02, On-Time 0x20, Off-Time 0x30  
>0x10000000 | 0x01000000 | 0x00200000 | 0x00020000 | 0x00002000 | 0x00000030  
>0x11222030 is our 32 bit value

>Example2: Blue, 3 flashes, NoColor, NoFlashes  short on-time moderate off-time  
>ColorA 0x40, Cycles 0x03, ColorB 0x00 , Cycles 0x00, On-Time 0x01, Off-Time 0x15  
>0x40000000 | 0x03000000 | 0x00000000 | 0x00000000 | 0x00000100 | 0x00000015  
>0x43000115 is our 32 bit value

>Example3: All Colors On continously  
>All Colors - Cycles - NoColor - Cycles On-Time  Off-Time  
>0x7 -------- 0 ------ 0 --------- 0 ----- 00 ------ 00  
>0x70000000 

>Example4: All Colors OFF completely  
>All Colors - Cycles - NoColor - Cycles On-Time  Off-Time  
>0x7 -------- F ------ 0 --------- 0 ----- 00 ------ 00  
>0x7F000000  

>Example5: Return to normal operation  
>All Colors - Cycles - NoColor - Cycles On-Time  Off-Time  
>0x7 -------- E ------ 0 --------- 0 ----- 00 ------ 00  
>0x7E000000  


PLEASE CALL ON THIS SERVICE LIKE THIS:

int32_t val = 0x22420919; // Color1 is Green 2 cycles Color2 is Blue 2 cycles. Off time 09 and On time 19 (25 dec)  

xQueueSendToBack(queHandleINDCmdRequest, &val, 30);


## Abstractions
[Indication Abstraction](./docs/ind_abstractions.md)

## Block Diagrams
[Indication Block Diagrams](./docs/ind_sequences.md)

## Sequence Diagrams
[Indication Sequence Diagrams](./docs/ind_sequences.md)

## State Transition Diagrams
[Indication State Models](./docs/ind_state_models.md)