#include <M5Unified.h>
#include <Arduino.h>

#include <driver/gpio.h>
#include "driver/i2s.h"
#include <math.h>

#define TAG "Main"

#define AW9523_ADDR 0x58
#define AW9523_SPK_PA_EN_ON() 	M5.In_I2C.bitOn(AW9523_ADDR, 0x03, 0x08, 400000)
#define AW9523_SPK_PA_EN_OFF() 	M5.In_I2C.bitOff(AW9523_ADDR, 0x03, 0x08, 400000)
#define AW9523_LRA_PA_EN_ON() 	M5.In_I2C.bitOn(AW9523_ADDR, 0x03, 0x04, 400000)
#define AW9523_LRA_PA_EN_OFF() 	M5.In_I2C.bitOff(AW9523_ADDR, 0x03, 0x04, 400000)
#define AW9523_LO_PA_EN_ON() 	M5.In_I2C.bitOn(AW9523_ADDR, 0x03, 0x02, 400000)
#define AW9523_LO_PA_EN_OFF() 	M5.In_I2C.bitOff(AW9523_ADDR, 0x03, 0x02, 400000)
#define AW9523_TRS_SW_ON() 		M5.In_I2C.bitOn(AW9523_ADDR, 0x03, 0x01, 400000)
#define AW9523_TRS_SW_OFF() 	M5.In_I2C.bitOff(AW9523_ADDR, 0x03, 0x01, 400000)

typedef struct {
	uint8_t key0 : 1;
	uint8_t key1 : 1;
} user_key_state_t;

int16_t mic_buf[2048];

void i2s_init(i2s_channel_fmt_t channel_fmt)
{
	i2s_config_t i2s_config;
    memset(&i2s_config, 0, sizeof(i2s_config_t));
    i2s_config.mode                 = (i2s_mode_t)( I2S_MODE_MASTER | I2S_MODE_TX );
    i2s_config.sample_rate          = 48000; // dummy setting
    i2s_config.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
    i2s_config.channel_format       = channel_fmt;
    i2s_config.communication_format = (i2s_comm_format_t)( I2S_COMM_FORMAT_STAND_I2S );
    i2s_config.tx_desc_auto_clear   = true;
    i2s_config.dma_buf_count        = 3;
    i2s_config.dma_buf_len          = 256;

    if (i2s_driver_install(I2S_NUM_1, &i2s_config, 0, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "I2S Install failed");
        return;
    }
    
    i2s_pin_config_t i2s_pin_cfg = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = 0,
        .ws_io_num = 27,
        .data_out_num = 2,
        .data_in_num = 34
	};

    if (i2s_set_pin(I2S_NUM_1, &i2s_pin_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "I2S Pin configuration failed");
        return;
    }
}

esp_err_t i2s_mic_init()
{
	esp_err_t err = ESP_OK;
	i2s_config_t i2s_config = {
		.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
		.sample_rate = 16000,
		.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
		.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    	.communication_format = I2S_COMM_FORMAT_I2S,
		.intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
		.dma_buf_count = 4,
		.dma_buf_len = 256,
		.use_apll = 0,
	};

	i2s_pin_config_t i2s_mic_pins = {
		.bck_io_num = I2S_PIN_NO_CHANGE,
		.ws_io_num = 27,
		.data_out_num = I2S_PIN_NO_CHANGE,
		.data_in_num = 34,
	};

	err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL); // PDM mode only supported in I2S_NUM_0
	if (err != ESP_OK) {
		return err;
	}

	err = i2s_set_pin(I2S_NUM_0, &i2s_mic_pins);
	if (err != ESP_OK) {
		return err;
	}

	err = i2s_start(I2S_NUM_0);
	if (err != ESP_OK) {
		return err;
	}
	return err;
}

void i2s_deinit()
{
	i2s_stop(I2S_NUM_1);
	i2s_driver_uninstall(I2S_NUM_1);
}

#include "audio_data.c"

void i2s_play_buf(const void* buf, uint32_t size, uint32_t cycle)
{
    size_t written;
    for (int i = 0; i < cycle; i++) {
        i2s_write(I2S_NUM_1, (const char*)buf, size, &written, 1000);
    }
	i2s_zero_dma_buffer(I2S_NUM_1); 
}

user_key_state_t get_key_state()
{
	user_key_state_t key_state;
	uint8_t p0_state = M5.In_I2C.readRegister8(AW9523_ADDR, 0x00, 400000);
	key_state.key0 = !((p0_state >> 4) & 0x01);
	key_state.key1 = !((p0_state >> 5) & 0x01);
	return key_state;
}

