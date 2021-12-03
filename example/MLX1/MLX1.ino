//Serial Test
#define LGFX_USE_V1

#include <LovyanGFX.hpp>
#include <Wire.h>
#include <Adafruit_MLX90640.h>

#define I2C_SCL 39
#define I2C_SDA 38

#define MLX_I2C_ADDR 0x33

#define LCD_CS 37
#define LCD_BLK 45

class LGFX : public lgfx::LGFX_Device
{
    //lgfx::Panel_ILI9341 _panel_instance;
    lgfx::Panel_ILI9488 _panel_instance;
    lgfx::Bus_Parallel16 _bus_instance; // 8ビットパラレルバスのインスタンス (ESP32のみ)

public:
    // コンストラクタを作成し、ここで各種設定を行います。
    // クラス名を変更した場合はコンストラクタも同じ名前を指定してください。
    LGFX(void)
    {
        {                                      // バス制御の設定を行います。
            auto cfg = _bus_instance.config(); // バス設定用の構造体を取得します。

            // 16位设置
            cfg.i2s_port = I2S_NUM_0;  // 使用するI2Sポートを選択 (0 or 1) (ESP32のI2S LCDモードを使用します)
            cfg.freq_write = 16000000; // 送信クロック (最大20MHz, 80MHzを整数で割った値に丸められます)
            cfg.pin_wr = 35;           // WR を接続しているピン番号
            cfg.pin_rd = 34;           // RD を接続しているピン番号
            cfg.pin_rs = 36;           // RS(D/C)を接続しているピン番号

            cfg.pin_d0 = 33;
            cfg.pin_d1 = 21;
            cfg.pin_d2 = 14;
            cfg.pin_d3 = 13;
            cfg.pin_d4 = 12;
            cfg.pin_d5 = 11;
            cfg.pin_d6 = 10;
            cfg.pin_d7 = 9;
            cfg.pin_d8 = 3;
            cfg.pin_d9 = 8;
            cfg.pin_d10 = 16;
            cfg.pin_d11 = 15;
            cfg.pin_d12 = 7;
            cfg.pin_d13 = 6;
            cfg.pin_d14 = 5;
            cfg.pin_d15 = 4;

            _bus_instance.config(cfg);              // 設定値をバスに反映します。
            _panel_instance.setBus(&_bus_instance); // バスをパネルにセットします。
        }

        {                                        // 表示パネル制御の設定を行います。
            auto cfg = _panel_instance.config(); // 表示パネル設定用の構造体を取得します。

            cfg.pin_cs = -1;   // CS要拉低
            cfg.pin_rst = -1;  // RST和开发板RST相连
            cfg.pin_busy = -1; // BUSYが接続されているピン番号 (-1 = disable)

            // ※ 以下の設定値はパネル毎に一般的な初期値が設定されていますので、不明な項目はコメントアウトして試してみてください。

            cfg.memory_width = 320;   // ドライバICがサポートしている最大の幅
            cfg.memory_height = 480;  // ドライバICがサポートしている最大の高さ
            cfg.panel_width = 320;    // 実際に表示可能な幅
            cfg.panel_height = 480;   // 実際に表示可能な高さ
            cfg.offset_x = 0;         // パネルのX方向オフセット量
            cfg.offset_y = 0;         // パネルのY方向オフセット量
            cfg.offset_rotation = 0;  // 回転方向の値のオフセット 0~7 (4~7は上下反転)
            cfg.dummy_read_pixel = 8; // ピクセル読出し前のダミーリードのビット数
            cfg.dummy_read_bits = 1;  // ピクセル以外のデータ読出し前のダミーリードのビット数
            cfg.readable = true;      // データ読出しが可能な場合 trueに設定
            cfg.invert = false;       // パネルの明暗が反転してしまう場合 trueに設定
            cfg.rgb_order = false;    // パネルの赤と青が入れ替わってしまう場合 trueに設定
            cfg.dlen_16bit = true;    // データ長を16bit単位で送信するパネルの場合 trueに設定
            cfg.bus_shared = true;    // SDカードとバスを共有している場合 trueに設定(drawJpgFile等でバス制御を行います)

            _panel_instance.config(cfg);
        }

        setPanel(&_panel_instance); // 使用するパネルをセットします。
    }
};

LGFX lcd;

Adafruit_MLX90640 mlx;
float frame[32 * 24]; // buffer for full frame of temperatures
// uncomment *one* of the below
#define PRINT_TEMPERATURES
// #define PRINT_ASCIIART

void setup(void)
{
    pinMode(LCD_CS, OUTPUT);
    pinMode(LCD_BLK, OUTPUT);

    digitalWrite(LCD_CS, LOW);
    digitalWrite(LCD_BLK, HIGH);

    Serial.begin(115200);
    lcd.init();
    lcd.setRotation(2);

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
    Serial.println("Found Adafruit MLX90640");

    Serial.print("Serial number: ");
    Serial.print(mlx.serialNumber[0], HEX);
    Serial.print(mlx.serialNumber[1], HEX);
    Serial.println(mlx.serialNumber[2], HEX);

    //mlx.setMode(MLX90640_INTERLEAVED);
    mlx.setMode(MLX90640_CHESS);
    Serial.print("Current mode: ");
    if (mlx.getMode() == MLX90640_CHESS)
    {
        Serial.println("Chess");
    }
    else
    {
        Serial.println("Interleave");
    }

    mlx.setResolution(MLX90640_ADC_18BIT);
    Serial.print("Current resolution: ");
    mlx90640_resolution_t res = mlx.getResolution();
    switch (res)
    {
    case MLX90640_ADC_16BIT:
        Serial.println("16 bit");
        break;
    case MLX90640_ADC_17BIT:
        Serial.println("17 bit");
        break;
    case MLX90640_ADC_18BIT:
        Serial.println("18 bit");
        break;
    case MLX90640_ADC_19BIT:
        Serial.println("19 bit");
        break;
    }

    mlx.setRefreshRate(MLX90640_2_HZ);
    Serial.print("Current frame rate: ");
    mlx90640_refreshrate_t rate = mlx.getRefreshRate();
    switch (rate)
    {
    case MLX90640_0_5_HZ:
        Serial.println("0.5 Hz");
        break;
    case MLX90640_1_HZ:
        Serial.println("1 Hz");
        break;
    case MLX90640_2_HZ:
        Serial.println("2 Hz");
        break;
    case MLX90640_4_HZ:
        Serial.println("4 Hz");
        break;
    case MLX90640_8_HZ:
        Serial.println("8 Hz");
        break;
    case MLX90640_16_HZ:
        Serial.println("16 Hz");
        break;
    case MLX90640_32_HZ:
        Serial.println("32 Hz");
        break;
    case MLX90640_64_HZ:
        Serial.println("64 Hz");
        break;
    }
}

void loop()
{
    delay(500);
    if (mlx.getFrame(frame) != 0)
    {
        Serial.println("Failed");
        return;
    }
    Serial.println();
    Serial.println();
    for (uint8_t h = 0; h < 24; h++)
    {
        for (uint8_t w = 0; w < 32; w++)
        {
            float t = frame[h * 32 + w];
#ifdef PRINT_TEMPERATURES
            // Serial.print(t, 1);
            // Serial.print(", ");

            Serial.print((int)t);
            Serial.print(", ");
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
            Serial.print(c);
#endif
        }
        Serial.println();
    }
}
