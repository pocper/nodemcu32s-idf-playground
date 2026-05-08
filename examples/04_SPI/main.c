#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/spi_common.h"
#include "esp_timer.h"

#include "img_this_is_fine.h"

#define ILI9341_HOST SPI3_HOST
#define PIN_NUM_CS   5
#define PIN_NUM_DC   0
#define PIN_NUM_MOSI 23
#define PIN_NUM_SCK  18
#define PIN_NUM_LED  15
#define PIN_NUM_MISO 19

// MOSI has two content: command or data, using DC line to control which one
#define DC_CMD  0
#define DC_DATA 1

#define PARALLEL_LINES 16

#define MAX_IMG 7

void lcd_pre_callback(spi_transaction_t *t);
void lcd_write_cmd(spi_device_handle_t spi, const uint8_t cmd, bool keep_cs_active);
void lcd_write_data(spi_device_handle_t spi, int len, const uint8_t *data);
void lcd_init(spi_device_handle_t spi);
static void send_lines(spi_device_handle_t spi, int ypos, uint16_t *lines_data);
static void wait_lines_response(spi_device_handle_t spi);
static void display(void *arg);
void calc_lines(uint16_t *dest, int y, int h);
void update_val(void *arg);

volatile uint8_t current_img = 0;
const uint8_t *ptr_img_arr[] = {img0_map, img1_map, img2_map, img3_map, img4_map, img5_map, img6_map};

typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;

DRAM_ATTR static const lcd_init_cmd_t ili_init_cmds[] = {
    /* Power control B, power control = 0, DC_ENA = 1 */
    {0xCF, {0x00, 0x83, 0X30}, 3},
    /* Power on sequence control,
     * cp1 keeps 1 frame, 1st frame enable
     * vcl = 0, ddvdh=3, vgh=1, vgl=2
     * DDVDH_ENH=1
     */
    {0xED, {0x64, 0x03, 0X12, 0X81}, 4},
    /* Driver timing control A,
     * non-overlap=default +1
     * EQ=default - 1, CR=default
     * pre-charge=default - 1
     */
    {0xE8, {0x85, 0x01, 0x79}, 3},
    /* Power control A, Vcore=1.6V, DDVDH=5.6V */
    {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
    /* Pump ratio control, DDVDH=2xVCl */
    {0xF7, {0x20}, 1},
    /* Driver timing control, all=0 unit */
    {0xEA, {0x00, 0x00}, 2},
    /* Power control 1, GVDD=4.75V */
    {0xC0, {0x26}, 1},
    /* Power control 2, DDVDH=VCl*2, VGH=VCl*7, VGL=-VCl*3 */
    {0xC1, {0x11}, 1},
    /* VCOM control 1, VCOMH=4.025V, VCOML=-0.950V */
    {0xC5, {0x35, 0x3E}, 2},
    /* VCOM control 2, VCOMH=VMH-2, VCOML=VML-2 */
    {0xC7, {0xBE}, 1},
    /* Memory access control, MX=MY=0, MV=1, ML=0, BGR=1, MH=0 */
    {0x36, {0x28}, 1},
    /* Pixel format, 16bits/pixel for RGB/MCU interface */
    {0x3A, {0x55}, 1},
    /* Frame rate control, f=fosc, 70Hz fps */
    {0xB1, {0x00, 0x1B}, 2},
    /* Enable 3G, disabled */
    {0xF2, {0x08}, 1},
    /* Gamma set, curve 1 */
    {0x26, {0x01}, 1},
    /* Positive gamma correction */
    {0xE0, {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00}, 15},
    // {0xE0, {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0X87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00}, 15},
    /* Negative gamma correction */
    {0xE1, {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F}, 15},
    // {0XE1, {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F}, 15},
    /* Column address set, SC=0, EC=0xEF */
    {0x2A, {0x00, 0x00, 0x00, 0xEF}, 4},
    /* Page address set, SP=0, EP=0x013F */
    {0x2B, {0x00, 0x00, 0x01, 0x3f}, 4},
    /* Memory write */
    {0x2C, {0}, 0},
    /* Entry mode set, Low vol detect disabled, normal display */
    {0xB7, {0x07}, 1},
    /* Display function control */
    {0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},
    /* Sleep out */
    {0x11, {0}, 0x80},
    /* Display on */
    {0x29, {0}, 0x80},
    {0, {0}, 0xff},
};


void app_main() {
    // GPIO Initialize
    gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_LED, GPIO_MODE_OUTPUT);

    // SPI Initialize
    spi_bus_config_t cfg_spi_bus = {
        .sclk_io_num = PIN_NUM_SCK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 320*PARALLEL_LINES*sizeof(uint16_t) // unit:bytes
    };
    spi_bus_initialize(ILI9341_HOST, &cfg_spi_bus, SPI_DMA_CH_AUTO);

    // CPOL=0 -> SCLK=0 when SPI idle
    // CPHA=0 -> first edge sample data
    spi_device_interface_config_t cfg_spi_device = {
        .clock_speed_hz = 40 * 1000 * 1000, // SCLK frequency = 40 MHz
        .mode = 0, // (CPOL, CPHA) = (0, 0)
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 20,
        .pre_cb = lcd_pre_callback
    };

    spi_device_handle_t spi;
    spi_bus_add_device(ILI9341_HOST, &cfg_spi_device, &spi);

    lcd_init(spi);
    xTaskCreate(display, "Display", 4096, spi, 2, NULL);
    xTaskCreate(update_val, "Update", 2048, NULL, 1, NULL);
    // display(spi);
}

