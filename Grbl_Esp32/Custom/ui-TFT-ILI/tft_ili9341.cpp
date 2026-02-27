#include <TFT_eSPI.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <JPEGDecoder.h>
#include "tft_ui_manager.h"
#include "tft_ui_touch.h"
#include "tft_ui_view.h"
#include "tft_ui_painter.h"

#define CALIBRATION_FILE "/TouchCalData2"  // Calibration file stored in SPIFFS
#define REPEAT_CAL false                   // if true calibration is requested after reboot

float                machine_position[MAX_N_AXIS];
float                work_position[MAX_N_AXIS];
bool                 existSD = true, actualizarInterfaz = true;
char                 currentFilename[128];
tft_ui::RuntimeState runtimeState;

String selectedGCodeFile = "";  // Nombre del archivo G-code a confirmar
bool   showConfirmDialog = false;
#define DIALOG_X 60
#define DIALOG_Y 45
#define DIALOG_W 220
#define DIALOG_H 150

TFT_eSPI tft = TFT_eSPI();

TaskHandle_t displayUpdateTaskHandle = NULL;
// Estado de la interfaz
tft_ui::UIState     ui_state = tft_ui::UI_MENU;  // Estado inicial
tft_ui::TouchReader touchReader;

// Botones
tft_ui::TouchButton buttons[] = {
    { 8, 18, 33, 43, "Home", "/ui-icons/casa.jpg", "/ui-icons/casa-activo.jpg", tft_ui::SIDEBAR_COLOR },
    { 8, 78, 33, 103, "Media", "/ui-icons/carpeta.jpg", "/ui-icons/carpeta-activo.jpg", tft_ui::SIDEBAR_COLOR },
    { 8, 138, 33, 163, "Control", "/ui-icons/control.jpg", "/ui-icons/control-activo.jpg", tft_ui::SIDEBAR_COLOR },
    { 8, 198, 33, 223, "Config", "/ui-icons/config.jpg", "/ui-icons/config-activo.jpg", tft_ui::SIDEBAR_COLOR },
    { 265,
      5,
      tft_ui::WIDTH_TOPBAR_BUTTON + 265,
      tft_ui::HEIGHT_TOPBAR_BUTTON + 5,
      "MicroSD",
      "/ui-icons/microSD.jpg",
      "/ui-icons/microSD.jpg",
      tft_ui::TOPBAR_COLOR },
    { 290,
      5,
      tft_ui::WIDTH_TOPBAR_BUTTON + 290,
      tft_ui::HEIGHT_TOPBAR_BUTTON + 5,
      "WiFi",
      "/ui-icons/wifi-no.jpg",
      "/ui-icons/wifi-ok.jpg",
      tft_ui::TOPBAR_COLOR },
    { 218, 130, 243, 155, "Play-Pause", "/ui-main-icon/play.jpg", "/ui-main-icon/pause.jpg", tft_ui::MAIN_BUTTON_COLOR },
    { 273, 130, 298, 155, "Stop", "/ui-main-icon/stop.jpg", "/ui-main-icon/stop.jpg", tft_ui::MAIN_BUTTON_COLOR },
};
const uint8_t NUM_BUTTONS = sizeof(buttons) / sizeof(buttons[0]);

void displayUpdate(void* pvParameters);

void display_init() {
#ifdef ENABLE_SD_CARD
    pinMode(SD_CS, OUTPUT);
#endif

// 3. INICIALIZACIÓN DE LA PANTALLA
#ifdef ENABLE_TFT_DISPLAY
    // Inicialización de Chip Selects
    digitalWrite(TOUCH_CS, HIGH);
    digitalWrite(TFT_CS, HIGH);
#    ifdef ENABLE_SD_CARD
    digitalWrite(SD_CS, HIGH);  // Solo si SD está habilitada
#    endif
#endif
    // Envia mensaje a la consola serial antes de inicializar la pantalla
    grbl_msg_sendf(CLIENT_SERIAL, MsgLevel::Info, "Init TFT ILI9341 by Kosey");

    //Inicialización de la pantalla
    tft.begin();
    tft.invertDisplay(true);
    tft.setRotation(1);  // Configuración de rotación horizontal
    tft.setTextSize(1);
    tft.fillScreen(TFT_BLACK);
    if (!SD.begin())
        existSD = false;
    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE)
        existSD = false;
    ui_state = tft_ui::UI_HOME;
    // Crear la tarea de actualización de la UI
    xTaskCreatePinnedToCore(displayUpdate, "displayUpdateTask", 8192, NULL, 1, &displayUpdateTaskHandle, APP_CPU_NUM);
}

// ===============================================
//  Funcion para dibujar imagenes
// ===============================================

void jpegRender(int xpos, int ypos) {
    //jpegInfo(); // Print information from the JPEG file (could comment this line out)

    uint16_t* pImg;
    uint16_t  mcu_w = JpegDec.MCUWidth;
    uint16_t  mcu_h = JpegDec.MCUHeight;
    uint32_t  max_x = JpegDec.width;
    uint32_t  max_y = JpegDec.height;

    bool swapBytes = tft.getSwapBytes();
    tft.setSwapBytes(true);

    // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
    // Typically these MCUs are 16x16 pixel blocks
    // Determine the width and height of the right and bottom edge image blocks
    uint32_t min_w = jpg_min(mcu_w, max_x % mcu_w);
    uint32_t min_h = jpg_min(mcu_h, max_y % mcu_h);

    // save the current image block size
    uint32_t win_w = mcu_w;
    uint32_t win_h = mcu_h;

    // record the current time so we can measure how long it takes to draw an image
    uint32_t drawTime = millis();

    // save the coordinate of the right and bottom edges to assist image cropping
    // to the screen size
    max_x += xpos;
    max_y += ypos;

    // Fetch data from the file, decode and display
    while (JpegDec.read()) {    // While there is more data in the file
        pImg = JpegDec.pImage;  // Decode a MCU (Minimum Coding Unit, typically a 8x8 or 16x16 pixel block)

        // Calculate coordinates of top left corner of current MCU
        int mcu_x = JpegDec.MCUx * mcu_w + xpos;
        int mcu_y = JpegDec.MCUy * mcu_h + ypos;

        // check if the image block size needs to be changed for the right edge
        if (mcu_x + mcu_w <= max_x)
            win_w = mcu_w;
        else
            win_w = min_w;

        // check if the image block size needs to be changed for the bottom edge
        if (mcu_y + mcu_h <= max_y)
            win_h = mcu_h;
        else
            win_h = min_h;

        // copy pixels into a contiguous block
        if (win_w != mcu_w) {
            uint16_t* cImg;
            int       p = 0;
            cImg        = pImg + win_w;
            for (int h = 1; h < win_h; h++) {
                p += mcu_w;
                for (int w = 0; w < win_w; w++) {
                    *cImg = *(pImg + w + p);
                    cImg++;
                }
            }
        }

        // calculate how many pixels must be drawn
        uint32_t mcu_pixels = win_w * win_h;

        // draw image MCU block only if it will fit on the screen
        if ((mcu_x + win_w) <= tft.width() && (mcu_y + win_h) <= tft.height())
            tft.pushImage(mcu_x, mcu_y, win_w, win_h, pImg);
        else if ((mcu_y + win_h) >= tft.height())
            JpegDec.abort();  // Image has run off bottom of screen so abort decoding
    }

    tft.setSwapBytes(swapBytes);
}

