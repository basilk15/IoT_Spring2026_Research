# ESP32 Memory Stats

## Program Storage (Flash Memory)

- Used: 1,063,316 bytes
- Maximum: 1,310,720 bytes
- Percentage: 81%

This is where your actual code (the sketch + libraries) is stored.

What it means:

- You still have about 19% free space (~247 KB).
- That is safe, but getting a bit on the higher side.
- If you keep adding large libraries or features, you could run out.

## Dynamic Memory (RAM)

- Used (global/static variables): 51,088 bytes
- Available for local variables (stack/heap): 276,592 bytes
- Maximum: 327,680 bytes
- Percentage used: 15%

This is the memory used while the program is running.

What it means:

- You have plenty of free RAM (~276 KB).
- This is very healthy - no immediate risk of crashes from memory.