void update_val(void *arg) {
    TickType_t last_time = xTaskGetTickCount();
    while(1) {
        if(current_img==(MAX_IMG-1)) {
            current_img = 0;
        }
        else {
            current_img++;
        }
        vTaskDelayUntil(&last_time, pdMS_TO_TICKS(200)); // 200 ms update img
    }
}

void lcd_pre_callback(spi_transaction_t *t) {
    int dc = (int)t->user;
    gpio_set_level(PIN_NUM_DC, dc);
}

void lcd_write_cmd(spi_device_handle_t spi, const uint8_t cmd, bool keep_cs_active) {
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8; // command length is 8-bits
    t.tx_buffer = &cmd;
    t.user = (void *)DC_CMD;
    if(keep_cs_active) {
        t.flags = SPI_TRANS_CS_KEEP_ACTIVE;
    }

    ret = spi_device_polling_transmit(spi, &t);
    assert(ret==ESP_OK);
}

void lcd_write_data(spi_device_handle_t spi, int len, const uint8_t *data) {
    if(len==0) {
        return;
    }

    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = len*8; // length in bytes
    t.tx_buffer = data;
    t.user = (void*)DC_DATA;
    ret = spi_device_polling_transmit(spi, &t);
    assert(ret==ESP_OK);
}

void lcd_init(spi_device_handle_t spi) {
    int cmd = 0;
    const lcd_init_cmd_t *lcd_cmd = ili_init_cmds;
    while(lcd_cmd[cmd].databytes !=0xff) {
        lcd_write_cmd(spi, lcd_cmd[cmd].cmd, false);
        lcd_write_data(spi, lcd_cmd[cmd].databytes & 0x1f, lcd_cmd[cmd].data);
        if(lcd_cmd[cmd].databytes & 0x80) {
            vTaskDelay(100/portTICK_PERIOD_MS);
        }
        cmd++;
    }
     gpio_set_level(PIN_NUM_LED, 1); // 1: backlight open
}

