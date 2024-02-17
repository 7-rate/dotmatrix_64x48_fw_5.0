#include <Arduino.h>
#include "matrix_drive.h"
#include "spiffs_fs.h"
#include "settings.h"
#include "mz_update.h"
#include "mz_wifi.h"
#include "buttons.h"
#include "mz_console.h"
#include "threadsync.h"
#include "calendar.h"
#include "status_led.h"
#include "ambient.h"
#include "mz_i2c.h"
#include "mz_bme.h"
#include "web_server.h"
#include "ui.h"
#include "mz_version.h"
#include "pendulum.h"
#include "fonts/font_ft.h"
#include "openweathermap.h"
#include "panic.h"

#define MY_CONFIG_ARDUINO_LOOP_STACK_SIZE 16384U
extern TaskHandle_t loopTaskHandle; // defined in main.cpp of Arduino core
extern void loopTask(void *pvParameters); // defined in main.cpp of Arduino core 

void setup() {
  // AARRRRRRRRRRGGGGHHHHHHHHHHHHHHHHHHHHHHH!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // I have no idea about properly increase the arduino user task's stack size.
  // Using -DCONFIG_ARDUINO_LOOP_STACK_SIZE=XXXXXX DOES NOT WORK because
  // it's redefined at sdkconfig.h and there is no way to use custom sdkconfig.h
  // at this time. I'm not sure if I can do that using ESP-IDF with Arduino
  // option enabled, not using ESP-IDF and the Arduino core separately.

  // So, ... This is one possible solution. Spawining a new loop task with large
  // stack size, and delete old one.
  static volatile bool second_run = false;
  if(!second_run)
  {
    second_run = true;
    xTaskCreateUniversal(loopTask, "loopTask", MY_CONFIG_ARDUINO_LOOP_STACK_SIZE,
      NULL, 1, &loopTaskHandle, CONFIG_ARDUINO_RUNNING_CORE);
    vTaskDelete(nullptr); // suicide
    for(;;) { /* TODO: panic */ ; }
  }

  // put your setup code here, to run once:
  init_console(); // this also initializes the serial output and stdio

  status_led_early_setup();
  matrix_drive_early_setup(); // blank all leds

  delay(1000);

  panic_init();
  panic_show_boot_count();
  panic_check_repeated_boot();
  printf("\n\nGreetings. This is MZ5 firmware.\n");
  printf("%s\n", version_get_info_string().c_str());
  show_ota_boot_status();
  status_led_setup();
  panic_record_checkpoint(CP_MATRIX_CHECK);
  matrix_drive_setup();

  panic_record_checkpoint(CP_SPIFFS_OPEN);
  init_fs(); // in init_fs(), if main SPIFFS mount fails, set_system_recovery_mode() issues.


  // before init_settings, check cancel buttion be pressed over 1sec
  delay(100); // wait for matrix row drive cycles several times
  button_check_physical_buttons_are_sane(); // check whether the button input is floating or not

  if(button_get_scan_bits() & BUTTON_CANCEL)
  {
    printf("Cancel button pressed. Seeing if this button is pressed over 1 sec .");
    bool clear_it = true;
    for(int i = 0; i < 10; ++i)
    {
      if(!(button_get_scan_bits() & BUTTON_CANCEL)) { clear_it = false; break; }
      delay(100);
      printf(".");
      fflush(stdout);
    }
    if(clear_it)
    {
        printf(" OK, now clearing settings.");
        fflush(stdout);
        clear_settings();
    }
    puts("");
  }

  // or, if /spiffs/.clear exist, clear all settings
  if(FILE *f = fopen(CLEAR_SETTINGS_INDICATOR_FILE, "r"))
  {
    fclose(f);
    unlink(CLEAR_SETTINGS_INDICATOR_FILE);
    printf("Clearing settings requested.\n");
    fflush(stdout);
    clear_settings();
  }

  panic_record_checkpoint(CP_SETTINGS_OPEN);
  init_settings();

  wifi_setup();
  init_calendar(); // sntp initialization needs to be located after network stack initialization
  init_i2c();
  init_bme280();
  init_ambient();
  init_font_ft();
  init_OpenWeatherMap();
  web_server_setup();
  ui_setup();
  begin_console();

  panic_record_checkpoint(CP_WIFI_START);
  wifi_start();
}

void loop() {
  // put your main code here, to run repeatedly:
  matrix_drive_loop();
  button_update();
  poll_main_thread_queue();
  status_led_loop();
  poll_ambient();
  poll_bme280();
  web_server_handle_client();
  poll_pendulum();
  ui_process();
  panic_notify_loop_is_running();
}