void drawSdJpeg(const char* filename, int xpos, int ypos) {
    // Open the named file (the Jpeg decoder library will close it)
    if (existSD) {
        File jpegFile = SD.open(filename, FILE_READ);  // or, file handle reference for SD library

        if (!jpegFile) {
            Serial.print("ERROR: File \"");
            Serial.print(filename);
            Serial.println("\" not found!");
            return;
        }

        bool decoded = JpegDec.decodeSdFile(jpegFile);

        if (decoded) {
            jpegRender(xpos, ypos);
        } else {
            Serial.println("Jpeg file format not supported!");
        }
    } else {
        tft.drawRect(xpos, ypos, 20, 20, TFT_WHITE);
    }
}

// ===============================================
//  Logica botones
// ===============================================
// Dibujar botones
void ui_draw_button(int numButton, bool active) {
    tft_ui::draw_button(tft, buttons[numButton], active, drawSdJpeg);
}

// Detectar touch botones
int ui_get_touched_button(uint16_t tx, uint16_t ty) {
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        auto& b = buttons[i];
        if (tx > b.x1 && tx < b.x2 && ty > b.y1 && ty < b.y2) {
            return i;
        }
    }
    return -1;
}

// ===============================================
//  Funcion para estructura del menu
// ===============================================
// Revisa conexión a internet

enum WifiIconState { ICON_WIFI_OFF, ICON_WIFI_AP, ICON_WIFI_STA_DISCONNECTED, ICON_WIFI_STA_CONNECTED };

WifiIconState getWifiState() {
    wifi_mode_t mode = WiFi.getMode();

    if (mode == WIFI_MODE_NULL) {
        return ICON_WIFI_OFF;
    }

    if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
        return ICON_WIFI_AP;
    }

    if (mode == WIFI_MODE_STA) {
        if (WiFi.status() == WL_CONNECTED)
            return ICON_WIFI_STA_CONNECTED;
        else
            return ICON_WIFI_STA_DISCONNECTED;
    }

    return ICON_WIFI_OFF;
}

void ui_update_wifi_icon() {
    WifiIconState st = getWifiState();

    switch (st) {
        case ICON_WIFI_OFF:
            ui_draw_button(5, false);
            break;

        case ICON_WIFI_AP:  // Access point
            ui_draw_button(5, true);
            break;

        case ICON_WIFI_STA_DISCONNECTED:
            ui_draw_button(5, false);
            break;

        case ICON_WIFI_STA_CONNECTED:
            ui_draw_button(5, true);
            break;
    }
}

void ui_draw_menu(const char* title) {
    tft_ui::draw_menu_shell(tft, title);
    ui_draw_button(4, true);  // MicroSD Icon
    ui_update_wifi_icon();
}

// ===============================================
//  Pantallas logica
// ===============================================
void ui_show_work() {
    sd_get_current_filename(currentFilename);
    ui_draw_menu(currentFilename);
    ui_draw_button(0, true);   // Home Icon
    ui_draw_button(1, false);  // Folder Icon
    ui_draw_button(2, false);  // Control Icon
    ui_draw_button(3, false);  // Config Icon
    // Estado
    tft.fillRect(45, 41, 150, 33, tft_ui::STATUS_BAR_COLOR);
    // Coords containers
    // X
    tft.fillRect(45, 85, 150, 20, tft_ui::CONTAINER_COORDS_COLOR);
    tft.drawRect(45, 85, 150, 20, tft_ui::CONTAINER_BORD_COORDS_COLOR);
    // Y
    tft.fillRect(45, 120, 150, 20, tft_ui::CONTAINER_COORDS_COLOR);
    tft.drawRect(45, 120, 150, 20, tft_ui::CONTAINER_BORD_COORDS_COLOR);
    // Z
    tft.fillRect(45, 155, 150, 20, tft_ui::CONTAINER_COORDS_COLOR);
    tft.drawRect(45, 155, 150, 20, tft_ui::CONTAINER_BORD_COORDS_COLOR);
    // Progress Bar
    tft.fillRoundRect(55, 195, 250, 15, 7, tft_ui::PROGRESS_BAR_BACKGROUND);
    // Relleno Progress Bar
    tft.fillRoundRect(55, 195, 0, 15, 7, tft_ui::PROGRESS_BAR_FILL);

    // Panel control
    tft.fillRect(205, 65, 50, 50, tft_ui::MAIN_BUTTON_COLOR);   // Temperatura
    tft.fillRect(260, 65, 50, 50, tft_ui::MAIN_BUTTON_COLOR);   // Ventilador
    tft.fillRect(205, 125, 50, 50, tft_ui::MAIN_BUTTON_COLOR);  // Play / Pause
    tft.fillRect(260, 125, 50, 50, tft_ui::MAIN_BUTTON_COLOR);  // Stop

    // Textos Iniciales
    // Estado
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_BLACK, tft_ui::STATUS_BAR_COLOR);
    tft.setTextSize(2);
    tft.drawString("DESCONECTADO", 121, 58);
    // Coordenadas
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(tft_ui::CONTAINER_BORD_COORDS_COLOR, tft_ui::CONTAINER_COORDS_COLOR);
    tft.setTextSize(1);
    tft.drawString("X", 50, 95);
    tft.setTextDatum(MR_DATUM);
    tft.drawString("0.000", 190, 95);

    tft.setTextDatum(ML_DATUM);
    tft.drawString("Y", 50, 130);
    tft.setTextDatum(MR_DATUM);
    tft.drawString("0.000", 190, 130);

    tft.setTextDatum(ML_DATUM);
    tft.drawString("Z", 50, 165);
    tft.setTextDatum(MR_DATUM);
    tft.drawString("0.000", 190, 165);

    // Progress Bar
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(tft_ui::PROGRESS_BAR_FILL, TFT_BLACK);
    tft.drawString("0%", 60, 220);

    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("00m 00s", 300, 220);

    // Titulo proyecto
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("SD CARD", 257, 51);

    // Tarjetas Info maquina
    tft.setTextColor(TFT_WHITE, tft_ui::MAIN_BUTTON_COLOR);
    drawSdJpeg("/ui-main-icon/temperatura.jpg", 218, 72);
    tft.drawString("0%", 230, 105);
    drawSdJpeg("/ui-main-icon/ventilador.jpg", 273, 72);
    tft.drawString("255", 285, 105);
    ui_draw_button(6, true);
    tft.drawString("Pausa", 230, 165);
    ui_draw_button(7, false);
    tft.drawString("Parar", 285, 165);
}