extern "C" void app_main()
{
	
	auto config = M5.config();
	config.internal_spk = false;
	config.internal_mic = false;
	config.internal_imu = false;
	config.internal_rtc = false;
	M5.begin(config);
	M5.Power.Axp192.setGPIO2(1);
	M5.Lcd.fillScreen(GREEN);
	M5.Lcd.setTextColor(WHITE);
	M5.Lcd.setTextSize(2);

	uint8_t aw9523_id = M5.In_I2C.readRegister8(AW9523_ADDR, 0x10, 400000);
	if (aw9523_id != 0x23) {
		M5.Lcd.fillScreen(RED);
		M5.Lcd.setTextDatum(CC_DATUM);
		M5.Lcd.drawString("AW9523 not found", 160, 120);
	}
	
	M5.In_I2C.writeRegister8(AW9523_ADDR, 0x04, 0xFF, 400000); // P0 all input
	M5.In_I2C.writeRegister8(AW9523_ADDR, 0x03, 0x00, 400000); // P1 all clear
	M5.In_I2C.writeRegister8(AW9523_ADDR, 0x05, 0x00, 400000); // P1 all output


	// 1、通过AW9523仅拉高SPK_PA_EN，使用I2S左声道输出一段声音，确认喇叭是否正常工作
	i2s_init(I2S_CHANNEL_FMT_ONLY_LEFT);
	AW9523_SPK_PA_EN_ON();
	i2s_set_clk(I2S_NUM_1, 64000, 16, I2S_CHANNEL_MONO);
	i2s_play_buf(sine_160hz, sizeof(sine_160hz), 3);
	AW9523_SPK_PA_EN_OFF( );
	i2s_deinit();

	// 2、通过AW9523仅拉高LRA_PA_EN，使用I2S右声道输出160Hz的正弦波音频，持续1S后拉低LRA_PA_EN，确认震动马达是否正常工作
	i2s_init(I2S_CHANNEL_FMT_ONLY_RIGHT);
	AW9523_LRA_PA_EN_ON();
	i2s_set_clk(I2S_NUM_1, 16000, 16, I2S_CHANNEL_MONO);
	i2s_play_buf(sine_160hz, sizeof(sine_160hz), 1);
	AW9523_LRA_PA_EN_OFF();
	i2s_deinit();

	// 3、通过AW9523仅拉高LO_PA_EN，使用I2S双声道播放一段音频，插入3.5mm耳机，确认耳机输出是否正常工作
	i2s_init(I2S_CHANNEL_FMT_ALL_LEFT);
	AW9523_LO_PA_EN_ON();
	i2s_set_clk(I2S_NUM_1, 64000, 16, I2S_CHANNEL_STEREO);
	i2s_play_buf(sine_160hz, sizeof(sine_160hz), 6);
	AW9523_LO_PA_EN_OFF();
	i2s_deinit();


	// 4、通过AW9523仅拉高nAUDIO/TRS_SW信号，在耳机插座4脚输入5V的串口MIDI信号，确认主机是否能收到
	// 5. User key test
	// 6. Mic test
	i2s_mic_init();
	Serial1.begin(31250, SERIAL_8N1, 19, 33);
	AW9523_TRS_SW_ON();
	M5.Lcd.setTextDatum(TL_DATUM);
	M5.Lcd.setTextColor(BLACK, GREEN);
	M5.Lcd.setCursor(10, 60);
	M5.Lcd.printf("MIDI: --");

	while (1)
	{	
		user_key_state_t key_state = get_key_state();
		if (key_state.key0) {
			M5.Lcd.drawString("K0 Pressed ", 10, 20);
		} 
		else {
			M5.Lcd.drawString("K0 Released", 10, 20);
		}
		if (key_state.key1) {
			M5.Lcd.drawString("K1 Pressed ", 10, 40);
		} 
		else {
			M5.Lcd.drawString("K1 Released", 10, 40);
		}

		if (Serial1.available()) {
			printf("%d\n", Serial1.available());
			uint8_t data = Serial1.read();
			M5.Lcd.setCursor(10, 60);
			M5.Lcd.printf("MIDI: 0x%02X", data);
			Serial1.flush();
		}

		size_t bytes_read;
		esp_err_t err = i2s_read(I2S_NUM_0, mic_buf, sizeof(mic_buf), &bytes_read, 1000);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "I2S Read failed, errno = %d", err);
			break;
		}

		// RMS
		uint64_t sum = 0;
		int len = sizeof(mic_buf) / sizeof(mic_buf[0]);
		for (int i = 0; i < len; i++) {
			sum += mic_buf[i] * mic_buf[i];
		}
		sum /= len;
		M5.Lcd.setCursor(10, 80);
		M5.Lcd.printf("Mic: %.1f", sqrtf(sum));

		delay(10);
	}
}
