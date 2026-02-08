/*
  laser_3axis_ili9341.h
  Definición personalizada de máquina para GRBL_ESP32
  CNC Laser 3 ejes (X, Y, Z) + pantalla TFT ILI9341 táctil
*/

#pragma once


#define DISPLAY_CODE_FILENAME   "Custom/tft_ili9341.cpp"

// =======================================================
//  CONFIGURACIÓN GENERAL DE LA MÁQUINA
// =======================================================
#define MACHINE_NAME "Laser_3Axis_ILI9341"

// Número de ejes
#define N_AXIS 3

// Tipo de controlador por eje (ejemplo: A4988, TMC2209, DRV8825)
#define X_DRIVER_TYPE  DRIVER_TYPE_A4988
#define Y_DRIVER_TYPE  DRIVER_TYPE_A4988
#define Z_DRIVER_TYPE  DRIVER_TYPE_A4988

// =======================================================
//  MAPEADO DE PINES DE MOTORES PASO A PASO
// =======================================================
#ifndef X_STEP_PIN
#define X_STEP_PIN       GPIO_NUM_12
#endif
#ifndef X_DIRECTION_PIN
#define X_DIRECTION_PIN  GPIO_NUM_14
#endif
#ifndef STEPPERS_DISABLE_PIN
#define STEPPERS_DISABLE_PIN GPIO_NUM_13
#endif

#ifndef Y_STEP_PIN
#define Y_STEP_PIN       GPIO_NUM_27
#endif
#ifndef Y_DIRECTION_PIN
#define Y_DIRECTION_PIN  GPIO_NUM_26
#endif

#ifndef Z_STEP_PIN
#define Z_STEP_PIN       GPIO_NUM_25
#endif
#ifndef Z_DIRECTION_PIN
#define Z_DIRECTION_PIN  GPIO_NUM_33
#endif

/*#ifndef A_STEP_PIN
#define A_STEP_PIN       GPIO_NUM_32
#endif

#ifndef A_DIRECTION_PIN
#define A_DIRECTION_PIN  GPIO_NUM_17
#endif*/

// =======================================================
//  FINAL DE CARRERA / LIMIT SWITCHES
// =======================================================
#ifndef X_LIMIT_PIN
#define X_LIMIT_PIN      GPIO_NUM_35  // input-only -> perfecto para endstop
#endif
#ifndef Y_LIMIT_PIN
#define Y_LIMIT_PIN      GPIO_NUM_34
#endif
#ifndef Z_LIMIT_PIN
#define Z_LIMIT_PIN      GPIO_NUM_39
#endif

#define INVERT_LIMIT_PINS false

// =======================================================
//  CONFIGURACIÓN DEL LÁSER
// =======================================================
#define ENABLE_LASER

#define SPINDLE_TYPE            SpindleType::LASER
#define LASER_OUTPUT_PIN      GPIO_NUM_16

// The laser module fires without a low signal. This keeps the enable on
//#define LVL_SHIFT_ENABLE        GPIO_NUM_32
//#define CUSTOM_CODE_FILENAME    "Custom/laser_3axis_ili9341.cpp"


// #define COOLANT_MIST_PIN      GPIO_NUM_ // Pin para el compresor (cambiar pin si se utiliza)

// =======================================================
//  PANTALLA TFT ILI9341 + TOUCH (SPI)
// =======================================================
// NOTA: La configuración SPI principal (SCK, MISO, MOSI) se hace
// en el archivo User_Setup.h de la librería TFT_eSPI (versión 2.5.43).

#define ENABLE_TFT_DISPLAY
#define ENABLE_TOUCH_UI
#define USE_CUSTOM_UI

#define TFT_DRIVER      ILI9341_DRIVER
#define TFT_ROTATION    1 // 0=0°, 1=90°, 2=180°, 3=270°

#define TFT_MISO   GPIO_NUM_19
#define TFT_MOSI   GPIO_NUM_23
#define TFT_SCLK   GPIO_NUM_18
#define TFT_CS     GPIO_NUM_15
#define TFT_DC     GPIO_NUM_2
#define TFT_RST    GPIO_NUM_4
#define TOUCH_CS   GPIO_NUM_21
#define TOUCH_IRQ  GPIO_NUM_36

#define SPI_FREQUENCY       27000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY 2500000

// =======================================================
//  COMUNICACIÓN SERIAL / WIFI (Opcional)
// =======================================================
/*
#define SERIAL_RX_PIN 3
#define SERIAL_TX_PIN 1

//#define ENABLE_WIFI
//#define WIFI_MODE_STA
//#define WIFI_SSID "INFINITUM6134"
//#define WIFI_PASSWORD "DGkQb3j4DS"
*/

// =======================================================
//  TARJETA SD (SPI SECUNDARIO) (Opcional)
// =======================================================
// NOTA: se debe usar SPI alternativo si la pantalla ocupa los pines VSPI.
#define ENABLE_SD_CARD
#define SD_MISO  GPIO_NUM_19
#define SD_MOSI  GPIO_NUM_23
#define SD_SCK   GPIO_NUM_18
#define SD_CS   GPIO_NUM_5