void ui_draw_ctrl_btn(int x, int y, int w, int h, const char* label) {
    tft_ui::draw_ctrl_btn(tft, x, y, w, h, label);
}

void ui_show_control() {
    ui_draw_menu("Control JOG");
    ui_draw_button(0, false);  // Home Icon
    ui_draw_button(1, false);  // Folder Icon
    ui_draw_button(2, true);   // Control Icon
    ui_draw_button(3, false);  // Config Icon

    // Visualizar Coordenadas
    // Contenedores
    // X
    tft.fillRect(48, 40, 84, 20, tft_ui::CONTAINER_COORDS_COLOR);
    tft.drawRect(48, 40, 84, 20, tft_ui::CONTAINER_BORD_COORDS_COLOR);
    // Y
    tft.fillRect(138, 40, 84, 20, tft_ui::CONTAINER_COORDS_COLOR);
    tft.drawRect(138, 40, 84, 20, tft_ui::CONTAINER_BORD_COORDS_COLOR);
    // Z
    tft.fillRect(228, 40, 84, 20, tft_ui::CONTAINER_COORDS_COLOR);
    tft.drawRect(228, 40, 84, 20, tft_ui::CONTAINER_BORD_COORDS_COLOR);
    // Valores
    // X
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(tft_ui::CONTAINER_BORD_COORDS_COLOR, tft_ui::CONTAINER_COORDS_COLOR);
    tft.setTextSize(1);
    tft.drawString("X", 53, 50);
    tft.setTextDatum(MR_DATUM);
    tft.drawString("0.000", 130, 50);
    // Y
    tft.setTextDatum(ML_DATUM);
    tft.drawString("Y", 143, 50);
    tft.setTextDatum(MR_DATUM);
    tft.drawString("0.000", 220, 50);
    // Z
    tft.setTextDatum(ML_DATUM);
    tft.drawString("Z", 233, 50);
    tft.setTextDatum(MR_DATUM);
    tft.drawString("0.000", 310, 50);
    // Botones de control JOG
    int w = 50;
    int h = 50;
    ui_draw_ctrl_btn(55, 75, w, h, "X- Y+");   // Diagonal Izq-Arr
    ui_draw_ctrl_btn(105, 75, w, h, "Y+");     // Arriba
    ui_draw_ctrl_btn(155, 75, w, h, "X+ Y+");  // Diagonal Der-Arr

    // FILA MEDIA (X)
    ui_draw_ctrl_btn(55, 125, w, h, "X-");     // Izquierda
    ui_draw_ctrl_btn(105, 125, w, h, "ZERO");  // Centro (Reset Zero)
    ui_draw_ctrl_btn(155, 125, w, h, "X+");    // Derecha

    // FILA INFERIOR (Y-)
    ui_draw_ctrl_btn(55, 175, w, h, "X- Y-");   // Diagonal Izq-Abj
    ui_draw_ctrl_btn(105, 175, w, h, "Y-");     // Abajo
    ui_draw_ctrl_btn(155, 175, w, h, "X+ Y-");  // Diagonal Der-Abj

    // COLUMNA Z (Derecha)
    ui_draw_ctrl_btn(245, 75, w, h, "Z+");     // Subir Z
    ui_draw_ctrl_btn(245, 125, w, h, "HOME");  // Ir a Casa (Go To Zero)
    ui_draw_ctrl_btn(245, 175, w, h, "Z-");    // Bajar Z
}

void ui_show_config() {
    ui_draw_menu("Configuracion");
    ui_draw_button(0, false);  // Home Icon
    ui_draw_button(1, false);  // Folder Icon
    ui_draw_button(2, false);  // Control Icon
    ui_draw_button(3, true);   // Config Icon
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("Config Screen", tft.width() / 2, tft.height() / 2);
}

void ui_show_home() {
    ui_draw_menu("Principal");
    ui_draw_button(0, true);   // Home Icon
    ui_draw_button(1, false);  // Folder Icon
    ui_draw_button(2, false);  // Control Icon
    ui_draw_button(3, false);  // Config Icon
}

// ===============================================
// Logica Loop Work
// ===============================================

float ui_get_laser_power() {
    // Verificar si el láser está apagado
    if (spindle->get_state() == SpindleState::Disable) {
        return 0.0f;
    }

    // Obtener valores
    float current_s = gc_state.spindle_speed;  // Valor S actual
    float max_s     = rpm_max->get();          // Valor $30
    float min_s     = rpm_min->get();          // Valor $31

    // Calcular porcentaje
    if (max_s <= min_s)
        return 0.0f;

    float percentage = ((current_s - min_s) / (max_s - min_s)) * 100.0f;

    // Limitar rango 0-100
    if (percentage > 100.0f)
        percentage = 100.0f;
    if (percentage < 0.0f)
        percentage = 0.0f;

    return percentage;
}

void ui_toggle_pause_resume() {
    static unsigned long lastButtonPress = 0;
    if (millis() - lastButtonPress < 500)
        return;  // Ignorar si han pasado menos de 500ms
    lastButtonPress = millis();

    // CASO 1: La máquina se está moviendo (Ciclo, Home o Jog) -> PAUSAR
    if (sys.state == State::Cycle || sys.state == State::Homing || sys.state == State::Jog) {
        tft_ui::Core::toggle_pause_resume();

        // Feedback visual forzado para debug
        tft.fillCircle(230, 140, 10, TFT_ORANGE);  // Pequeño punto naranja indicador
    }
    // CASO 2: La máquina está en espera (Hold) -> REANUDAR
    else if (sys.state == State::Hold) {
        tft_ui::Core::toggle_pause_resume();

        // Feedback visual forzado
        tft.fillCircle(230, 140, 10, TFT_GREEN);  // Pequeño punto verde indicador
    }
}

