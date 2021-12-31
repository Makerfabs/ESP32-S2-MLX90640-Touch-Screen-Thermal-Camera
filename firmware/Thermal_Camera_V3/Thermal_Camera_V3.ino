/*
Makerfabs MLX90640 Camera demo
Author  : Vincent
Version : 3.0

Att

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
#define WIFI_MODE

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

#define MINTEMP 25
#define MAXTEMP 37
#define MLX_MIRROR 1 // Set 1 when the camera is facing the screen
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

void setup(void)
{
    pinMode(LCD_CS, OUTPUT);
    pinMode(LCD_BLK, OUTPUT);

    digitalWrite(LCD_CS, LOW);
    digitalWrite(LCD_BLK, HIGH);

    Serial.begin(115200);

#ifdef WIFI_MODE

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.println(F(""));

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(F("."));
    }
    Serial.println(F(""));
    Serial.print(F("Connected to "));
    Serial.println(ssid);
    Serial.print(F("IP address: "));
    Serial.println(WiFi.localIP());

    if (client.connect(host, 80))
    {
        Serial.println(F("Success"));
        client.print(String("GET /") + " HTTP/1.1\r\n" +
                     "Host: " + host + "\r\n" +
                     "Connection: close\r\n" +
                     "\r\n");
    }
#endif

    Serial.println(ESP.getFreeHeap());

    lcd.init();
    lcd.setRotation(0);
    display_ui();

    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    if (SD_init())
    {
        Serial.println(F("SD init error"));
    }
    appendFile(SD, "/temper.txt", "New Begin\n");

    //I2C init
    Wire.begin(I2C_SDA, I2C_SCL);
    byte error, address;

    Wire.beginTransmission(MLX_I2C_ADDR);
    error = Wire.endTransmission();

    if (error == 0)
    {
        Serial.print(F("I2C device found at address 0x"));
        Serial.print(MLX_I2C_ADDR, HEX);
        Serial.println(F("  !"));
    }
    else if (error == 4)
    {
        Serial.print(F("Unknown error at address 0x"));
        Serial.println(MLX_I2C_ADDR, HEX);
    }

    Serial.println(F("Adafruit MLX90640 Simple Test"));
    if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire))
    {
        Serial.println(F("MLX90640 not found!"));
        while (1)
            delay(10);
    }

    //mlx.setMode(MLX90640_INTERLEAVED);
    mlx.setMode(MLX90640_CHESS);
    mlx.setResolution(MLX90640_ADC_18BIT);
    mlx90640_resolution_t res = mlx.getResolution();
    mlx.setRefreshRate(MLX90640_16_HZ);
    mlx90640_refreshrate_t rate = mlx.getRefreshRate();
    Wire.setClock(1000000); // max 1 MHz

    Serial.println(ESP.getFreeHeap());

    //image init
    if (INTERPOLATION_ENABLE == 1)
    {
        inter_p = (uint16_t *)malloc(320 * 240 * sizeof(uint16_t));
        if (inter_p == NULL)
        {
            Serial.println(F("inter_p Malloc error"));
        }

        for (int i = 0; i < 320 * 240; i++)
        {
            *(inter_p + i) = 0x480F;
        }

        Serial.println(ESP.getFreeHeap());
    }

    //frame init
    temp_frame = (float *)malloc(32 * 24 * sizeof(float));
    if (temp_frame == NULL)
    {
        Serial.println(F("temp_frame Malloc error"));
    }

    for (int i = 0; i < 32 * 24; i++)
    {
        temp_frame[i] = MINTEMP;
    }
    Serial.println(ESP.getFreeHeap());

    Serial.println(F("All init over."));
}

uint32_t runtime = 0;
int fps = 0;
float max_temp = 0.0;
int record_index = 0;

void loop()
{
    //获取一帧
    //Get a frame
    if (mlx.getFrame(frame) != 0)
    {
        Serial.println(F("Get frame failed"));
        return;
    }

    //和上次结果平均，滤波
    //Filter temperature data
    filter_frame(frame, temp_frame);

    //快排
    qusort(frame, 0, 32 * 24 - 1);
    max_temp += frame[767];

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
        Serial.println((String) "x=" + pos[0] + ",y=" + pos[1]);
        if (pos[0] > 210 && pos[1] > 400 && pos[0] < 320 && pos[1] < 480)
        {
            touch_flag = 1;
        }
    }

    if ((millis() - runtime) > 2000)
    {
        lcd.fillRect(0, 280, 319, 79, TFT_BLACK);

        lcd.setTextSize(4);
        lcd.setCursor(0, 280);
        lcd.setTextColor(camColors[map_f(max_temp / fps, MINTEMP, MAXTEMP)]);
        lcd.println("  Max Temp:");
        lcd.printf("  %6.1lf C", max_temp / fps);

        lcd.setTextSize(1);
        lcd.setTextColor(TFT_WHITE);
        lcd.setCursor(0, 350);
        lcd.printf("  FPS:%2.1lf", fps / 2.0);

#ifdef WIFI_MODE

        String udp_str = "";
        udp_str = udp_str + "{\"MAXTEMP\":" + max_temp / fps + "}";
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
            sprintf(c, "[%d]\tT:%lf C\n", record_index++, max_temp / fps);
            appendFile(SD, "/temper.txt", c);
        }

        runtime = millis();
        max_temp = 0.0;
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
    for (int i = 0; i < 256; i++)
        lcd.drawFastVLine(32 + i, 255, 20, camColors[i]);

    lcd.setTextSize(2);
    lcd.setCursor(5, 255);
    lcd.println((String) "" + MINTEMP);

    lcd.setCursor(290, 255);
    lcd.println((String) "" + MAXTEMP);

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
        Serial.println(F("Card Mount Failed"));
        return 1;
    }
    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE)
    {
        Serial.println(F("No SD card attached"));
        return 1;
    }

    Serial.print(F("SD Card Type: "));
    if (cardType == CARD_MMC)
    {
        Serial.println(F("MMC"));
    }
    else if (cardType == CARD_SD)
    {
        Serial.println(F("SDSC"));
    }
    else if (cardType == CARD_SDHC)
    {
        Serial.println(F("SDHC"));
    }
    else
    {
        Serial.println(F("UNKNOWN"));
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);
    listDir(SD, "/", 2);

    Serial.println(F("SD init over."));
    return 0;
}

void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if (!root)
    {
        Serial.println(F("Failed to open directory"));
        return;
    }
    if (!root.isDirectory())
    {
        Serial.println(F("Not a directory"));
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            Serial.print(F("  DIR : "));
            Serial.println(file.name());
            if (levels)
            {
                listDir(fs, file.name(), levels - 1);
            }
        }
        else
        {
            Serial.print(F("  FILE: "));
            Serial.print(file.name());
            Serial.print(F("  SIZE: "));
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void readFile(fs::FS &fs, const char *path)
{
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if (!file)
    {
        Serial.println(F("Failed to open file for reading"));
        return;
    }

    Serial.print(F("Read from file: "));
    while (file.available())
    {
        Serial.write(file.read());
    }
    file.close();
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        Serial.println(F("Failed to open file for writing"));
        return;
    }
    if (file.print(message))
    {
        Serial.println(F("File written"));
    }
    else
    {
        Serial.println(F("Write failed"));
    }
    file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message)
{
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if (!file)
    {
        Serial.println(F("Failed to open file for appending"));
        return;
    }
    if (file.print(message))
    {
        Serial.println(F("Message appended"));
    }
    else
    {
        Serial.println(F("Append failed"));
    }
    file.close();
}

#ifdef WIFI_MODE

void send_udp(String msg_json)
{
    Serial.println(F("Prepare send UDP"));
    if (client.connected())
    {
        client.print(msg_json);
        Serial.println(F("UDP send"));
    }
}
#endif
