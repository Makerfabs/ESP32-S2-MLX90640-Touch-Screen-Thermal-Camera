/*
Makerfabs MLX90640 Camera demo
Author  : Vincent
Version : 3.1

        V3.1: Added camera spot repair function.



使用 1.1.9  版本的库 LovyanGFX 在文件夹： C:\Users\maker\Documents\Arduino\libraries\LovyanGFX 
使用 2.0.0  版本的库 Wire 在文件夹： C:\Users\maker\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.11\libraries\Wire 
使用 1.0.4  版本的库 Adafruit_MLX90640 在文件夹： C:\Users\maker\Documents\Arduino\libraries\Adafruit_MLX90640 
使用 1.14.4  版本的库 Adafruit_BusIO 在文件夹： C:\Users\maker\Documents\Arduino\libraries\Adafruit_BusIO 
使用 2.0.0  版本的库 FS 在文件夹： C:\Users\maker\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.11\libraries\FS 
使用 2.0.0  版本的库 SD 在文件夹： C:\Users\maker\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.11\libraries\SD 
使用 2.0.0  版本的库 SPI 在文件夹： C:\Users\maker\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.11\libraries\SPI 



*/

#define LGFX_USE_V1

#include <LovyanGFX.hpp>
#include <Wire.h>
#include <Adafruit_MLX90640.h>
#include "color.h"
#include "FT6236.h"
#include "lcd_set.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"

//Choice local mode or wifi mode
//#define WIFI_MODE

#ifdef WIFI_MODE

#include <WiFi.h>
const char *ssid = "Makerfabs";
const char *password = "20160704";
const char *host = "192.168.1.29";
WiFiClient client;

#endif

LGFX lcd;

#define SPI_MOSI 2 //1
#define SPI_MISO 41
#define SPI_SCK 42
#define SD_CS 1 //2

#define I2C_SCL 39
#define I2C_SDA 38

#define MLX_I2C_ADDR 0x33
Adafruit_MLX90640 mlx;

const int i2c_touch_addr = TOUCH_I2C_ADD;
#define get_pos ft6236_pos
int touch_flag = 0;
int pos[2] = {0, 0};

#define UI_REFRESH_SECONDS 1

#define FAHRENHEIT 1

//#define MINTEMP 28
//#define MAXTEMP 37
float MINTEMP = 10;
float MAXTEMP = 49;
#define DYNAMIC_RANGE 1
#define DYNAMIC_RANGE_PADDING 0
#define MLX_MIRROR 0 // Set 1 when the camera is facing the screen
#define FILTER_ENABLE 1

//Interploation need a lot ram, can't work in wifi mode
#ifndef WIFI_MODE
#define INTERPOLATION_ENABLE 1
#else
#define INTERPOLATION_ENABLE 0
#endif

float frame[32 * 24]; // buffer for full frame of temperatures
float *temp_frame = NULL;
uint16_t *inter_p = NULL;

float convertTemp(float c) {
  #ifdef FAHRENHEIT
    c = (c * 9/5) + 32;
  #endif
  return c;
}

void setup(void)
{
    pinMode(LCD_CS, OUTPUT);
    pinMode(LCD_BLK, OUTPUT);

    digitalWrite(LCD_CS, LOW);
    digitalWrite(LCD_BLK, HIGH);

    USBSerial.begin(115200);

#ifdef WIFI_MODE

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    USBSerial.println(F(""));

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        USBSerial.print(F("."));
    }
    USBSerial.println(F(""));
    USBSerial.print(F("Connected to "));
    USBSerial.println(ssid);
    USBSerial.print(F("IP address: "));
    USBSerial.println(WiFi.localIP());

    if (client.connect(host, 80))
    {
        USBSerial.println(F("Success"));
        client.print(String("GET /") + " HTTP/1.1\r\n" +
                     "Host: " + host + "\r\n" +
                     "Connection: close\r\n" +
                     "\r\n");
    }
