#include <lvgl.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <WiFi.h>
#include <time.h>

// Your working display pin configurations
#define CLK 6
#define MOSI 7
#define DC 2
#define RST 1
#define BLK 3
#define CS -1 
#define SIZE 240
#define SPEED 10 * 1000 * 1000

// --- Wi-Fi Credentials ---
const char* ssid     = "Connecticut Highway";
const char* password = "My@home--pass15";

// --- Timezone Configuration ---
const long  gmtOffset_sec = 10800; 
const int   daylightOffset_sec = 0; 
const char* ntpServer = "pool.ntp.org";

Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, CS, DC, RST);

// LVGL internal rendering buffer (1/10th of screen size)
#define DRAW_BUF_SIZE (SIZE * SIZE / 10)
uint16_t draw_buf[DRAW_BUF_SIZE]; 

// Global UI Labels for Stacked Layout
lv_obj_t *hour_label;
lv_obj_t *min_label;
lv_obj_t *date_label;

// Helper function to return system milliseconds to LVGL core engine
uint32_t my_tick_get_cb(void) {
    return millis();
}

// LVGL Flush Callback
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.drawRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
    lv_display_flush_ready(disp);
}

// LVGL Automated Timer Callback: Fires every 1 second
void clock_timer_cb(lv_timer_t * timer) {
    struct tm timeinfo;
    
    if(!getLocalTime(&timeinfo)){
        lv_label_set_text(hour_label, "--");
        lv_label_set_text(min_label, "--");
        lv_label_set_text(date_label, "NTP Sync Drop...");
        return;
    }

    // 1. Update Separate Hour and Minute Text Strings
    lv_label_set_text_fmt(hour_label, "%02d", timeinfo.tm_hour);
    lv_label_set_text_fmt(min_label, "%02d", timeinfo.tm_min);

    // 2. Format and Update Date String at bottom
    char date_str[32];
    strftime(date_str, sizeof(date_str), "%A, %b %d", &timeinfo);
    
    // Capitalize date text cleanly
    for (int i = 0; date_str[i]; i++) date_str[i] = toupper(date_str[i]);
    lv_label_set_text(date_label, date_str);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    // ==========================================
    // STEP 1: SILENT BACKGROUND WI-FI & NTP SYNC
    // ==========================================
    Serial.printf("Connecting to %s ", ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" CONNECTED!");

    IPAddress localIP = WiFi.localIP();
    IPAddress gateway = WiFi.gatewayIP();
    IPAddress subnet  = WiFi.subnetMask();
    IPAddress dns1(8, 8, 8, 8); 
    WiFi.config(localIP, gateway, subnet, dns1);

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    Serial.print("Waiting for NTP Time Sync");
    struct tm timeTest;
    int retryCounter = 0;
    while(!getLocalTime(&timeTest, 1000) && retryCounter < 15) {
        Serial.print(".");
        retryCounter++;
        delay(500);
    }
    Serial.println("\nTime Synchronized Successfully!");

    // ==========================================
    // STEP 2: DISPLAY HARDWARE & SPI INITIALIZATION
    // ==========================================
    pinMode(BLK, OUTPUT);
    digitalWrite(BLK, HIGH);
    pinMode(RST, OUTPUT);
    digitalWrite(RST, HIGH);
    delay(50);
    digitalWrite(RST, LOW);
    delay(100);
    digitalWrite(RST, HIGH);
    delay(150);

    SPI.begin(CLK, -1, MOSI, -1);
    tft.init(SIZE, SIZE, SPI_MODE3);
    tft.setSPISpeed(SPEED);
    tft.invertDisplay(true); 
    tft.setRotation(0);
    tft.fillScreen(ST77XX_BLACK);

    // ==========================================
    // STEP 3: LVGL CORE ENGINE INITIALIZATION
    // ==========================================
    lv_init();
    
    // Core Timing fix to prevent frozen time frames
    lv_tick_set_cb(my_tick_get_cb);

    lv_display_t *disp = lv_display_create(SIZE, SIZE);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, my_disp_flush);

    // Set background layout color to pure black
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), LV_PART_MAIN);

    // ==========================================
    // STEP 4: CREATE THE STACKED CLOCK LAYOUT UI
    // ==========================================

    // 1. HOUR LABEL (Top row - Neon Cyan)
    hour_label = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(hour_label, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(hour_label, lv_color_hex(0x00FFCC), LV_PART_MAIN); 
    lv_label_set_text(hour_label, "--");
    lv_obj_align(hour_label, LV_ALIGN_CENTER, 0, -60); // Shifted up high

    // 2. MINUTE LABEL (Middle row - Bright White for contrast)
    min_label = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(min_label, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(min_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN); 
    lv_label_set_text(min_label, "--");
    lv_obj_align(min_label, LV_ALIGN_CENTER, 0, 0); // Positioned directly dead-center

    // 3. DATE LABEL (Bottom row - Muted light grey text)
    date_label = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(date_label, lv_color_hex(0xAAAAAA), LV_PART_MAIN); 
    lv_label_set_text(date_label, "LOADING...");
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 60); // Shifted low to the bottom

    // Force an initial update cycle manually to populate time values instantly
    clock_timer_cb(NULL);
    lv_timer_handler(); 

    // Create a 1-second refresh task
    lv_timer_create(clock_timer_cb, 1000, NULL);
}

void loop() {
    lv_timer_handler(); 
    delay(5); 
}