void ui_stop_job() {
    tft_ui::Manager::on_stop_requested(existSD);

    ui_state = tft_ui::UI_HOME;
    tft.fillScreen(tft_ui::BG_COLOR);
    tft.setTextColor(TFT_WHITE);
    tft.drawCentreString("CANCELADO", tft.width() / 2, tft.height() / 2, 4);
    delay(1000);
    tft.fillScreen(tft_ui::BG_COLOR);
    ui_show_home();
}

bool ui_start_selected_file() {
    if (selectedGCodeFile.length() > 0) {
        if (!tft_ui::Core::start_sd_job(selectedGCodeFile)) {
            // Manejar error (archivo no encontrado o sistema ocupado)
            return false;
        } else {
            // detectará el cambio a SDState::BusyPrinting y cambiará la pantalla solo.
            showConfirmDialog = false;
            return true;
        }
    }
    return false;
}

// Configuración de JOG
float jog_dist = 10.0;  // Distancia por defecto (mm)
int   jog_feed = 2000;  // Velocidad de movimiento (mm/min)
// Enviar comando de movimiento incremental ($J)
void ui_send_jog(float x, float y, float z) {
    tft_ui::Core::send_jog(x, y, z, jog_feed);
}

// Establecer el Cero de Trabajo actual (Reset Zero)
void ui_set_zero_all() {
    tft_ui::Core::set_zero_all();
}

// Mover la máquina al origen (Go To Zero)
void ui_go_to_zero() {
    tft_ui::Core::run_home_cycle();
}

void ui_handle_control_touch(uint16_t tx, uint16_t ty) {
    // --- DEFINICIÓN DE LA CUADRÍCULA ---
    // X inicio: 55, Ancho celda: 50
    // Y inicio: 75, Alto celda: 50

    // ZONA XY (Cuadrícula 3x3)
    // Rango X: 55 a 205 (55 + 50*3)
    // Rango Y: 75 a 225 (75 + 50*3)

    if (tx >= 55 && tx <= 205 && ty >= 75 && ty <= 225) {
        // Calcular columna (0, 1, 2) y fila (0, 1, 2)
        int col = (tx - 55) / 50;
        int row = (ty - 75) / 50;

        float moveX = 0;
        float moveY = 0;

        // --- Lógica de Dirección XY ---

        // FILA 0 (Superior): Y+
        if (row == 0)
            moveY = jog_dist;

        // FILA 2 (Inferior): Y-
        if (row == 2)
            moveY = -jog_dist;

        // COLUMNA 0 (Izquierda): X-
        if (col == 0)
            moveX = -jog_dist;

        // COLUMNA 2 (Derecha): X+
        if (col == 2)
            moveX = jog_dist;

        // --- Ejecución ---

        // Si es el CENTRO (Col 1, Row 1) -> RESET ZERO
        if (col == 1 && row == 1) {
            ui_set_zero_all();  // Llama a tu función G10 L20...
            delay(200);
            return;
        }

        // Si hay movimiento (Diagonales o Rectos)
        if (moveX != 0 || moveY != 0) {
            ui_send_jog(moveX, moveY, 0);  // Mover solo XY
            delay(100);                    // Anti-rebote
        }
        return;
    }

    // 2. ZONA Z y HOME (Columna derecha)
    // X: 245 a 295 (50px ancho)
    // Y: 75 a 225 (3 botones de 50px alto)

    if (tx >= 245 && tx <= 295 && ty >= 75 && ty <= 225) {
        int zRow = (ty - 75) / 50;  // 0=Arriba, 1=Medio, 2=Abajo

        // Botón Superior (75-125): Z+
        if (zRow == 0) {
            ui_send_jog(0, 0, jog_dist);
            delay(100);
        }

        // Botón Medio (125-175): RETURN HOME
        else if (zRow == 1) {
            ui_go_to_zero();  // Llama a tu función G90 G0 X0 Y0
            delay(200);
        }

        // Botón Inferior (175-225): Z-
        else if (zRow == 2) {
            ui_send_jog(0, 0, -jog_dist);
            delay(100);
        }
        return;
    }

    // 3. CONTROL DE VALORES DRO (Opcional: Setear cero individualmente)
    // Si tocan los contenedores de coordenadas superiores para hacer cero un solo eje
    // X Container: 48, 40, 84, 20
    if (tx >= 48 && tx <= 132 && ty >= 40 && ty <= 60) {
        char cmd[] = "G10 P0 L20 X0";  // Buffer local
        system_execute_line(cmd, (uint8_t)CLIENT_SERIAL, WebUI::AuthenticationLevel::LEVEL_ADMIN);
        // ... feedback ...
    }
    // Reset solo Y
    else if (tx >= 138 && tx <= 222 && ty >= 40 && ty <= 60) {
        char cmd[] = "G10 P0 L20 Y0";
        system_execute_line(cmd, (uint8_t)CLIENT_SERIAL, WebUI::AuthenticationLevel::LEVEL_ADMIN);
        // ... feedback ...
    }
    // Reset solo Z
    else if (tx >= 228 && tx <= 312 && ty >= 40 && ty <= 60) {
        char cmd[] = "G10 P0 L20 Z0";
        system_execute_line(cmd, (uint8_t)CLIENT_SERIAL, WebUI::AuthenticationLevel::LEVEL_ADMIN);
        // ... feedback ...
    }
}