#endif

    USBSerial.println(ESP.getFreeHeap());

    lcd.init();
    lcd.setRotation(0);
    display_ui();

    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    if (SD_init())
    {
        USBSerial.println(F("SD init error"));
    }
    appendFile(SD, "/temper.txt", "New Begin\n");

    //I2C init
    Wire.begin(I2C_SDA, I2C_SCL);
    byte error, address;

    Wire.beginTransmission(MLX_I2C_ADDR);
    error = Wire.endTransmission();

    if (error == 0)
    {
        USBSerial.print(F("I2C device found at address 0x"));
        USBSerial.print(MLX_I2C_ADDR, HEX);
        USBSerial.println(F("  !"));
    }
    else if (error == 4)
    {
        USBSerial.print(F("Unknown error at address 0x"));
        USBSerial.println(MLX_I2C_ADDR, HEX);
    }

    USBSerial.println(F("Adafruit MLX90640 Simple Test"));
    if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire))
    {
        USBSerial.println(F("MLX90640 not found!"));
        while (1)
            delay(10);
    }

    // mlx.setMode(MLX90640_INTERLEAVED);
    mlx.setMode(MLX90640_CHESS);
    mlx.setResolution(MLX90640_ADC_18BIT);
    mlx90640_resolution_t res = mlx.getResolution();
    mlx.setRefreshRate(MLX90640_16_HZ);
    mlx90640_refreshrate_t rate = mlx.getRefreshRate();
    Wire.setClock(1000000); // max 1 MHz

    USBSerial.println(ESP.getFreeHeap());

    //image init
    if (INTERPOLATION_ENABLE == 1)
    {
        inter_p = (uint16_t *)malloc(320 * 240 * sizeof(uint16_t));
        if (inter_p == NULL)
        {
            USBSerial.println(F("inter_p Malloc error"));
        }

        for (int i = 0; i < 320 * 240; i++)
        {
            *(inter_p + i) = 0x480F;
        }

        USBSerial.println(ESP.getFreeHeap());
    }

    //frame init
    temp_frame = (float *)malloc(32 * 24 * sizeof(float));
    if (temp_frame == NULL)
    {
        USBSerial.println(F("temp_frame Malloc error"));
    }

    for (int i = 0; i < 32 * 24; i++)
    {
        temp_frame[i] = MINTEMP;
    }
    USBSerial.println(ESP.getFreeHeap());

    USBSerial.println(F("All init over."));
}

uint32_t runtime = 0;
int fps = 0;
float avg_max_temp = 0.0;
float avg_min_temp = 0.0;
int record_index = 0;

void loop()
{
    //获取一帧
    //Get a frame
    if (mlx.getFrame(frame) != 0)
    {
        USBSerial.println(F("Get frame failed"));
        return;
    }

    bug_fix(frame);

    //和上次结果平均，滤波
    //Filter temperature data
    filter_frame(frame, temp_frame);

    //快排
    qusort(frame, 0, 32 * 24 - 1);
    avg_max_temp += frame[767];
    avg_min_temp += frame[1];

    if(DYNAMIC_RANGE == 1) {
      //if ((millis() - runtime) > UI_REFRESH_SECONDS * 1000) {
        MINTEMP = frame[1];
        MAXTEMP = frame[767];

        MINTEMP -= DYNAMIC_RANGE_PADDING;
        MAXTEMP += DYNAMIC_RANGE_PADDING;
      //}
    }

    if (INTERPOLATION_ENABLE == 1)
    {
        //温度矩阵转换图像矩阵，将32*24插值到320*240
        //Display with 320*240 pixel
        interpolation(temp_frame, inter_p);
        lcd.pushImage(0, 0, 320, 240, (lgfx::rgb565_t *)inter_p);
    }
    else
    {
        //Display with 32*24 pixel
        for (uint8_t h = 0; h < 24; h++)
        {
            for (uint8_t w = 0; w < 32; w++)
            {
                uint8_t colorIndex = map_f(temp_frame[h * 32 + w], MINTEMP, MAXTEMP);
                lcd.fillRect(10 * w, 10 * h, 10, 10, camColors[colorIndex]);
            }
        }
    }

    fps++;

    if (get_pos(pos))
    {
        // USBSerial.println((String) "x=" + pos[0] + ",y=" + pos[1]);
        if (pos[0] > 210 && pos[1] > 400 && pos[0] < 320 && pos[1] < 480)
        {
            touch_flag = 1;
        }
    }

    if ((millis() - runtime) > UI_REFRESH_SECONDS * 1000)
    {
    	  display_ui();
        lcd.fillRect(0, 280, 319, 79, TFT_BLACK);
        lcd.fillRect(0, 360, 150, 150, TFT_BLACK);

        lcd.setCursor(0, 300);
        
        if(DYNAMIC_RANGE != 1) {
          lcd.setTextColor(camColors[map_f(avg_min_temp / fps, MINTEMP, MAXTEMP)]);
        }
        lcd.printf("Min temp: %6.1lf F", convertTemp(avg_min_temp / fps));  

        lcd.setTextSize(2);
        lcd.setCursor(0, 280);
        if(DYNAMIC_RANGE != 1) {
          lcd.setTextColor(camColors[map_f(avg_max_temp / fps, MINTEMP, MAXTEMP)]);
        }
        lcd.printf("Max temp: %6.1lf F", convertTemp(avg_max_temp / fps));

        lcd.setTextSize(1);
        lcd.setTextColor(TFT_WHITE);
        lcd.setCursor(0, 350);
        lcd.printf("  FPS:%2.1lf", fps / UI_REFRESH_SECONDS);

        lcd.setCursor(0, 360);
        lcd.printf("  frame[1]:%2.1lf", convertTemp(frame[1]));

        lcd.setCursor(0, 370);
        lcd.printf("  frame[767]:%2.1lf", convertTemp(frame[767]));

        lcd.setCursor(0, 380);
        lcd.printf("  MINTEMP:%2.1lf", convertTemp(MINTEMP));

        lcd.setCursor(0, 390);
        lcd.printf("  MAXTEMP:%2.1lf", convertTemp(MAXTEMP));

#ifdef WIFI_MODE

        String udp_str = "";
        udp_str = udp_str + "{\"MAXTEMP\":" + avg_max_temp / fps + ", \"MINTEMP\":" + avg_min_temp / fps + "}";
        send_udp(udp_str);

#endif

        if (touch_flag == 1)
        {
            touch_flag = 0;

            lcd.fillRect(0, 380, 220, 80, TFT_BLACK);
            lcd.setTextColor(TFT_WHITE);
            lcd.setTextSize(1);
            lcd.setCursor(10, 390);
            lcd.printf("Record NO.%d to SD", record_index);

            //Save Max temperture to sd card
            char c[20];
            sprintf(c, "[%d]\tT:%lf C\n", record_index++, avg_max_temp / fps);
            appendFile(SD, "/temper.txt", c);
        }

        runtime = millis();
        avg_max_temp = 0.0;
        avg_min_temp = 0.0;
        fps = 0;
    }
}

