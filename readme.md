DeskCube
========

Display scrolling text and icons on an 8x8 LED matrix using a serial protocoll.

Hardware
--------

- Board: Arduino Nano
- Processor: ATmega328
- a 8x8 LED matrix (MAX7219 or MAX7221 driver)

Wiring
------

    A.+5v <-> LCD.VCC
    A.GND <-> LCD.GND
    A.D12 <-> LCD.DIN
    A.D11 <-> LCD.CLK  ! crossed vvv
    A.D10 <-> LCD.CS   ! crossed ^^^

Serial Commands
---------------

    0                  Idle
    s<text>            Scroll text
    l<slot><bytes>     Load 8 bytes into slot
    h<slot><char>      Load character into slot
    i<action><icon*>   Show/hide icon(s) (action can be '+' or '-';
                         icon can be number or '*' for all)
    f<bytes>           Show frame using 8 byte values
    F<char>...<char>   Show frame using 8x8 chars ('x' is on ' ' is off)
    C<char>            Show character
    c                  Clear display
    b<action>          Turn blinking on or off (action '+' is on; '-' is off)

Example Graphics
----------------

**Balloon**

    ...........
     xxxxxx
    x    xxx
    xx  xxxx
     xxxxx  
       x    
        x    
      x     
       x    
    ...........

    F xxxxxx x    xxxxx  xxxx xxxxxx    x        x      x        x   
    -1234567812345678123456781234567812345678123456781234567812345678
    -        +       +       +       +       +       +       +