void loopWork() {
    // Actualizar estado maquina
    switch (sys.state) {
        case State::Idle:
            tft.fillRect(45, 41, 150, 33, tft_ui::STATUS_BAR_COLOR);  // Limpiar Contenedor
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_BLACK, tft_ui::STATUS_BAR_COLOR);
            tft.setTextSize(2);
            tft.drawString("IDLE", 121, 58);
            tft.setTextSize(1);
            break;
        case State::Alarm:
            tft.fillRect(45, 41, 150, 33, TFT_RED);  // Limpiar Contenedor
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_BLACK, TFT_RED);
            tft.setTextSize(2);
            tft.drawString("ALARM", 121, 58);
            tft.setTextSize(1);
            break;
        case State::CheckMode:
            tft.fillRect(45, 41, 150, 33, tft_ui::STATUS_BAR_COLOR);  // Limpiar Contenedor
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_BLACK, tft_ui::STATUS_BAR_COLOR);
            tft.setTextSize(2);
            tft.drawString("CHECKMODE", 121, 58);
            tft.setTextSize(1);
            break;
        case State::Homing:
            tft.fillRect(45, 41, 150, 33, tft_ui::STATUS_BAR_COLOR);  // Limpiar Contenedor
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_BLACK, tft_ui::STATUS_BAR_COLOR);
            tft.setTextSize(2);
            tft.drawString("HOMING", 121, 58);
            tft.setTextSize(1);
            break;
        case State::Cycle:
            tft.fillRect(45, 41, 150, 33, TFT_GREEN);  // Limpiar Contenedor
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_BLACK, TFT_GREEN);
            tft.setTextSize(2);
            tft.drawString("RUN", 121, 58);
            tft.setTextSize(1);
            break;
        case State::Hold:
            tft.fillRect(45, 41, 150, 33, TFT_ORANGE);  // Limpiar Contenedor
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_BLACK, TFT_ORANGE);
            tft.setTextSize(2);
            tft.drawString("HOLD", 121, 58);
            tft.setTextSize(1);
            break;
        case State::Jog:
            tft.fillRect(45, 41, 150, 33, TFT_GREEN);  // Limpiar Contenedor
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_BLACK, TFT_GREEN);
            tft.setTextSize(2);
            tft.drawString("JOG", 121, 58);
            tft.setTextSize(1);
            break;
        case State::SafetyDoor:
            tft.fillRect(45, 41, 150, 33, tft_ui::STATUS_BAR_COLOR);  // Limpiar Contenedor
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_BLACK, tft_ui::STATUS_BAR_COLOR);
            tft.setTextSize(2);
            tft.drawString("SAFETYDOOR", 121, 58);
            tft.setTextSize(1);
            break;
        case State::Sleep:
            tft.fillRect(45, 41, 150, 33, tft_ui::STATUS_BAR_COLOR);  // Limpiar Contenedor
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_BLACK, tft_ui::STATUS_BAR_COLOR);
            tft.setTextSize(2);
            tft.drawString("SLEEP", 121, 58);
            tft.setTextSize(1);
            break;
        default:
            tft.fillRect(45, 41, 150, 33, TFT_RED);  // Limpiar Contenedor
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_BLACK, TFT_RED);
            tft.setTextSize(2);
            tft.drawString("ERROR", 121, 58);
            tft.setTextSize(1);
            break;
    }
    // Actualizar Coordenas (DRO)
    /*uint8_t idx;
    auto    n_axis = number_axis->get();
    float*  wco    = get_wco();
    // Convierte las posiciones de pasos a mm
    system_convert_array_steps_to_mpos(machine_position, sys_position);
    for (idx = 0; idx < n_axis; idx++) {
        work_position[idx] = machine_position[idx] - wco[idx];
    }*/

    float* mpos = system_get_mpos();
    float* wco  = get_wco();
    // Calcular WPos
    float w_x = mpos[X_AXIS] - wco[X_AXIS];
    float w_y = mpos[Y_AXIS] - wco[Y_AXIS];
    float w_z = mpos[Z_AXIS] - wco[Z_AXIS];
    // Coords containers
    // X
    tft.fillRect(45, 85, 150, 20, tft_ui::CONTAINER_COORDS_COLOR);
    tft.drawRect(45, 85, 150, 20, tft_ui::CONTAINER_BORD_COORDS_COLOR);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(tft_ui::CONTAINER_BORD_COORDS_COLOR, tft_ui::CONTAINER_COORDS_COLOR);
    tft.setTextSize(1);
    tft.drawString("X", 50, 95);
    tft.setTextDatum(MR_DATUM);
    tft.drawFloat(w_x, 3, 190, 95);
    // Y
    tft.fillRect(45, 120, 150, 20, tft_ui::CONTAINER_COORDS_COLOR);
    tft.drawRect(45, 120, 150, 20, tft_ui::CONTAINER_BORD_COORDS_COLOR);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("Y", 50, 130);
    tft.setTextDatum(MR_DATUM);
    tft.drawFloat(w_y, 3, 190, 130);
    // Z
    tft.fillRect(45, 155, 150, 20, tft_ui::CONTAINER_COORDS_COLOR);
    tft.drawRect(45, 155, 150, 20, tft_ui::CONTAINER_BORD_COORDS_COLOR);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("Z", 50, 165);
    tft.setTextDatum(MR_DATUM);
    tft.drawFloat(w_z, 3, 190, 165);

    // Ver estaods
    const int BAR_X      = 55;
    const int BAR_Y      = 195;
    const int BAR_WIDTH  = 250;
    const int BAR_HEIGHT = 15;
    if (get_sd_state(false) == SDState::BusyPrinting) {
        // --- MODO TRABAJO ---
        // Barra de Progreso
        float percentage = sd_report_perc_complete();
        if (percentage > 100.0f)
            percentage = 100.0f;
        int barWidth = (int)(250.0f * (percentage / 100.0f));

        // Dibujar solo si cambia para evitar parpadeo excesivo
        tft.fillRoundRect(55, 195, barWidth, 15, 7, tft_ui::PROGRESS_BAR_FILL);

        // Texto de porcentaje
        tft.setTextSize(1);
        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(tft_ui::PROGRESS_BAR_FILL, TFT_BLACK);
        String percStr = String((int)percentage) + "%";
        tft.drawString(percStr, 60, 220);

        // --- Calculo Tiempos ---
        unsigned long currentMillis = millis();

        // Detectar si entramos en PAUSA (Hold)
        if (sys.state == State::Hold) {
            if (!runtimeState.timer_paused) {
                // Acabamos de entrar en pausa, guardamos el momento exacto
                runtimeState.pause_start_timestamp = currentMillis;
                runtimeState.timer_paused          = true;
            }
        }
        // Detectar si salimos de PAUSA (Reanudamos)
        else {
            if (runtimeState.timer_paused) {
                // Acabamos de reanudar, sumamos lo que duró la pausa al acumulador
                runtimeState.total_pause_time += (currentMillis - runtimeState.pause_start_timestamp);
                runtimeState.timer_paused = false;
            }
        }
        // Calcular Tiempo Efectivo (Tiempo real - Pausas)
        unsigned long elapsedMillis;

        if (runtimeState.timer_paused) {
            // Si está pausado, congelamos el tiempo visual en el momento que pausó
            elapsedMillis = runtimeState.pause_start_timestamp - runtimeState.job_start_time - runtimeState.total_pause_time;
        } else {
            // Si está corriendo, es el tiempo actual menos el inicio y las pausas previas
            elapsedMillis = currentMillis - runtimeState.job_start_time - runtimeState.total_pause_time;
        }

        unsigned long elapsedSecs   = elapsedMillis / 1000;
        unsigned long remainingSecs = 0;
        if (percentage > 1.0f) {
            // Regla de tres
            unsigned long totalEstSecs = (unsigned long)((float)elapsedSecs * 100.0f / percentage);
            if (totalEstSecs > elapsedSecs) {
                remainingSecs = totalEstSecs - elapsedSecs;
            }
        }

        // --- DIBUJAR ---
        char timeStr[32];
        sprintf(timeStr, "%02lu:%02lu / -%02lu:%02lu", (elapsedSecs / 60), (elapsedSecs % 60), (remainingSecs / 60), (remainingSecs % 60));

        // Tiempo imprimir
        tft.setTextDatum(MR_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(timeStr, 300, 220);

        // Potencia de husillo:
        float laserPower = ui_get_laser_power();
        float currentS   = gc_state.spindle_speed;

        static float lastPower = -1.0;
        static float lastS     = -1.0;
        if (abs(laserPower - lastPower) > 0.5 || currentS != lastS) {
            // Texto Porcentaje
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_WHITE, tft_ui::MAIN_BUTTON_COLOR);
            tft.drawString(String((int)laserPower) + "%", 230, 105);
            // Actualizar estado previo
            lastPower = laserPower;
            lastS     = currentS;
        }
    }
    static State lastStateDraw = State::Idle;
    const int    btnX          = 205;
    const int    btnY          = 125;

    if (sys.state == State::Hold) {
        if (lastStateDraw != State::Hold) {
            tft.fillRect(btnX, btnY, 50, 50, tft_ui::MAIN_BUTTON_COLOR);

            ui_draw_button(6, false);

            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_WHITE, tft_ui::MAIN_BUTTON_COLOR);
            tft.fillRect(205, 160, 40, 10, tft_ui::MAIN_BUTTON_COLOR);
            tft.drawString("Reanudar", 230, 165);

            lastStateDraw = State::Hold;  // Actualizar estado dibujado
        }
    }
    // CASO B: CORRIENDO (CYCLE / HOMING / JOG) -> Mostrar botón "PAUSA"
    else if (sys.state == State::Cycle || sys.state == State::Homing || sys.state == State::Jog) {
        // Si el estado cambió (ej. de Hold a Cycle, o de Idle a Cycle)
        // Usamos una lógica para agrupar estados de movimiento
        bool isMoving = true;
        // Comparamos contra el último estado dibujado específico
        // Si lo último dibujado NO fue "Movimiento", redibujamos a Pausa
        if (lastStateDraw == State::Hold || lastStateDraw == State::Idle) {
            tft.fillRect(btnX, btnY, 50, 50, tft_ui::MAIN_BUTTON_COLOR);
            ui_draw_button(6, true);

            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_WHITE, tft_ui::MAIN_BUTTON_COLOR);
            tft.fillRect(205, 160, 40, 10, tft_ui::MAIN_BUTTON_COLOR);  // Limpiar texto previo
            tft.drawString("Pausa", 230, 165);

            lastStateDraw = State::Cycle;  // Marcamos como "en movimiento"
        }
    }
}