//消抖并翻转
//Filter temperature data and change camera direction
void filter_frame(float *in, float *out)
{
    if (MLX_MIRROR == 1)
    {
        for (int i = 0; i < 32 * 24; i++)
        {
            if (FILTER_ENABLE == 1)
                out[i] = (out[i] + in[i]) / 2;
            else
                out[i] = in[i];
        }
    }
    else
    {
        for (int i = 0; i < 24; i++)
            for (int j = 0; j < 32; j++)
            {
                if (FILTER_ENABLE == 1)
                    out[32 * i + 31 - j] = (out[32 * i + 31 - j] + in[32 * i + j]) / 2;
                else
                    out[32 * i + 31 - j] = in[32 * i + j];
            }
    }
}

//32*24插值10倍到320*240
//Transform 32*24 to 320 * 240 pixel
void interpolation(float *data, uint16_t *out)
{

    for (uint8_t h = 0; h < 24; h++)
    {
        for (uint8_t w = 0; w < 32; w++)
        {
            out[h * 10 * 320 + w * 10] = map_f(data[h * 32 + w], MINTEMP, MAXTEMP);
        }
    }
    for (int h = 0; h < 240; h += 10)
    {
        for (int w = 1; w < 310; w += 10)
        {
            for (int i = 0; i < 9; i++)
            {
                out[h * 320 + w + i] = (out[h * 320 + w - 1] * (9 - i) + out[h * 320 + w + 9] * (i + 1)) / 10;
            }
        }
        for (int i = 0; i < 9; i++)
        {
            out[h * 320 + 311 + i] = out[h * 320 + 310];
        }
    }
    for (int w = 0; w < 320; w++)
    {
        for (int h = 1; h < 230; h += 10)
        {
            for (int i = 0; i < 9; i++)
            {
                out[(h + i) * 320 + w] = (out[(h - 1) * 320 + w] * (9 - i) + out[(h + 9) * 320 + w] * (i + 1)) / 10;
            }
        }
        for (int i = 0; i < 9; i++)
        {
            out[(231 + i) * 320 + w] = out[230 * 320 + w];
        }
    }
    for (int h = 0; h < 240; h++)
    {
        for (int w = 0; w < 320; w++)
        {
            out[h * 320 + w] = camColors[out[h * 320 + w]];
        }
    }
}

//float to 0,255
int map_f(float in, float a, float b)
{
    if (in < a)
        return 0;

    if (in > b)
        return 255;

    return (int)(in - a) * 255 / (b - a);
}

