# **Changes**

This version of the driver firmware introduces several important updates:

- Added **PID control** to the `G` (Go-To) commands.  
  Motors now accelerate, cruise, and decelerate smoothly to the target position at the requested RPM.  
- Removed **current sensing** functionality.  
  The ADC code for reading motor current has been disabled to simplify the firmware.

---

## Command Overview

The firmware supports a set of commands, each identified by a single letter.  
My rule of thumb for letter assignments is:

- **A** → Address (I²C related)  
- **B** → Basic Movement (direct PWM control)  
- **C** → Change Setting (configuration)  
- **F** → Flag (true/false values)  
- **G** → Go-To Movement (PID controlled position moves)  
- **M** → Motor selection (A, B, or both)  
- **N** → Number (encoder counts, etc.)  
- **P** → Position (absolute target position)  
- **R** → Rate (acceleration/deceleration)  
- **S** → Speed (RPM in Go-To mode, PWM in Basic mode)  

---

## Command Details

### `B` — Basic Movement
- `B0` → Stop motor  
- `B1` → Forward  
- `B2` → Reverse  
- Speed is set with `S` (0–1000 = PWM duty cycle)

### `G` — Go-To Movement
- `G00` → Stop  
- `G01 P<value> S<value>` → Move to absolute position at given speed (RPM)  
- Example: `G01 P1000 S50 M1` → Move Motor A to position 1000 at 50 RPM

### `C` — Change Settings
- `C0` → Reset positions to zero  
- `C1 P<value>` → Set current position  
- `C6 F<value>` → Enable/disable station flag  
- `C7 P<value>` → Set forward station position  
- `C8 P<value>` → Set reverse station position  
- `C10 A<value>` → Change I²C address  
- `C13 F<value>` → Set I²C speed (100k/400k)  
- `C14 R<value>` → Set acceleration rate  
- `C15 N<value>` → Set encoder count  

### `S` — Speed
- In **Basic mode (`B`)**: 0–1000 = PWM duty cycle  
- In **Go-To mode (`G`)**: 0–1000 = RPM  

### `M` — Motor Selection
- `M1` → Motor A  
- `M2` → Motor B  
- `M3` → Both motors  

---
## Example Commands

### **'G' Command**
- `G01 P2000 S500 M1\r\n`  
  Move **Motor A** to **Position 2000** at **500 RPM**.  
  *Note: `P` (Position) can be a negative **int32_t** value.*
  
### **'B' Command**
- `B1 S750 M2\r\n`  
  Run **Motor B** forward at **75% PWM duty cycle**.  
- `B0 M3\r\n`  
  Stop **Motors A and B**.


---

## Notes on Command Transmission

All commands should:
- Be sent to the I²C address defined in **Settings**.
- **End** with `\r\n`.
- **A garbage Char should be sent befor the command**
- `#` (or any single character — it is ignored due to an ESP8266 I²C bug).  

### Example: Sending a Command Buffer

```c
int Target_I2C_Address = 0x30;
char Buffer_TX[BUFF_64];    /* message */

void SendBufferOnI2C(int I2C_address) {
    Wire.beginTransmission(I2C_address);   // Get device at I2C_address attention
    Wire.write('#');                       /* Send garbage for the first byte (ignored) */
    Wire.write(Buffer_TX, sofar);          // Send Buffer_TX
    Wire.endTransmission();                // Stop transmitting
}