void loopHome() {
    if (get_sd_state(false) == SDState::BusyPrinting) {
        loopWork();
    } else {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE);
        tft.drawString("Sin Imprimir", tft.width() / 2, tft.height() / 2);
    }
}

void loopJOG(const tft_ui::TouchEvent& touch) {
    // 1. Obtener posición actual
    float* mpos = system_get_mpos();
    float* wco  = get_wco();

    // Calcular posición de trabajo (Work Position)
    float w_x = mpos[X_AXIS] - wco[X_AXIS];
    float w_y = mpos[Y_AXIS] - wco[Y_AXIS];
    float w_z = mpos[Z_AXIS] - wco[Z_AXIS];

    // 2. Actualizar valores en pantalla (Solo los números)
    tft.setTextSize(1);
    tft.setTextColor(tft_ui::CONTAINER_BORD_COORDS_COLOR, tft_ui::CONTAINER_COORDS_COLOR);

    // Usamos fillRect para limpiar solo el área del número antes de escribir
    // X
    tft.setTextDatum(MR_DATUM);
    tft.fillRect(80, 42, 50, 16, tft_ui::CONTAINER_COORDS_COLOR);
    tft.drawFloat(w_x, 3, 130, 50);

    // Y
    tft.fillRect(170, 42, 50, 16, tft_ui::CONTAINER_COORDS_COLOR);
    tft.drawFloat(w_y, 3, 220, 50);

    // Z
    tft.fillRect(260, 42, 50, 16, tft_ui::CONTAINER_COORDS_COLOR);
    tft.drawFloat(w_z, 3, 310, 50);

    if (touch.pressed) {
        ui_handle_control_touch(touch.x, touch.y);
    }
}

// -------------- Media ---------------
enum FileType { FILE_TYPE_DIRECTORY, FILE_TYPE_GCODE, FILE_TYPE_IMAGE, FILE_TYPE_TXT, FILE_TYPE_OTHER };

struct FileEntry {
    String   name;
    FileType type;
};

FileEntry fileList[50];  // Máximo 50 archivos por carpeta
uint8_t   fileCount   = 0;
uint8_t   scrollIndex = 0;  // Para paginar / scroll
String    currentPath = "/";

FileType getFileType(const String& filename, bool isDir) {
    if (isDir) {
        return FILE_TYPE_DIRECTORY;
    }

    String upperName = filename;
    upperName.toUpperCase();  // Convierte a mayúsculas para una comprobación insensible a mayúsculas

    if (upperName.endsWith(".GCODE") || upperName.endsWith(".NC")) {
        return FILE_TYPE_GCODE;
    }

    if (upperName.endsWith(".JPG") || upperName.endsWith(".JPEG")) {
        return FILE_TYPE_IMAGE;
    }

    if (upperName.endsWith(".TXT")) {
        return FILE_TYPE_TXT;
    }

    return FILE_TYPE_OTHER;
}

void scanDirectory(const char* path) {
    if (existSD) {
        fileCount   = 0;
        currentPath = path;
        currentPath += "/";
        File dir = SD.open(path);
        if (!dir || !dir.isDirectory()) {
            return;
        }

        File entry;
        while ((entry = dir.openNextFile())) {
            if (fileCount >= 50)
                break;

            String fullName  = entry.name();
            int    lastSlash = fullName.lastIndexOf('/');
            String shortName;
            if (lastSlash != -1) {
                shortName = fullName.substring(lastSlash + 1);
            } else {
                shortName = fullName;  // Si no hay ruta, usar el nombre completo
            }
            fileList[fileCount].name = shortName;
            fileList[fileCount].type = getFileType(fileList[fileCount].name, entry.isDirectory());
            fileCount++;

            entry.close();
        }
        dir.close();
    }
}