//Quick sort
void qusort(float s[], int start, int end) //自定义函数 qusort()
{
    int i, j;        //定义变量为基本整型
    i = start;       //将每组首个元素赋给i
    j = end;         //将每组末尾元素赋给j
    s[0] = s[start]; //设置基准值
    while (i < j)
    {
        while (i < j && s[0] < s[j])
            j--; //位置左移
        if (i < j)
        {
            s[i] = s[j]; //将s[j]放到s[i]的位置上
            i++;         //位置右移
        }
        while (i < j && s[i] <= s[0])
            i++; //位置左移
        if (i < j)
        {
            s[j] = s[i]; //将大于基准值的s[j]放到s[i]位置
            j--;         //位置左移
        }
    }
    s[i] = s[0]; //将基准值放入指定位置
    if (start < i)
        qusort(s, start, j - 1); //对分割出的部分递归调用qusort()函数
    if (i < end)
        qusort(s, j + 1, end);
}

void display_ui()
{
	  lcd.fillRect(0, 255, 320, 50, TFT_BLACK);
    for (int i = 0; i < 256; i++)
        lcd.drawFastVLine(32 + i, 255, 20, camColors[i]);

    lcd.setTextSize(2);
    lcd.setCursor(5, 255);
    //lcd.println((String) "" + MINTEMP);
    lcd.printf("%2.0lf", convertTemp(MINTEMP));

    lcd.setCursor(290, 255);
    //lcd.println((String) "" + MAXTEMP);
    lcd.printf("%2.0lf", convertTemp(MAXTEMP));

    lcd.fillRect(220, 380, 80, 80, TFT_GREEN);
    lcd.setCursor(230, 390);
    lcd.println("SAVE");

#ifdef WIFI_MODE
    lcd.setTextSize(1);
    lcd.setCursor(10, 460);
    lcd.println(WiFi.localIP());
#endif
}

int SD_init()
{

    if (!SD.begin(SD_CS))
    {
        USBSerial.println(F("Card Mount Failed"));
        return 1;
    }
    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE)
    {
        USBSerial.println(F("No SD card attached"));
        return 1;
    }

    USBSerial.print(F("SD Card Type: "));
    if (cardType == CARD_MMC)
    {
        USBSerial.println(F("MMC"));
    }
    else if (cardType == CARD_SD)
    {
        USBSerial.println(F("SDSC"));
    }
    else if (cardType == CARD_SDHC)
    {
        USBSerial.println(F("SDHC"));
    }
    else
    {
        USBSerial.println(F("UNKNOWN"));
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    USBSerial.printf("SD Card Size: %lluMB\n", cardSize);
    listDir(SD, "/", 2);

    USBSerial.println(F("SD init over."));
    return 0;
}

void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
    USBSerial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if (!root)
    {
        USBSerial.println(F("Failed to open directory"));
        return;
    }
    if (!root.isDirectory())
    {
        USBSerial.println(F("Not a directory"));
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            USBSerial.print(F("  DIR : "));
            USBSerial.println(file.name());
            if (levels)
            {
                listDir(fs, file.name(), levels - 1);
            }
        }
        else
        {
            USBSerial.print(F("  FILE: "));
            USBSerial.print(file.name());
            USBSerial.print(F("  SIZE: "));
            USBSerial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void readFile(fs::FS &fs, const char *path)
{
    USBSerial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if (!file)
    {
        USBSerial.println(F("Failed to open file for reading"));
        return;
    }

    USBSerial.print(F("Read from file: "));
    while (file.available())
    {
        USBSerial.write(file.read());
    }
    file.close();
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
    USBSerial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        USBSerial.println(F("Failed to open file for writing"));
        return;
    }
    if (file.print(message))
    {
        USBSerial.println(F("File written"));
    }
    else
    {
        USBSerial.println(F("Write failed"));
    }
    file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message)
{
    USBSerial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if (!file)
    {
        USBSerial.println(F("Failed to open file for appending"));
        return;
    }
    if (file.print(message))
    {
        USBSerial.println(F("Message appended"));
    }
    else
    {
        USBSerial.println(F("Append failed"));
    }
    file.close();
}

#ifdef WIFI_MODE

void send_udp(String msg_json)
{
    USBSerial.println(F("Prepare send UDP"));
    if (client.connected())
    {
        client.print(msg_json);
        USBSerial.println(F("UDP send"));
    }
}
#endif


// The camera allows up to four non-consecutive bad points with a value of nan.
void bug_fix(float *frame)
{
    for (int i = 0; i < 768 - 1; i++)
    {
        if (isnan(frame[i]))
            frame[i] = frame[i + 1];
    }
}