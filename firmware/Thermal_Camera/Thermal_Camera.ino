//最终界面
#define LGFX_USE_V1

#include <LovyanGFX.hpp>
#include <Wire.h>
#include <Adafruit_MLX90640.h>
#include "color.h"

#include "lcd_set.h"
LGFX lcd;

#define I2C_SCL 39
#define I2C_SDA 38

#define MLX_I2C_ADDR 0x33

Adafruit_MLX90640 mlx;

#define MINTEMP 25
#define MAXTEMP 37

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
    lcd.init();
    lcd.setRotation(0);

    //I2C init
    Wire.begin(I2C_SDA, I2C_SCL);
    byte error, address;

    Wire.beginTransmission(MLX_I2C_ADDR);
    error = Wire.endTransmission();

    if (error == 0)
    {
        Serial.print("I2C device found at address 0x");
        Serial.print(MLX_I2C_ADDR, HEX);
        Serial.println("  !");
    }
    else if (error == 4)
    {
        Serial.print("Unknown error at address 0x");
        Serial.println(MLX_I2C_ADDR, HEX);
    }

    Serial.println("Adafruit MLX90640 Simple Test");
    if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire))
    {
        Serial.println("MLX90640 not found!");
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

    //image init
    inter_p = (uint16_t *)malloc(320 * 240 * sizeof(uint16_t));
    if (inter_p == NULL)
    {
        Serial.println("Malloc error");
    }

    for (int i = 0; i < 320 * 240; i++)
    {
        *(inter_p + i) = 0x480F;
    }

    //frame init
    temp_frame = (float *)malloc(32 * 24 * sizeof(float));
    if (inter_p == NULL)
    {
        Serial.println("Malloc error");
    }

    for (int i = 0; i < 32 * 24; i++)
    {
        temp_frame[i] = MINTEMP;
    }

    Serial.println("All init over.");

    display_ui();
}

uint32_t runtime = 0;
int fps = 0;
float max_temp = 0.0;

void loop()
{
    //获取一帧
    if (mlx.getFrame(frame) != 0)
    {
        Serial.println("Get frame failed");
        return;
    }

    //和上次结果平均，滤波
    filter_frame(frame, temp_frame);

    //快排
    qusort(frame, 0, 32 * 24 - 1);
    max_temp += frame[767];

    //温度矩阵转换图像矩阵，将32*24插值到320*240
    interpolation(temp_frame, inter_p);
    lcd.pushImage(0, 0, 320, 240, (lgfx::rgb565_t *)inter_p);

    fps++;

    if ((millis() - runtime) > 2000)
    {
        lcd.fillRect(0, 330, 319, 149, TFT_BLACK);

        lcd.setTextSize(4);
        lcd.setCursor(0, 330);
        lcd.setTextColor(camColors[map_f(max_temp / fps, MINTEMP, MAXTEMP)]);
        lcd.println("  Max Temp:");
        lcd.printf("  %6.1lf C", max_temp / fps);

        lcd.setTextSize(1);
        lcd.setTextColor(TFT_WHITE);
        lcd.setCursor(0, 440);
        lcd.println("  FPS:");
        lcd.printf("  %6.1lf", fps / 2.0);

        runtime = millis();
        max_temp = 0.0;
        fps = 0;
    }
}

//消抖
void filter_frame(float *in, float *out)
{
    for (int i = 0; i < 32 * 24; i++)
    {
        out[i] = (out[i] + in[i]) / 2;
    }
}

//32*24插值10倍到320*240
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
        lcd.drawFastVLine(32 + i, 260, 20, camColors[i]); // 垂直線

    lcd.setTextSize(2);
    lcd.setCursor(10, 290);
    lcd.println((String) "" + MINTEMP);

    lcd.setTextSize(2);
    lcd.setCursor(280, 290);
    lcd.println((String) "" + MAXTEMP);
}