void ui_draw_scroll_button(int x, int y, bool isUp) {
    // Aquí puedes dibujar un triángulo, o usar un ícono de tu SD.
    if (isUp) {
        tft.fillTriangle(x + 15, y - 10, x + 5, y + 10, x + 25, y + 10, TFT_WHITE);
    } else {
        tft.fillTriangle(x + 15, y + 30, x + 5, y + 10, x + 25, y + 10, TFT_WHITE);
    }
}

void ui_show_media() {
    if (currentPath == "//") {
        ui_draw_menu("Archivos SD");
    } else {
        ui_draw_menu(currentPath.c_str());
    }
    ui_draw_button(0, false);  // Home Icon
    ui_draw_button(1, true);   // Folder Icon
    ui_draw_button(2, false);  // Control Icon
    ui_draw_button(3, false);  // Config Icon
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE);
    tft.fillRect(40, 30, 280, 210, tft_ui::BG_COLOR);

    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, tft_ui::BG_COLOR);
    tft.setTextDatum(TL_DATUM);

    uint8_t maxLines = 8;
    for (uint8_t i = 0; i < maxLines; i++) {
        uint8_t idx = scrollIndex + i;
        if (idx >= fileCount)
            break;
        int y = 40 + i * 25;
        // Dibujar icono carpeta / archivo etc
        switch (fileList[idx].type) {
            case FILE_TYPE_DIRECTORY:
                drawSdJpeg("/ui-files-icons/A_carpeta.jpg", 45, y);
                break;
            case FILE_TYPE_GCODE:
                drawSdJpeg("/ui-files-icons/A_gcode.jpg", 45, y);
                break;
            case FILE_TYPE_IMAGE:
                drawSdJpeg("/ui-files-icons/A_imagen.jpg", 45, y);
                break;
            case FILE_TYPE_TXT:
                drawSdJpeg("/ui-files-icons/A_txt.jpg", 45, y);
                break;
            default:  // FILE_TYPE_OTHER
                drawSdJpeg("/ui-files-icons/A_desconocido.jpg", 45, y);
                break;
        }
        // Nombre
        tft.drawString(fileList[idx].name, 80, y + 5);
    }

    tft.fillRect(280, 40, 50, 200, tft_ui::BG_COLOR);
    // Flecha arriba
    if (scrollIndex > 0) {
        // Contenedor flechas
        ui_draw_scroll_button(280, 50, true);
    }
    //Flecha Abajo
    if ((fileCount > maxLines && (scrollIndex + maxLines) < fileCount)) {
        ui_draw_scroll_button(280, 200, false);
    }
}

void ui_draw_confirm_dialog() {
    if (!showConfirmDialog)
        return;

    // 1. Dibujar el fondo del diálogo (Ejemplo: gris claro con borde)
    tft.fillRect(DIALOG_X, DIALOG_Y, DIALOG_W, DIALOG_H, tft_ui::BG_COLOR);
    tft.drawRect(DIALOG_X, DIALOG_Y, DIALOG_W, DIALOG_H, TFT_WHITE);

    // 2. Título / Mensaje
    tft.setTextDatum(MC_DATUM);  // Centro Medio
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, tft_ui::BG_COLOR);

    // Primera línea de texto
    tft.drawString("Iniciar trabajo?", DIALOG_X + DIALOG_W / 2, DIALOG_Y + 25);

    // Nombre del archivo (en una fuente o color diferente)
    tft.setTextColor(TFT_BLUE, tft_ui::BG_COLOR);
    tft.drawString(selectedGCodeFile, DIALOG_X + DIALOG_W / 2, DIALOG_Y + 50);

    // 3. Botón "SÍ" (Iniciar)
    int btnY = DIALOG_Y + 100;
    tft.fillRect(DIALOG_X + 20, btnY, 80, 40, tft_ui::PROGRESS_BAR_FILL);
    tft.setTextColor(TFT_WHITE, tft_ui::PROGRESS_BAR_FILL);
    tft.drawString("Si", DIALOG_X + 20 + 40, btnY + 20);

    // 4. Botón "NO" (Cancelar)
    tft.fillRect(DIALOG_X + 120, btnY, 80, 40, tft_ui::MAIN_BUTTON_COLOR);
    tft.setTextColor(TFT_WHITE, tft_ui::MAIN_BUTTON_COLOR);
    tft.drawString("No", DIALOG_X + 120 + 40, btnY + 20);
}

void ui_handle_media_touch(uint16_t tx, uint16_t ty) {
    // --- Manejo del desplazamiento (Scroll) ---
    uint8_t   maxLines         = 8;
    const int SCROLL_BTN_X_MIN = 270;  // Coordenada X mínima del botón de scroll
    const int SCROLL_BTN_X_MAX = 310;  // Coordenada X máxima del botón de scroll

    if (tx >= SCROLL_BTN_X_MIN && tx <= SCROLL_BTN_X_MAX) {
        // Boton de flecha arriba
        if (ty >= 30 && ty <= 70) {
            if (scrollIndex > 0) {
                // Retrocede una línea (o un bloque completo si quieres paginar rápido)
                scrollIndex--;
                ui_show_media();  // Redibuja la lista
                return;
            }
        }

        // Boton de flecha abajo
        if (ty >= 180 && ty <= 220) {
            // El scrollIndex solo puede avanzar si quedan archivos ocultos
            if (fileCount > maxLines && (scrollIndex + maxLines) < fileCount) {
                // Avanza una linea
                scrollIndex++;
                ui_show_media();
                return;
            }
        }
        return;
    }

    if (tx < 40 || ty < 40)
        return;  // Fuera del area lista de archivos

    uint8_t line = (ty - 40) / 25;
    uint8_t idx  = scrollIndex + line;

    if (idx >= fileCount)
        return;

    if (fileList[idx].type == FILE_TYPE_DIRECTORY) {
        // Entrar a la carpeta
        String newPath = currentPath + fileList[idx].name;
        scanDirectory(newPath.c_str());
        ui_show_media();
    } else {
        // Selección de archivo GCODE
        if (fileList[idx].type == FILE_TYPE_GCODE) {
            // Guardar el nombre completo del archivo
            selectedGCodeFile = currentPath;
            if (selectedGCodeFile.endsWith("/") == false)
                selectedGCodeFile += "/";
            selectedGCodeFile += fileList[idx].name;

            // Activar la ventana emergente
            showConfirmDialog = true;
            ui_draw_confirm_dialog();
            return;
        }
    }
}

