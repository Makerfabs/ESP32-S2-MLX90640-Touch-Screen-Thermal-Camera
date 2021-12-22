# ESP32-S2 MLX90640 Touch Screen Thermal Camera 

```c++
/*
Version:		V1.0
Author:			Vincent
Create Date:	2021/12/3
Note:

	2021/12/22	V2.0: Added a modular kit and has an acrylic case.
	
*/
```
![](md_pic/main.jpg)


[toc]

# Makerfabs

[Makerfabs home page](https://www.makerfabs.com/)

[Makerfabs Wiki](https://makerfabs.com/wiki/index.php?title=Main_Page)

# ESP32-S2 MLX90640 Touch Screen Thermal Camera
## Intruduce

Product Link ：[]() 

Wiki Link : []() 

Based on Makefabs ESP32-S2 Parallel TFT, Mabee MLX90640(Thermal imaging camera). This is a thermal imaging camera kit, contains a laser-cut acrylic case. 

It can realize non-contact remote temperature measurement and fault detection. The advantage is that it is simple and intuitive, showing temperature differences across the entire region. It can also detect the movement of living things with no light at all.

Or just checking the temperature of your coffee.



# Item List

- [Makerfabs ESP32-S2 Parallel TFT with Touch](https://github.com/Makerfabs/Makerfabs-ESP32-S2-Parallel-TFT-with-Touch)
- Mabee MLX90640 


### Front:

![front](md_pic/front.jpg)

### Back:
![back](md_pic/back.jpg)


# Code


## Compiler Options

**If you have any questions，such as how to install the development board, how to download the code, how to install the library. Please refer to :[Makerfabs_FAQ](https://github.com/Makerfabs/Makerfabs_FAQ)**

- Install board : ESP32 .
- Install library : LovyanGFX
- Install library : Adafruit_MLX90640

For detailed burning method, please refer to [Makerfabs ESP32-S2 Parallel TFT with Touch](https://github.com/Makerfabs/Makerfabs-ESP32-S2-Parallel-TFT-with-Touch)

## Firmware

### Thermal_Camera V2

Based on ESP32-S2, ILI9488 screen, ft6236 touch drive, MLX90640 sensor is a thermal imaging camera demo. 

Temperature images can be displayed on the screen.  The highest temperature in the range will be displayed. 

Data can be stored to an SD card by pressing buttons on the touch screen.

Added macro definition of camera orientation, which can be combined with the hole in the acrylic case to choose whether the camera faces the screen or the other side

![firmware](md_pic/firmware.jpg)

If you want change MLX90640 direction. Default is 1.

```c++
#define MLX_MIRROR 0 // Set 1 when the camera is facing the screen
```



### Thermal_Camera

Based on ESP32-S2, ILI9488 screen, ft6236 touch drive, MLX90640 sensor is a thermal imaging camera demo. 

Temperature images can be displayed on the screen.



## Example

### MLX1

Simply obtain the temperature matrix of MLX90640 transmission. And  display via serial port.

