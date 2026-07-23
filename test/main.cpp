#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Typische Maße für das ESP32-C3 Slim OLED Board (72x40 oder 64x32)
#define SCREEN_WIDTH 72
#define SCREEN_HEIGHT 40

// Standard I2C-Pins für die meisten ESP32-C3 Boards
#define I2C_SDA 5
#define I2C_SCL 6

// OLED Reset-Pin (-1 bedeutet: teilt sich den Reset-Pin mit dem ESP32)
#define OLED_RESET -1
// Typische I2C-Adresse für diese Displays (oft 0x3C, seltener 0x3D)
#define SCREEN_ADDRESS 0x3D

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(115200);
  delay(1000); // Kurze Pause für den Serial Monitor
  Serial.println("Starte ESP32-C3 OLED Helligkeits-Test...");

  // 1. I2C Bus mit den korrekten ESP32-C3 Pins starten
  Wire.begin(I2C_SDA, I2C_SCL);

  // 2. Display initialisieren (SSD1306_SWITCHCAPVCC schaltet die interne Spannung ein)
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("OLED Display wurde nicht gefunden! Pins oder Adresse (0x3C) prüfen.");
    for(;;); // Stoppt das Programm bei Fehler
  }

  // =========================================================================
  // FIX FÜR DIE HELLIGKEIT (Erzwingt maximale Spannung & Kontrast)
  // =========================================================================
  display.ssd1306_command(0x8D);             // Befehl für die Ladungspumpe (Charge Pump)
  display.ssd1306_command(0x14);             // 0x14 aktiviert die Pumpe (0x10 würde sie abschalten = Display dunkel)
  
  display.ssd1306_command(SSD1306_SETCONTRAST); // Befehl für den Kontrastregler
  display.ssd1306_command(0xFF);             // 0xFF ist das absolute Maximum (Standard ist oft viel niedriger)
  // =========================================================================

  // Ersten Text anzeigen
  display.clearDisplay();
  display.setTextSize(1);                    // Textgröße (1 = klein, passt gut auf 72x40)
  display.setTextColor(SSD1306_WHITE);       // Textfarbe Weiß (bzw. Displayfarbe)
  display.setCursor(0, 0);                   // Start oben links
  display.println("ESP32-C3");
  display.setCursor(0, 15);
  display.println("OLED HELL!");
  display.display();                         // Befehl an das Display senden, um es anzuzeigen
}

void loop() {
  // Ein kleiner Counter, um zu sehen, dass das Display live aktualisiert
  static int counter = 0;
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("ESP32-C3");
  
  display.setCursor(0, 15);
  display.print("Status: ");
  display.println("Aktiv");
  
  display.setCursor(0, 30);
  display.print("Zaehler: ");
  display.println(counter++);
  
  display.display();
  
  delay(1000); // Jede Sekunde aktualisieren
}