static void send_lines(spi_device_handle_t spi, int ypos, uint16_t *lines_data) {
    // trans[0]: CASET(2Ah)
    // trans[1]: SC[15:8], SC[7:0], EC[15:8], EC[7:0]
    // trans[2]: PASET(2Bh)
    // trans[3]: SP[15:8], SP[7:0], EP[15:8], EP[7:0]
    // trans[4]: RAMWR(2Ch)
    // trans[5]: Blue[16:12], Green[11:5], Red[4:0]
    static spi_transaction_t trans[6];

    for(int i=0;i<6;i++) {
        memset(&trans[i], 0, sizeof(spi_transaction_t));
        if((i&1)==0) {
            // Even for cmd
            trans[i].user = (void*) DC_CMD;
        }
        else {
            // Odd for data
            trans[i].user = (void*) DC_DATA;
        }

        if(i < 5) {
            if((i&1)==0) {
                trans[i].length = 8;
            }
            else {
                trans[i].length = 8*4;
            }
            // trans[4:0] uses tx_data
            trans[i].flags = SPI_TRANS_USE_TXDATA;
        }
        else {
            // trans[5]   uses tx_buffer
            trans[i].length = 320*PARALLEL_LINES*sizeof(uint16_t)*8;
        }
    }

    // Column set(2Ah)
    trans[0].tx_data[0] = 0x2A;
    // SC = 0
    trans[1].tx_data[0] = 0;
    trans[1].tx_data[1] = 0;
    // EC = 320-1
    trans[1].tx_data[2] = (320-1) >> 8;
    trans[1].tx_data[3] = (320-1) & 0xff;

    // Page set(2Bh)
    trans[2].tx_data[0] = 0x2B;

    // SP = ypos
    trans[3].tx_data[0] = (ypos)>>8;
    trans[3].tx_data[1] = (ypos) & 0xff;
    // EP = ypos+PARALLEL_LINES-1
    trans[3].tx_data[2] = (ypos+PARALLEL_LINES-1)>>8;
    trans[3].tx_data[3] = (ypos+PARALLEL_LINES-1) & 0xff;

    // RAMWR(2Ch)
    trans[4].tx_data[0] = 0x2C;

    // Data = BGR
    trans[5].tx_buffer = lines_data;
    esp_err_t ret;
    for(int i=0;i<6;i++) {
        ret = spi_device_queue_trans(spi, &trans[i], portMAX_DELAY);
        assert(ret == ESP_OK);
    }
}

static void wait_lines_response(spi_device_handle_t spi) {
    spi_transaction_t *trans_response;
    esp_err_t ret;
    for(int i=0;i<6;i++) {
        ret = spi_device_get_trans_result(spi, &trans_response, portMAX_DELAY);
        assert(ret == ESP_OK);
    }
}

static void display(void *arg) {
    spi_device_handle_t spi = (spi_device_handle_t)arg;
    uint16_t *lines[2];
    for(int i=0;i<2;i++) {
        lines[i] = spi_bus_dma_memory_alloc(ILI9341_HOST, 320*PARALLEL_LINES*sizeof(uint16_t), 0);
        assert(lines[i] != NULL);
    }

    int n_calc_lines = 0;
    int n_display_lines = -1;
    while(1) {
        for(int y=0;y<240;y+=PARALLEL_LINES) {
            calc_lines(lines[n_calc_lines], y, PARALLEL_LINES);
            
            if(n_display_lines!=-1) {
                wait_lines_response(spi);
            }
            
            n_display_lines = n_calc_lines;
            n_calc_lines = (n_calc_lines==0)?1:0;
            
            send_lines(spi, y, lines[n_display_lines]);
        }
    }
}

void calc_lines(uint16_t *dest, int ypos, int h) {
    const uint8_t *ptr_img = ptr_img_arr[current_img];
    for(int y=ypos;y<(ypos+h);y++) {
        for(int x=0;x<320;x++) {
            if(y>=200) {
                *dest = 0x0000;
            }
            else {
                *dest = (ptr_img[((y*320+x)<<1) + 0] << 8) | (ptr_img[((y*320+x)<<1) + 1]);
            }
            dest++;
        }
    }
}