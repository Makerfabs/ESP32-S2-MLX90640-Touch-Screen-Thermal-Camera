

#include <Wire.h>
#include <Adafruit_MLX90640.h>
#include <math.h>

#define I2C_SCL 39
#define I2C_SDA 38

#define MLX_I2C_ADDR 0x33

Adafruit_MLX90640 mlx;
float frame[32 * 24]; // buffer for full frame of temperatures

// uncomment *one* of the below
#define PRINT_TEMPERATURES
// #define PRINT_ASCIIART

void setup(void)
{
    USBSerial.begin(115200);

    Wire.begin(I2C_SDA, I2C_SCL);
    byte error, address;

    Wire.beginTransmission(MLX_I2C_ADDR);
    error = Wire.endTransmission();

    if (error == 0)
    {
        USBSerial.print("I2C device found at address 0x");
        USBSerial.print(MLX_I2C_ADDR, HEX);
        USBSerial.println("  !");
    }
    else if (error == 4)
    {
        USBSerial.print("Unknown error at address 0x");
        USBSerial.println(MLX_I2C_ADDR, HEX);
    }

    USBSerial.println("Adafruit MLX90640 Simple Test");
    if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire))
    {
        USBSerial.println("MLX90640 not found!");
        while (1)
            delay(10);
    }
    USBSerial.println("Found Adafruit MLX90640");

    USBSerial.print("USBSerial number: ");
    USBSerial.print(mlx.serialNumber[0], HEX);
    USBSerial.print(mlx.serialNumber[1], HEX);
    USBSerial.println(mlx.serialNumber[2], HEX);

    // mlx.setMode(MLX90640_INTERLEAVED);
    mlx.setMode(MLX90640_CHESS);
    USBSerial.print("Current mode: ");
    if (mlx.getMode() == MLX90640_CHESS)
    {
        USBSerial.println("Chess");
    }
    else
    {
        USBSerial.println("Interleave");
    }

    mlx.setResolution(MLX90640_ADC_18BIT);
    USBSerial.print("Current resolution: ");
    mlx90640_resolution_t res = mlx.getResolution();
    switch (res)
    {
    case MLX90640_ADC_16BIT:
        USBSerial.println("16 bit");
        break;
    case MLX90640_ADC_17BIT:
        USBSerial.println("17 bit");
        break;
    case MLX90640_ADC_18BIT:
        USBSerial.println("18 bit");
        break;
    case MLX90640_ADC_19BIT:
        USBSerial.println("19 bit");
        break;
    }

    mlx.setRefreshRate(MLX90640_2_HZ);
    USBSerial.print("Current frame rate: ");
    mlx90640_refreshrate_t rate = mlx.getRefreshRate();
    switch (rate)
    {
    case MLX90640_0_5_HZ:
        USBSerial.println("0.5 Hz");
        break;
    case MLX90640_1_HZ:
        USBSerial.println("1 Hz");
        break;
    case MLX90640_2_HZ:
        USBSerial.println("2 Hz");
        break;
    case MLX90640_4_HZ:
        USBSerial.println("4 Hz");
        break;
    case MLX90640_8_HZ:
        USBSerial.println("8 Hz");
        break;
    case MLX90640_16_HZ:
        USBSerial.println("16 Hz");
        break;
    case MLX90640_32_HZ:
        USBSerial.println("32 Hz");
        break;
    case MLX90640_64_HZ:
        USBSerial.println("64 Hz");
        break;
    }
}

void loop()
{
    delay(500);
    if (mlx.getFrame(frame) != 0)
    {
        USBSerial.println("Failed");
        return;
    }

    bug_fix(frame);
    USBSerial.println();
    USBSerial.println();
    for (uint8_t h = 0; h < 24; h++)
    {
        for (uint8_t w = 0; w < 32; w++)
        {
            float t = frame[h * 32 + w];
#ifdef PRINT_TEMPERATURES
            // USBSerial.print(t, 1);
            // USBSerial.print(", ");

            char c[10];
            sprintf(c, "%.1f,", t);

            USBSerial.print(c);
            // USBSerial.print(", ");
#endif
#ifdef PRINT_ASCIIART
            char c = '&';
            if (t < 20)
                c = ' ';
            else if (t < 23)
                c = '.';
            else if (t < 25)
                c = '-';
            else if (t < 27)
                c = '*';
            else if (t < 29)
                c = '+';
            else if (t < 31)
                c = 'x';
            else if (t < 33)
                c = '%';
            else if (t < 35)
                c = '#';
            else if (t < 37)
                c = 'X';
            USBSerial.print(c);
#endif
        }
        USBSerial.println();
    }
}

void bug_fix(float *frame)
{
    for (int i = 0; i < 768 - 1; i++)
    {
        if (isnan(frame[i]))
            frame[i] = frame[i + 1];
    }
}