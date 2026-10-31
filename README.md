# pic32_mp3_decoder
PIC32 Olimex Arduino MP3 decoder with VS1053 chip (Helix SW decoder is inside but does not work I think)

Put a file named AVA.MP3 on the SD Card. This will be played by default is something is going wrong.

## Buttons
The buttons are somehow weird. I was not able to find normal push buttons, so they are now a toggle button. One press it remains closed. Another press it will be opened. Dont know how this type is called. So the SW is made to handle this.
4 buttons. One Mode Selector

| Volume Mode | Play Mode   | Olimex Pinguino |
|-------------|-------------|-----------------|
| Volume Up   | Prev        | RB1             |
| N/A         | Play/Pause  | RB2             |
| Volume Down | Next        | RB3             |
| Mode select | Mode select | RB4             |


## VS1053 pinout

This is a MP3 shiled for Arduino. It have a VS1053 chip and an SD Card holder. The one on the PIC32 Olimex board is not used.

| VS1053      | Arduino     | Olimex Pinguino |
|-------------|-------------|-----------------|
| SCK         | D13         | RG6 SCK2        |
| MISO        | D12         | RG7 DSI2        |
| MOSI        | D11         | RG8 DSO2        |
| CS          | D9          | RB14            |
| XRESET      | D8          | RB13            |
| XDCS        | D7          | RD11            |
| XCS         | D6          | RD8             |
| DREQ        | D2          | RD4             |