void ui_handle_dialog_touch(uint16_t tx, uint16_t ty) {
    if (!showConfirmDialog)
        return;

    // Coordenadas de los botones (ajustar si cambiaste DIALOG_X/Y/W/H)
    int btnY = DIALOG_Y + 100;

    // Botón "Si"
    if (tx >= DIALOG_X + 20 && tx <= DIALOG_X + 100 && ty >= btnY && ty <= btnY + 40) {
        showConfirmDialog = false;
        if (ui_start_selected_file()) {
            // - myFile = archivo abierto
            // - sd_state = SDState::BusyPrinting
            // - sd_current_line_number = 0
            ui_state = tft_ui::UI_HOME;
            ui_show_work();
        } else {
            showConfirmDialog = false;
            ui_show_media();
        }
        return;
    }

    // Boton "No"
    if (tx >= DIALOG_X + 120 && tx <= DIALOG_X + 200 && ty >= btnY && ty <= btnY + 40) {
        // ACCIÓN: CANCELAR
        showConfirmDialog = false;
        ui_show_media();  // Redibujar la lista limpia
        return;
    }
}

void loopMedia(const tft_ui::TouchEvent& touch) {
    if (touch.pressed) {
        if (showConfirmDialog) {
            ui_handle_dialog_touch(touch.x, touch.y);  // Manejar el diálogo
        } else {
            ui_handle_media_touch(touch.x, touch.y);  // Manejar la lista de archivos
        }
    }
}

void ui_loop() {
    // ============================================================
    //  MONITOR DE ESTADO GLOBAL
    // ============================================================
    static SDState prevSDState = SDState::Idle;
    SDState        currSDState = get_sd_state(false);  // Leer estado actual

    // Detectar si hubo un cambio en el estado de la SD
    if (currSDState != prevSDState) {
        // CASO A: Acaba de empezar a imprimir (desde WebUI, Serial o TFT)
        if (currSDState == SDState::BusyPrinting) {
            tft_ui::Manager::on_job_started(runtimeState);
            ui_state = tft_ui::UI_HOME;               // Forzar cambio al estado HOME
            tft.fillScreen(tft_ui::BG_COLOR);  // Limpiar pantalla
            ui_show_work();                           // Cargar interfaz de Trabajo/Progreso
        }
        // CASO B: Acaba de terminar de imprimir (o se canceló)
        else if (prevSDState == SDState::BusyPrinting) {
            if (get_sd_state(true) == SDState::Idle) {
                existSD = true;
            } else {
                existSD = false;
            }
            ui_state = tft_ui::UI_HOME;  // Volver a HOME
            tft.fillScreen(tft_ui::BG_COLOR);
            ui_show_home();  // Cargar interfaz de Inicio normal
        }

        // Actualizar el estado previo para el siguiente ciclo
        prevSDState = currSDState;
    }
    // ============================================================

    tft_ui::TouchEvent touch = touchReader.read(tft);

    // Máquina de Estados Normal
    switch (ui_state) {
        case tft_ui::UI_HOME:
            loopHome();
            break;
        case tft_ui::UI_MEDIA:
            loopMedia(touch);
            break;
        case tft_ui::UI_CONTROL:
            loopJOG(touch);
            break;
        default:
            break;
    }

    if (touch.pressed) {
        int btn = ui_get_touched_button(touch.x, touch.y);
        if (btn >= 0) {
            tft_ui::UIState next_state = ui_state;
            switch (btn) {
                case 0:
                    next_state = tft_ui::UI_HOME;
                    break;
                case 1:
                    next_state = tft_ui::UI_MEDIA;
                    break;
                case 2:
                    next_state = tft_ui::UI_CONTROL;
                    break;
                case 3:
                    next_state = tft_ui::UI_CONFIG;
                    break;
                case 6:
                    if (ui_state == tft_ui::UI_HOME) {
                        ui_toggle_pause_resume();
                    }
                    next_state = tft_ui::UI_HOME;
                    break;
                case 7:
                    if (ui_state == tft_ui::UI_HOME) {
                        ui_stop_job();
                    }
                    next_state = tft_ui::UI_HOME;
                    break;
                default:
                    next_state = tft_ui::NO_CHANGE;
                    break;
            }

            if (tft_ui::should_redraw_screen(ui_state, next_state)) {
                ui_state = next_state;
                switch (ui_state) {
                    case tft_ui::UI_HOME:
                        if (get_sd_state(false) == SDState::BusyPrinting) {
                            ui_show_work();
                        } else {
                            ui_show_home();
                        }
                        break;
                    case tft_ui::UI_MEDIA:
                        scrollIndex = 0;
                        scanDirectory("/");
                        ui_show_media();
                        break;
                    case tft_ui::UI_CONTROL:
                        ui_show_control();
                        break;
                    case tft_ui::UI_CONFIG:
                        ui_show_config();
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

void displayUpdate(void* pvParameters) {
    TickType_t       xLastWakeTime;
    const TickType_t xDisplayFrequency = pdMS_TO_TICKS(100);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextFont(1);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Interface designed by Kosey", tft.width() / 2, tft.height() / 2);
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString(GRBL_VERSION, tft.width() / 2, (tft.height() / 2) + 20);
    tft.setTextColor(TFT_WHITE);
    tft.setTextColor(TFT_WHITE);
    uint8_t  cardType = SD.cardType();
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    if (!existSD)
        tft.drawString("Card Mount Failed!", tft.width() / 2, (tft.height() / 2) + 60);
    if (cardType == CARD_NONE) {
        tft.drawString("No SD card attached!", tft.width() / 2, (tft.height() / 2) + 40);
    } else if (cardType == CARD_MMC) {
        tft.drawString("SD Type: MMC", tft.width() / 2, (tft.height() / 2) + 40);
    } else if (cardType == CARD_SD) {
        tft.drawString("SD Type: SDSC", tft.width() / 2, (tft.height() / 2) + 40);
    } else if (cardType == CARD_SDHC) {
        tft.drawString("SD Type: SDHC", tft.width() / 2, (tft.height() / 2) + 40);
    } else {
        tft.drawString("SD Type: UNKNOWN", tft.width() / 2, (tft.height() / 2) + 40);
    }
    String size_str = "SD Card Size: " + String((unsigned long)cardSize) + " MB";
    tft.drawString(size_str, tft.width() / 2, (tft.height() / 2) + 50);
    vTaskDelay(pdMS_TO_TICKS(3000));
    xLastWakeTime = xTaskGetTickCount();
    tft.setTextSize(1);
    tft.setTextFont(1);
    tft.fillScreen(TFT_BLACK);
    ui_show_home();
    // **********************************************
    // BUCLE DE ACTUALIZACIÓN (while(true))
    // **********************************************
    while (true) {
        ui_loop();
        vTaskDelayUntil(&xLastWakeTime, xDisplayFrequency);
    }
}