//
// Google Home / Alexa Enabled WiFi Thermostat
// -------------------------------------------
//
// Copyright (c) 2018 Claude G. Beaudoin (claudegbeaudoin @ hotmail.com)
//
// Programming level required:  Advanced
//
// Supported boards: Adafruit Feather ESP32, DOIT ESP32 DevKit, Heltec WiFi, Wemos Lolin32
// Code will need to be modified for other board types.
//
// NOTE:  I have modified Adafruit's libraries and they are included in this distribution.
//        The reason for this is mainly that Adafruit's display libraries have not yet been
//        updated to support the ESP32 boards.  You get compiler errors and some functions just
//        don't work anymore.  I'm sure they will get around to fixing them...
//
//        Also the Adafruit IO libraries.  Mainly the ->exist() and ->create() functions do not 
//        work and you had to hard code your WiFi network name / password along with your 
//        user name / API key.  The changes I have brought to the libraries are backwards
//        compatible, so nothing was broken in adding (fixing) these libraries.  
//        

//
// Code Revision History:
//
// MM-DD-YYYY---Version---Description--------------------------------------------------------
// 31-03-2018   v0.0.14   Added option to clear statistics and schedule in AP_Config()
//                        This is to allow changing of WiFi network without clearing
//                        historical data and schedule.
// 02-27-2018   v0.0.13   Code published to GitHub.
// 02-24-2018             Added code to create Adafruit "Home Heating" dashboard.
// 02-13-2018             Added code to broadcast schedule to all thermostats in network.
// 02-12-2018   v0.0.12   Finished scheduling page and supporting code to run schedule.
// 02-02-2018   v0.0.11   Finished statistical graphs.
//                        Modified Adafruit_IO libraries to be able to use configuration data instead of hard coding
//                        username and API key in the initial call to define the objects.
// 01-23-2018   v0.0.10   Added support for SH1106 OLED display.  These displays are 128x64 pixels so had to adjust
//                        several functions to accommodate.
// 01-20-2018   v0.0.9    Added https://io.adafruit.com connection to receive Google Home commands for thermostats.
//                        I've added this to prevent malicious hackers from posting to the Google dweet feed to
//                        change your thermostat setting.  If you don't specify an Adafruit Username/API Key in the initial
//                        setup, it will use dweet.io as the place to look for new Google Home commands.
// 01-09-2018   v0.0.8    Code cleanup and support for Celcius or Farenheit
//                        Added code for MASTER device support (this enables only one IFTTT script for all devices in network)
//                        Added web server service for configuration and reporting.
//                        Started working on graphs that are generated on the fly by https://livegap.com/
// 01-08-2018   v0.0.7    Changed when the thermostat goes on to 0.5 degrees below requested temp
// 01-07-2018   v0.0.6    Fixed bug where it was flooding dweet.io with post
// 12-26-2017   v0.0.5    Started working on code for Heltec WiFi module
//                        Added UpTime to dweet feed
//                        Added calculation of heating cost.  Needed to add new fields in the Config record,
//                        Heater size in watts, kWh cost in cents and total time the heater has been on.
//                        Added CRC checksum in Config record
//                        Added more error checking
//                        Completed support for DOIT ESP32 DevKit V1
//                        Added on some end case faults discovered with the DOIT ESP32 board specifically
//                        in the handling of WiFi functions.
// 12-24-2017   v0.0.4    Code cleanup and some additional error checking added.
// 12-23-2017   v0.0.3    Added code to display the Arduino Logo at start up.
//                        Finished code for dweet.io and Google home interface.
//                        Finished NTP time server code
// 12-17-2017   v0.0.2    Added code to get NTP server time
//                        Added code to broadcast over UPD the device name to see if another
//                        device has already that location name defined.
//                        Added dweet.io device name in AP configuration with hyperlink to it.
//                        Added display of RSSI signal level
// 12-10-2017   v0.0.1    Start of project and initial code

//
// List of valid device types
//
// This is to avoid working on a project and re-using a development board that had
// a previous configuration record stored in its EEPROM memory with the same init flag.
// Basically, this is a list of projects I have developped thus far.
//
#define Vienna      0xCB01  // Vienna Superautomatica Espresso Maker controlled by Google Home
#define Motorcycle  0xCB02  // Motorcycle Alarm System with GPS tracking and SMS notifications
#define Thermostat  0xCB03  // Thermostat device controlled by Google Home

//
// Define firmware version and device type
//
#define FIRMWARE    "v0.0.14"
#define DEVICETYPE  Thermostat

//
// Define OLED display variables
//
#if defined(ARDUINO_Heltec_WIFI_LoRa_32) || defined(ARDUINO_Heltec_WIFI_Kit_32)
  #define OLED_Display
  #define OLED_ADDR     0x3C  // Define I2C Oled Address
  #define OLED_Reset    16    // Heltec OLED reset pin
#else
  // Uncomment next three lines if you are using an I2C OLED display.
  // Otherwise you are using a Nokia 5110 display
  #define OLED_Display  
  #define OLED_ADDR     0x3C  // Define I2C Oled Address
  #define OLED_Reset    -1    // Define the reset pin used for OLED module (If there is none, set to -1)
#endif

//
// Needed include files
//
#include <EEPROM.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#include <AdafruitIO_WiFi.h>          // Modified Adafruit ilbrary.  Repository: https://github.com/CGB01/Adadruit_IO
#include "DHT.h"                      // Modified Adafruit library
#include <TimeLib.h>                  // Repository: https://github.com/PaulStoffregen/Time
#include <Timezone.h>                 // Repository: https://github.com/JChristensen/Timezone
#include <Adafruit_GFX.h>
#ifdef OLED_Display
  #if defined(ARDUINO_Heltec_WIFI_LoRa_32) || defined(ARDUINO_Heltec_WIFI_Kit_32)
    #include "Adafruit_SSD1306.h"     // Modified Adafruit library
  #else
    #include "Adafruit_SH1106.h"      // Modified Adafruit library
  #endif
  #include "WiFi_Logo.h"
  #include "Arduino_Logo.h"
    
  // Reverse the definition of what BLACK and WHITE are for the OLED display
  #define BLACK 1
  #define WHITE 0
#else  
  #include "Adafruit_PCD8544.h"       // Modified Adafruit library
#endif

//
// Define PINs used by sketch.  I have mapped the pins so that it uses the same GPIO pins from
// one board to another (as much as possible!).
//
#ifdef ARDUINO_FEATHER_ESP32
// I2C SDA = GPIO 23, SCL = GPIO 22
  #define VBATPIN       A13   // GPIO 35
  #define DHTPIN        A10   // GPIO 27
  #define RelayPIN      A1    // GPIO 25
#ifndef OLED_Display  
  #define BackLight     A0    // GPIO 26
#endif  
  #define DC            A8    // GPIO 15
  #define CS            A7    // GPIO 32
  #define RST           A6    // GPIO 14
#endif  
#ifdef ARDUINO_ESP32_DEV
// I2C SDA = GPIO 11, SCL = GPIO 22
  #define DHTPIN        A17   // GPIO 27
  #define RelayPIN      A18   // GPIO 25
#ifndef OLED_Display  
  #define BackLight     A19   // GPIO 26
#endif  
  #define DC            A13   // GPIO 15
  #define CS            A4    // GPIO 32
  #define RST           A16   // GPIO 14
  #define SPI_CLK       SCK   // GPIO 18
  #define SPI_MOSI      MOSI  // GPIO 23      
#endif  
#ifdef ARDUINO_LOLIN32
// I2C SDA = GPIO 21, SCL = GPIO 22
  #define DHTPIN        A17   // GPIO 27
  #define RelayPIN      A18   // GPIO 25
#ifndef OLED_Display  
  #define BackLight     A19   // GPIO 26
#endif  
  #define DC            A13   // GPIO 15
  #define CS            A4    // GPIO 32
  #define RST           A16   // GPIO 14
  #define SPI_CLK       SCK   // GPIO 18
  #define SPI_MOSI      MOSI  // GPIO 23      
#endif  
#ifdef ARDUINO_Heltec_WIFI_LoRa_32
// I2C SDA = GPIO 4, SCL = GPIO 15 (For OLED display)
  #define DHTPIN        A14   // GPIO 13
  #define RelayPIN      A18   // GPIO 25
#endif
#ifdef ARDUINO_Heltec_WIFI_Kit_32
// I2C SDA = GPIO 4, SCL = GPIO 15 (For OLED display)
  #define DHTPIN        A14   // GPIO 13
  #define RelayPIN      A18   // GPIO 25
#endif  
#ifndef DHTPIN
  #error Unsupported Board Type!
#endif

//
// Define what turns on/off the LED_BUILTIN.
// Some (stupid) boards need a LOW level to turn it on!
//
#ifdef BUILTIN_LED
  #ifdef ARDUINO_LOLIN32
    #define LED_ON    LOW
    #define LED_OFF   HIGH
  #else
    #define LED_ON    HIGH
    #define LED_OFF   LOW
  #endif
#endif

// 
// Define OTA hostname/password and Dweet name
//
#define OTA_PASS      "admin"                   // Define your own password for updates
char HOSTNAME[32]     = "Termo-location-OTA";   // 'location' will change to whatever the device location is
char DWEETNAME[32]    = "Thermo-eFusedMac";     // This will change to Thermo-'FusedMacAddress'

// 
// Define what signal level turns ON and OFF the thermostat relay
//
#define RelayON       HIGH
#define RelayOFF      LOW

//
// Define temperature min/max values allowed
//
#define MinCelcius    5
#define MaxCelcius    30
#define MinFarenheit  41
#define MaxFarenheit  86

//
// Define time constants (values are in millseconds)
//
#define OneSecond     1000            // One second
#define OneMinute     60000           // One minute
#define OneHour       3600000         // One hour
#define OneDay        86400000        // One day
#define DisplayDelay  10000           // Delay before turning off the display backlight (PCD8544 only)
#define SensorDelay   15000           // Delay before reading the temperature sensor
#define DweetDelay    15000           // Delay between post to Dweet.io

//
// Define data structures stored in EEPROM memory
// EEPROM memory is partitioned into 3 segments.  Config, Statistics and Schedule
//

struct CONFIG
{
  unsigned int      Init;             // Set to DEVICETYPE if valid data in structure
  float             MinTemp;          // Trigger temperature
  bool              Celcius;          // TRUE is using celcius, FALSE when using Farenheit
  byte              Contrast;         // Display constrast level for Nokia 5110 display
  byte              DeviceLocation;   // Device location
  char              CommonName[32];   // Google Home location name (instead of "Bedroom 1" I could speak "Mary's room")
  float             kWh_Cost;         // Electricity cost per kWh in cents
  int               Watts;            // Heater wattage
  unsigned long     HeatingTime;      // Time the heater has been on in seconds
  char              WiFi_SSID[32];    // Wifi SSID name
  char              WiFi_PASS[64];    // Wifi password
  char              Master[17];       // Master device MAC address (not used if using io.adafruit.com)
  char              IO_USER[32];      // Adafruit IO User Name
  char              IO_KEY[64];       // Adafruit IO Key
  char              LastUpdated[32];  // Date/Time of last Google Home message received
  byte              SoftReboot;       // Soft reboot count
  byte              RollOver;         // Number of times the millis() counter has rolled back to zero
  unsigned int      CRC;              // CRC of all bytes up to this field.  Must always be the last on the Config record
};

//
// Define structure for statistics
//
struct STAT_ENTRY
{
  float             Temperature;      // Average temperature for period
  float             Humidity;         // Average humidity for period
  unsigned long     HeatingTime;      // Number of seconds the heater was on for period
  int               Samples;          // Number of samples take for period (set to zero if avg is calculated)
};

struct STATS
{
  STAT_ENTRY        Hourly[24];       // Last 24 hour statistics
  STAT_ENTRY        Daily[45];        // Last 45 days statistics
  STAT_ENTRY        Monthly[2][12];   // Current and previous year's monthly statistics
};

//
// Define heating schedule structure
//
struct SCHEDULE
{
  int               SetHour[4];       // Hour at which to change temperature
  int               SetMinute[4];     // Minutes at which to change temperature
  float             SetTemp[4];       // Temperature set
  bool              Valid[4];         // Time is valid for cycle
};

struct WEEK_SCHEDULE
{
  bool              Enabled;          // Is the schedule enabled?
  SCHEDULE          Day[7];           // Schedule for each day of the week
};

//
// Define EEPROM memory offsets for EEPROM.put function
//
#define CONFIG_OFFSET     0
#define STATS_OFFSET      sizeof(CONFIG)
#define SCHEDULE_OFFSET   sizeof(CONFIG) + sizeof(STATS)

//
// Define possible device locations
//
// The length of the location name is being limited by the display size.
// If you have an LCD display that has more than 14 characters per line,
// then change the location names.  This will help you when speaking to 
// Google Home to identify the locations...
//
char Location[][15] = {
  "Kitchen",
  "Dining Room",
  "Living Room",
  "Family Room",
  "Master Bedroom",
  "Bedroom",
  "Bedroom 1",
  "Bedroom 2",
  "Bedroom 3",
  "Bathroom",
  "Bathroom 1",
  "Bathroom 2",
  "Bathroom 3",
  "Office",
// Locations below 'Office' are excluded from the keyword "Home" location  
  "Basement",
  "Cantina",
  "Wine Cellar",
  "Garage",
  "Outdoor Shed"
};

//
// Define text formating options for display
//
#define CENTER  1
#define LEFT    2
#define RIGHT   3

//
// Define chart types available
//
enum CHARTS
{
  HOURLY_TEMP = 0,          // Hourly temperature/humidity
  HOURLY_COST,              // Hourly heating cost
  HOURLY_KWH,               // Hourly kWh consumption
  DAILY_TEMP,               // Daily temperature/humidity
  DAILY_COST,               // Daily heating cost 
  DAILY_KWH,                // Daily kWh consumption 
  MONTHLY_TEMP,             // Monthly temperature/humidity 
  MONTHLY_COST,             // Monthly heating cost
  MONTHLY_KWH,              // Monthly kWh consumption
  COMPARE_TEMP,             // Monthly temperature with last year's comparison bar chart
  COMPARE_COST,             // Monthly temperature with last year's comparison bar chart
  COMPARE_KWH,              // Monthly temperature with last year's comparison bar chart
  CHART_NONE                // No chart
};

//
// Create display object
// NOTE:  The software SPI does *NOT* work with the ESP32 module and Adafruit_PCD8544 library.
//        Only use hardware...
//
#ifdef OLED_Display
  #if defined(ARDUINO_Heltec_WIFI_LoRa_32) || defined(ARDUINO_Heltec_WIFI_Kit_32)
    Adafruit_SSD1306  display(OLED_Reset);
  #else
    Adafruit_SH1106   display(OLED_Reset);
  #endif
#else
  Adafruit_PCD8544    display = Adafruit_PCD8544(DC, CS, RST);   // Using hardware SPI
#endif

//
// Create temperature sensor, HTTPClient and webserver objects
//
DHT               dht(DHTPIN, DHT11);
WiFiServer        WebServer(80);
HTTPClient        http;

//
// Eastern Time Zone rule (this will need to be changed for other time zones and daylight savings rules)
//
TimeChangeRule myEDT = {"EDT", Second, Sun, Mar, 2, -240};  // Eastern Daylight Time = UTC - 4 hours
TimeChangeRule myEST = {"EST", First, Sun, Nov, 2, -300};   // Eastern Standard Time = UTC - 5 hours
Timezone myTZ(myEDT, myEST);

//
// Set up the Adafruit IO objects
//
AdafruitIO_WiFi io("", "", "", "");                             // Modified Adafruit library
AdafruitIO_Feed *GoogleHome = io.feed("MASTER", false);         // Modified Adafruit library
AdafruitIO_Feed *IO_Schedule = io.feed("Schedule", false);      // Modified Adafruit library
AdafruitIO_Feed *IO_Location = io.feed("", false);              // Modified Adafruit library

//
// Define global variables
//
bool            WiFiConnected, LostWiFi, HeatOn, SensorError, DweetFailed, OTA_Update;
float           temperature, humidity, RequestedTemp, Last_T, Last_H;
char            googleText[20];
int             MaxLocations, ExcludeLocation, LastHour, LastDay, LastMonth, MDNS_Services;
unsigned long   CurrentTime, DisplayTime, TempReading, HeatOnTime, HourlyHeatOnTime, DweetTime, DataSave;
CONFIG          Config;
STATS           HeatingStats;
WEEK_SCHEDULE   Schedule;

//
// Setup function, runs before the main loop
//
void setup()
{
  char  Time[32];
  int   chk, Cntr;
  bool  Done, InitFlag;
  
  // Start by saving MaxLocations and DWEET name (used during first time configuration)
  MaxLocations = sizeof(Location) / 15;
  for(chk=0; chk<MaxLocations; chk++) { if(strcmp(Location[chk], "Basement") == 0) break; }
  ExcludeLocation = chk;
  sprintf(DWEETNAME, "Thermo-%s", FusedMAC());

  // Set pin modes and turn on the LCD backlight
  pinMode(RelayPIN, OUTPUT);
  digitalWrite(RelayPIN, RelayOFF);

#ifdef BackLight
  pinMode(BackLight, OUTPUT);
  digitalWrite(BackLight, HIGH);
#endif

#ifdef BUILTIN_LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LED_ON);
#endif  

  // Display board name and firmware version
  Serial.begin(115200);
  while(!Serial);
  delay(1000);

  // Ok, let's start...
  for(Cntr = 0; Cntr < 50; Cntr++) Serial.print("-");
  Serial.println("\nGoogle Home / Alexa Enabled WiFi Thermostat");
  Serial.printf("Running on %s / Firmware %s\n", ARDUINO_VARIANT, FIRMWARE);

  // Initialize EEPROM memory
  if(!EEPROM.begin(sizeof(Config)+sizeof(HeatingStats)+sizeof(Schedule)))
  {
    // Unable to initialize EEPROM memory.  Hang here.
    Serial.println("Failed to initialize EEPROM memory...");
    while(1);
  }
 
  // Get EEPROM data
  EEPROM.get(CONFIG_OFFSET, Config);
  EEPROM.get(STATS_OFFSET, HeatingStats);
  EEPROM.get(SCHEDULE_OFFSET, Schedule);
  InitFlag = (Config.Init == DEVICETYPE && Config.CRC == Calc_CRC());
  
  // Initialize display object
#ifdef OLED_Display
  #if defined(ARDUINO_Heltec_WIFI_LoRa_32) || defined(ARDUINO_Heltec_WIFI_Kit_32)
    display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  #else
    display.begin(SH1106_EXTERNALVCC, OLED_ADDR);
  #endif
#else
  // Comment next line if you want full speed on PCD8544 LCD display.
  // I've found that on some ESP32 boards running at full speed can be a problem.
  // Your options are SPI_CLOCK_DIV4 or SPI_CLOCK_DIV8 or SPI_CLOCK_DIV16
  // Lower clock divider gives faster updates on the LCD
  #define PCD8544_SPI_CLOCK_DIV SPI_CLOCK_DIV16
  display.begin();
  display.setContrast(InitFlag ? Config.Contrast : 60);
#endif
  ArduinoLogo(3000);

  // Read sensor until valid data is returned
  temperature = NAN;
  dht.begin();
  while(isnan(temperature))
  {
    Serial.print("Reading temperature sensor.");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextColor(WHITE, BLACK);
    display.println(FormatText("Sensor Init", CENTER));
    display.setTextColor(BLACK, WHITE);
    display.println();
    display.println(FormatText("Temperature", CENTER));
    display.println(FormatText("sensor read", CENTER));
    display.println();
    display.display();
    Cntr = 0;
    do
    {
      Serial.print(".");
      display.print(".");
      display.display();
      if(InitFlag)
        temperature = dht.readTemperature(!Config.Celcius);
      else
        temperature = dht.readTemperature();
      delay(1000);
#ifdef OLED_Display
    } while(++Cntr < 21 && isnan(temperature));
#else      
    } while(++Cntr < 14 && isnan(temperature));
#endif    
    if(!isnan(temperature))
    {
      Last_T = temperature;
      Last_H = humidity = dht.readHumidity();
      Done = true;
      Serial.print("  OK!\n\nTemperature   = "); 
      Serial.print(temperature, 1);
      if(InitFlag)
        Serial.printf(" %c\n", Config.Celcius ? 'C' : 'F');
      else
        Serial.println("C");
      Serial.print("Humidity      = "); Serial.print(humidity, 0); Serial.println("%");
    }
    else
    {
      // Failed to read temperature sensor.
      Serial.println("  Failed!\nCheck wiring...  Program halting.");
      display.clearDisplay();
      display.setCursor(0, 0);
      display.setTextColor(WHITE, BLACK);
      display.println(FormatText("Sensor Init", CENTER));
      display.setTextColor(BLACK, WHITE);
      display.println();
      display.println(FormatText("FAILED!", CENTER));
      display.println();
      display.println(FormatText("Check wiring...", CENTER));
      display.println();
      display.display();
      while(1)
      {
#ifdef BUILTIN_LED
        digitalWrite(LED_BUILTIN, LED_ON); delay(250);
        digitalWrite(LED_BUILTIN, LED_OFF); delay(250);   
#endif
      }
    }
  }
  
  // If Init flag is not DEVICETYPE or CRC values doesn't match, then run first time init process
  if(Config.Init != DEVICETYPE || Config.SoftReboot >= 3 || Config.CRC != Calc_CRC()) AP_Config();    

  // Display heater size, kWh used, running cost
  CurrentTime = 0;
  Cntr = (Config.HeatingTime / 86400UL) * 24;
  Serial.printf("Heating Time  = %02u:%02u:%02u\n", numberOfHours(Config.HeatingTime) + Cntr, numberOfMinutes(Config.HeatingTime), numberOfSeconds(Config.HeatingTime));
  Serial.printf("Heater Size   = %d watts\n", Config.Watts);
  Serial.print("Consumption   = "); Serial.print(Calc_kWh(Config.HeatingTime), 2); Serial.print(" kWh\n");
  Serial.print("kWh cost      = "); Serial.print(Config.kWh_Cost, 1); Serial.println(" cents");
  Serial.print("Running cost  = $ "); Serial.print(Calc_HeatingCost(Config.HeatingTime), 2); Serial.print("\n");
  if(strlen(Config.IO_USER) == 0) 
  { 
    Serial.print("Master Device = "); 
    Serial.printf("%s\n", (strcmp(Config.Master, FusedMAC()) == 0 ? "YES" : Config.Master)); 
  }
  Serial.println();
  
  // Set HOSTNAME
  sprintf(HOSTNAME, "Thermo-%s-OTA", Location[Config.DeviceLocation]);

  // Connect to WiFi
  WiFiConnected = false;
  WiFiConnect();

  // The only way I get here is if I'm connected, clear the SoftReboot counter
  Config.SoftReboot = 0;
  Config.CRC = Calc_CRC();                        
  EEPROM.put(CONFIG_OFFSET, Config);
  EEPROM.commit();

  // Display connection info
  Serial.print("MAC Address = ");
  Serial.println(WiFi.macAddress());
  Serial.print("IP Address  = "); 
  Serial.println(WiFi.localIP());
  Serial.print("RSSI Signal = ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
    
  // Get time from NTP server
  Serial.print("Getting time from NTP server... ");
  setSyncProvider(getNTPTime);
  setSyncInterval(OneHour);
  Serial.println((timeStatus() == timeSet) ? "OK!" : "Failed!");

  // Now setup OTA information
  Serial.printf("\nHostname name is %s\n", HOSTNAME);
  Serial.printf("dweet.io name is %s\n", DWEETNAME);
  InitOTA();

  // Print my Common Name if I have one defined 
  if(strlen(Config.CommonName) != 0) Serial.printf("Location '%s' has a common name of \"%s\"\n", Location[Config.DeviceLocation], Config.CommonName);
  
  // Default global variables and display initial temperature reading
  OTA_Update = HeatOn = SensorError = DweetFailed = LostWiFi = false;
  strcpy(googleText, "");
  UpdateTemperature();
  DisplayTime = TempReading = DataSave = millis();

  // Set RequestedTemp to either Config.MinTemp or if there's a schedule (and it's enabled), to the scheduled temperature
  Serial.printf("Scheduling is %sabled.\n", (Schedule.Enabled ? "en" : "dis"));
  RequestedTemp = ScheduledTemp(Config.MinTemp);
  strcpy(googleText, "");
  Serial.printf("Temperature will be set to %s %c\n", String(RequestedTemp, 1).c_str(), (Config.Celcius ? 'C' : 'F')); 
      
  // Start WebServer
  Serial.println("Starting web service.");
  WebServer.begin();

  // Initialize variables for statistics
  HourlyHeatOnTime = 0;
  LastHour = hour();
  LastDay = day();
  LastMonth = month();

  // Init done, run program
#ifdef BUILTIN_LED
  digitalWrite(LED_BUILTIN, LED_OFF);
#endif  
  Serial.printf("\n%s %s\nRunning program...\n", getDate(false), getTime(false));
  for(Cntr = 0; Cntr < 50; Cntr++) Serial.print("-");
  Serial.println();
}

//
// Main loop.  Runs forever.  Well, almost...
//
void loop()
{
  int     chk, Cntr;
  float   temp;
  char    buf[32];
  String  MasterValue;
  
  // Save current time
  CurrentTime = millis();

  // If CurrentTime is about to roll over (go back to zero) wait for it to occur.
  // This occurs every 49.71 days.  It's to prevent the machine from initiating
  // an action, saving the time and then waiting for specific number of seconds
  // before ending the action.  i.e. if I start an action one second before
  // the roll over value of 4,294,967,295 with a 3 second delay, I'd be waiting
  // for the clock to reach 4,294,969,295 which will never occur.
  if(CurrentTime >= 4294967295UL - OneMinute) 
  { 
    // Wait for the roll over
    Serial.printf("[%s] Waiting for clock roll over.\n", getTime(false));
    while(millis() > 100);
    ++Config.RollOver;
    Config.CRC = Calc_CRC();
    EEPROM.put(CONFIG_OFFSET, Config);
    EEPROM.commit();
    Serial.printf("[%s] Rebooting from clock roll over.\n\n", getTime(false));
    delay(1000);
    ESP.restart();
    while(1);
  }  

  // Check if still connected to the WiFi Access Point
  WiFiConnected = (WiFi.status() == WL_CONNECTED);
  if(!WiFiConnected && !LostWiFi) { Serial.printf("[%s] Lost WiFi connection.  Attempting to reconnect...\n", getTime(false)); LostWiFi = true; }
  if(WiFiConnected && LostWiFi) { Serial.printf("[%s] WiFi reconnected!\n", getTime(false)); LostWiFi = false; }
  
  // Handle OTA process and do nothing else if I'm in an OTA Update
  ArduinoOTA.handle();
  if(OTA_Update) return;
  if(strlen(Config.IO_USER) != 0 && strlen(Config.IO_KEY) != 0) io.run();
  
  // Check if I have a client connecting to web server
  ProcessClient();
  
  // Time to turn off the backlight?
#ifdef BackLight
  if(CurrentTime - DisplayTime >= DisplayDelay) digitalWrite(BackLight, LOW);
#endif

  // If scheduling is enabled, check if it's time to change temperature
  if(Schedule.Enabled)
  {
    temp = ScheduledTemp(Config.MinTemp);
    if(temp != RequestedTemp)
    {
      Serial.printf("[%s] Scheduled temperature now set to %s %c\n", getTime(false), String(temp, 1).c_str(), (Config.Celcius ? 'C' : 'F'));
      sprintf(googleText, "Time = %s", String(temp, 1).c_str());
      RequestedTemp = temp;
    }
  }
  
  // Is it time to read the temperature sensor?
  if(CurrentTime - TempReading >= SensorDelay)
  {
    // If not connected to WiFi, try to reconnect
    if(!WiFiConnected) WiFi.reconnect();

    // Read temperature
    TempReading = CurrentTime;
    temperature = dht.readTemperature(!Config.Celcius);
    if(!isnan(temperature))
    {
      // Reading was ok
      humidity = dht.readHumidity();
      SensorError = false;

      // Add temperature/humidity reading to hourly statistics
      if(String(HeatingStats.Hourly[LastHour].Temperature, 1) == "nan" || 
        String(HeatingStats.Hourly[LastHour].Temperature, 1) == "inf" ||
        String(HeatingStats.Hourly[LastHour].Humidity, 0) == "nan" ||
        String(HeatingStats.Hourly[LastHour].Humidity, 0) == "inf")
      {
        HeatingStats.Hourly[LastHour].Temperature = 0.00;
        HeatingStats.Hourly[LastHour].Humidity = 0.00;
        HeatingStats.Hourly[LastHour].Samples = 0;
      }
      HeatingStats.Hourly[LastHour].Temperature += temperature;
      HeatingStats.Hourly[LastHour].Humidity += humidity;
      ++HeatingStats.Hourly[LastHour].Samples;

      // Add to daily statistics
      if(String(HeatingStats.Daily[0].Temperature, 1) == "nan" || 
        String(HeatingStats.Daily[0].Temperature, 1) == "inf" ||
        String(HeatingStats.Daily[0].Humidity, 0) == "nan" ||
        String(HeatingStats.Daily[0].Humidity, 0) == "inf")
      {
        HeatingStats.Daily[0].Temperature = 0.00;
        HeatingStats.Daily[0].Humidity = 0.00;
        HeatingStats.Daily[0].Samples = 0;
      }
      HeatingStats.Daily[0].Temperature += temperature;
      HeatingStats.Daily[0].Humidity += humidity;
      ++HeatingStats.Daily[0].Samples;

      // Add to monthly statistics
      if(String(HeatingStats.Monthly[0][LastMonth-1].Temperature, 1) == "nan" || 
        String(HeatingStats.Monthly[0][LastMonth-1].Temperature, 1) == "inf" ||
        String(HeatingStats.Monthly[0][LastMonth-1].Humidity, 0) == "nan" ||
        String(HeatingStats.Monthly[0][LastMonth-1].Humidity, 0) == "inf")
      {
        HeatingStats.Monthly[0][LastMonth-1].Temperature = 0.00;
        HeatingStats.Monthly[0][LastMonth-1].Humidity = 0.00;
        HeatingStats.Monthly[0][LastMonth-1].Samples = 0;
      }
      HeatingStats.Monthly[0][LastMonth-1].Temperature += temperature;
      HeatingStats.Monthly[0][LastMonth-1].Humidity += humidity;
      ++HeatingStats.Monthly[0][LastMonth-1].Samples;
      
      // Do I need to turn on the heat?
      if(!HeatOn && fabs(temperature - RequestedTemp) >= 0.5F && temperature < RequestedTemp)
      {
        HeatOn = true;
        HeatOnTime = HourlyHeatOnTime = CurrentTime;
        digitalWrite(RelayPIN, RelayON);
      }

      // Wait a bit before turning off the heat
      // This is so that the relay doesn't go on and off with small temperature differences
      if(HeatOn && CurrentTime - HeatOnTime >= OneMinute && temperature >= RequestedTemp)
      {
        // Turn off the heat
        HeatOn = false;
        digitalWrite(RelayPIN, RelayOFF);

        // Now add to the total heater on time in the config record
        Config.HeatingTime += (CurrentTime - HeatOnTime) / OneSecond;
        Config.CRC = Calc_CRC();
        EEPROM.put(CONFIG_OFFSET, Config);
        EEPROM.commit();

        // Save heating time to statistics
        HeatingStats.Hourly[LastHour].HeatingTime += (CurrentTime - HourlyHeatOnTime) / OneSecond;
        HeatingStats.Daily[0].HeatingTime += (CurrentTime - HourlyHeatOnTime) / OneSecond;
        HeatingStats.Monthly[0][LastMonth-1].HeatingTime += (CurrentTime - HourlyHeatOnTime) / OneSecond;
      }

      // Save the new temperature reading
      Last_T = temperature;
      Last_H = humidity;
    } else SensorError = true;
      
    // Update display with new values (or sensor error message)
    UpdateTemperature();   

    // Post to Adafruit if needed
    if(strlen(Config.IO_USER) != 0 && strlen(Config.IO_KEY) != 0) IO_Location->save(Last_T);
  }

  // Time to dweet new values?
  if(CurrentTime - DweetTime > DweetDelay) DweetPost();  

  // Update stats if it's time...
  UpdateStats();
}

//
// Connect to WiFi Access point or to Adafruit IO
//
void  WiFiConnect(void)
{
  int   Cntr = 0;

  // Start by disconnecting from WiFi.  This is to re-initialize it
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  delay(500);

  // Connect to AP or to Adadruit
  if(strlen(Config.IO_USER) == 0 || strlen(Config.IO_KEY) == 0)
    WiFi.begin(Config.WiFi_SSID, Config.WiFi_PASS);
  else
  {
    // Initial Adafruit handlers and set up the message callback for feed
    io.init(Config.IO_USER, Config.IO_KEY);             // Modified Adafruit library
    GoogleHome->init();                                 // Modified Adafruit library
    GoogleHome->onMessage(GoogleCommand);    
    IO_Location->init(Location[Config.DeviceLocation]); // Modified Adafruit library
    IO_Location->onMessage(IOCommand);    
    IO_Schedule->init();                                // Modified Adafruit library
    IO_Schedule->onMessage(IOSchedule);
    io.connect(Config.WiFi_SSID, Config.WiFi_PASS);     // Modified Adafruit library
  }
  while(!WiFiConnected)
  {
    if(Cntr == 0)
    {
      Serial.printf("Attemp %d connecting to WiFi.", ++Config.SoftReboot);
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
#ifdef OLED_Display       
      display.println(FormatText("Connecting to", CENTER));
      if(strlen(Config.IO_USER) == 0 || strlen(Config.IO_KEY) == 0)
        display.println(FormatText(Config.WiFi_SSID, CENTER));
      else
        display.println(FormatText("ADAFRUIT", CENTER));
      display.println("\n\n\n");
      display.drawXBitmap((display.width() - WiFi_Logo_width)/2, ((display.height() - WiFi_Logo_height)/2)+5, WiFi_Logo_bits, WiFi_Logo_width, WiFi_Logo_height, BLACK);  
#else
      display.setTextColor(WHITE, BLACK);
      display.println(FormatText("Startup", CENTER));
      display.setTextColor(BLACK, WHITE);
      display.println();
      display.println("Connecting to ");
      if(strlen(Config.IO_USER) == 0 || strlen(Config.IO_KEY) == 0)
      {
        if(strlen(Config.WiFi_SSID) < 15)
          display.println(FormatText(Config.WiFi_SSID, CENTER));
        else
          display.println(" WiFi Network ");
      } else display.println(" Adafruit IO  ");
#endif      
      display.println();
      display.display();
    }
    if(strlen(Config.IO_USER) == 0 || strlen(Config.IO_KEY) == 0)
      { if(WiFi.status() == WL_CONNECTED) WiFiConnected = true; }
    else
      { if(io.status() == AIO_CONNECTED) WiFiConnected = true; }
    if(!WiFiConnected) 
    { 
      Serial.print("."); 
      display.print(".");
      display.display();
      delay(1000);
#ifdef OLED_Display
      if(++Cntr >= 21)
#else
      if(++Cntr >= 14) 
#endif      
      {
        // WiFi connection failed, save config and reboot
        WiFi.disconnect();
        Serial.println("  Failed!");
        Config.CRC = Calc_CRC();
        EEPROM.put(CONFIG_OFFSET, Config);
        EEPROM.commit();
        display.setCursor(0, 0);
        display.println("\n\n\n\n");
#ifdef OLED_Display
        display.println("\n");
#endif  
        display.println(FormatText("Failed!", CENTER));
        display.display();
        delay(5000);
        ESP.restart();
        while(1);
      }
    }
    else
    {
      // I'm connected
      Serial.println("  OK!");
      display.setCursor(0, 0);
      display.println("\n\n\n\n");
#ifdef OLED_Display
      display.println("\n");
#endif  
      display.println(FormatText("Connected!", CENTER));
      display.display();
      if(strlen(Config.IO_USER) != 0 && strlen(Config.IO_KEY) != 0)
      {
        // Adafruit objects for dashboard creation
        AdafruitIO_Dashboard *dashboard = io.dashboard("Home Heating");
        SliderBlock *slider = dashboard->addSliderBlock(GoogleHome);
        GaugeBlock *gauge = dashboard->addGaugeBlock(IO_Location);
        ToggleBlock *toggle = dashboard->addToggleBlock(IO_Schedule);

        // Check if the Adafruit MASTER feed already exits
        Serial.println(io.userAgent());
        if(!GoogleHome->exists()) 
        {
          Serial.print("Creating Adafruit MASTER feed...  ");        
          if(!GoogleHome->create()) Serial.println("Failed!"); else Serial.println("OK!");
        }      

        // Check if the Schedule feed exists
        if(!IO_Schedule->exists()) 
        {
          Serial.print("Creating Adafruit Schedule feed...  ");        
          if(!IO_Schedule->create()) Serial.println("Failed!"); else Serial.println("OK!");
        }      
        
        // Check if the Adafruit location feed already exits
        if(!IO_Location->exists()) 
        {
          Serial.print("Creating Adafruit location feed...  ");        
          if(!IO_Location->create()) Serial.println("Failed!"); else Serial.println("OK!");
        }  

        // Now check if the dashboard exists
        if(!dashboard->exists()) 
        {
          Serial.print("Creating Thermostat dashboard... ");
          if(dashboard->create())
          {
            Serial.print("OK!\nCreating MASTER slider... ");
            slider->min = (Config.Celcius ? MinCelcius : MinFarenheit);
            slider->max = (Config.Celcius ? MaxCelcius : MaxFarenheit);
            slider->step = 0.50F;
            slider->label = (Config.Celcius ? "Celcius" : "Farenheit");
            bool added = slider->save("MASTER");
            Serial.println(added ? "OK!" : "Failed!");

            Serial.print("Creating Schedule switch...");
            toggle->onText = "ON";
            toggle->offText = "OFF";
            added = toggle->save("Schedule");
            Serial.println(added ? "OK!" : "Failed!");

            Serial.printf("Creating \"%s\" gauge... ", Location[Config.DeviceLocation]);
            gauge->min = (Config.Celcius ? MinCelcius : MinFarenheit);
            gauge->max = (Config.Celcius ? MaxCelcius : MaxFarenheit);
            gauge->ringWidth = "thin";
            gauge->label = (Config.Celcius ? "Celcius" : "Farenheit");
            added = gauge->save(Location[Config.DeviceLocation]);
            Serial.println(added ? "OK!" : "Failed!");
          }
          else
            Serial.println("Failed!");
        }
        else
        {
          // The dashboard exists, check if the location guage exists
          if(!gauge->exists(Location[Config.DeviceLocation]))
          {
            Serial.printf("Creating \"%s\" gauge... ", Location[Config.DeviceLocation]);
            gauge->min = (Config.Celcius ? MinCelcius : MinFarenheit);
            gauge->max = (Config.Celcius ? MaxCelcius : MaxFarenheit);
            gauge->ringWidth = "thin";
            gauge->label = (Config.Celcius ? "Celcius" : "Farenheit");
            bool added = gauge->save(Location[Config.DeviceLocation]);
            Serial.println(added ? "OK!" : "Failed!");
          }          
        }
      }
    }
  }    
}

//
// Initialize OTA functions
//
void  InitOTA(void)
{
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.onStart([]() 
  { 
    OTA_Update = true;
    WebServer.stop();
#ifdef BackLight
    digitalWrite(BackLight, HIGH);      // Turn backlight on
#endif
    digitalWrite(RelayPIN, RelayOFF);   // Turn heater off during updates
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.setTextColor(WHITE, BLACK);
    display.println(FormatText("OTA Update", CENTER));
    display.setTextColor(BLACK, WHITE);
    display.println();
#ifdef OLED_Display
    display.println();
    display.setTextSize(3);
#else    
    display.setTextSize(2);
    display.println("   0%");
#endif    
    display.display();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) 
  { 
    int   p = (progress / (total / 100));
    char  buf[25];
    // Comment next line if you want to see increaments of 1% (slower OTA updates)
    // I recommend using the 5% updates when using an I2C OLED display
    if(p % 5 == 0)
    {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.setTextColor(WHITE, BLACK);
      display.println(FormatText("OTA Update", CENTER));
      display.setTextColor(BLACK, WHITE);
      display.println();
#ifdef OLED_Display
      display.println();
      display.setTextSize(3);
#else    
      display.setTextSize(2);
#endif      
      display.print(" ");
      if(p <= 100) display.print(" ");
      if(p < 10) display.print(" ");
      display.print(p);
      display.println("%");
      display.setTextSize(1);
      display.println();
      sprintf(buf, "Size = %u", total);
      display.printf(FormatText(buf, CENTER));
      display.display();
    }
  });
  ArduinoOTA.onEnd([]() 
  { 
    WiFi.disconnect();
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.setTextColor(WHITE, BLACK);
    display.println(FormatText("OTA Update", CENTER));
    display.setTextColor(BLACK, WHITE);
    display.println();
#ifdef OLED_Display
    display.println();
    display.print(" ");
    display.setTextSize(3);
#else    
    display.print(" ");
    display.setTextSize(2);
#endif      
    display.print(" DONE");
    display.setTextSize(1);
    display.println();
    display.println();
    display.println();
#ifdef OLED_Display
    display.println();
#endif        
    display.print(FormatText("Rebooting!", CENTER));
    display.display();
    delay(2000);
  });
  ArduinoOTA.onError([](ota_error_t error) 
  {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.setTextColor(WHITE, BLACK);
    display.println(FormatText("OTA Error!", CENTER));
    display.setTextColor(BLACK, WHITE);
    display.println();
    display.println();
    if(error == OTA_AUTH_ERROR) display.println(FormatText("Auth Failed", CENTER));
    else if (error == OTA_BEGIN_ERROR) display.println(FormatText("Begin Failed", CENTER));
    else if (error == OTA_CONNECT_ERROR) display.println(FormatText("Connect Failed", CENTER));
    else if (error == OTA_RECEIVE_ERROR) display.println(FormatText("Receive Failed", CENTER));
    else if (error == OTA_END_ERROR) display.println(FormatText("End Failed", CENTER));
    else display.println(FormatText("Unknown Error", CENTER));
    display.println();
    display.print(FormatText("Rebooting!", CENTER));
    display.display();
    WiFi.disconnect();
    delay(5000);
    ESP.restart();
  });
  ArduinoOTA.begin();  
}

//
// Update statistics 
//
void  UpdateStats(void)
{
  int   Cntr;
  
  if(LastHour != hour())
  {
    // Average out the temperature and humidity reading
    HeatingStats.Hourly[LastHour].Temperature /= HeatingStats.Hourly[LastHour].Samples;
    HeatingStats.Hourly[LastHour].Humidity /= HeatingStats.Hourly[LastHour].Samples;

    // If heat is on, reset HourlyHeatOnTime to CurrentTime
    if(HeatOn)
    {
      HeatingStats.Hourly[LastHour].HeatingTime += (CurrentTime - HourlyHeatOnTime) / OneSecond;
      HeatingStats.Daily[0].HeatingTime += (CurrentTime - HourlyHeatOnTime) / OneSecond;
      HeatingStats.Monthly[0][LastMonth-1].HeatingTime += (CurrentTime - HourlyHeatOnTime) / OneSecond;
      HourlyHeatOnTime = CurrentTime;
    }
    HeatingStats.Hourly[LastHour].Samples = 0;    

    // Save new LastHour value and write back to EEPROM
    LastHour = hour();
    memset(&HeatingStats.Hourly[LastHour], NULL, sizeof(STAT_ENTRY));
    HeatingStats.Hourly[LastHour].Temperature = Last_T;
    HeatingStats.Hourly[LastHour].Humidity = Last_H;
    HeatingStats.Hourly[LastHour].Samples = 1;
    EEPROM.put(STATS_OFFSET, HeatingStats);
    EEPROM.commit();
  }

  // Time to update daily stats?
  if(LastDay != day())
  {
    // Average out the temperature and humidity reading
    HeatingStats.Daily[0].Temperature /= HeatingStats.Daily[0].Samples;
    HeatingStats.Daily[0].Humidity /= HeatingStats.Daily[0].Samples;
    HeatingStats.Daily[0].Samples = 0;

    // Shift days in structure
    for(int i=43; i>=0; i--) memcpy(&HeatingStats.Daily[i+1], &HeatingStats.Daily[i], sizeof(STAT_ENTRY));

    // Now clear current daily stats and write back to EEPROM
    LastDay = day();
    memset(&HeatingStats.Daily[0], NULL, sizeof(STAT_ENTRY));
    HeatingStats.Daily[0].Temperature = Last_T;
    HeatingStats.Daily[0].Humidity = Last_H;
    HeatingStats.Daily[0].Samples = 1;
    EEPROM.put(STATS_OFFSET, HeatingStats);
    EEPROM.commit();
  }
  
  // Time to update monthly stats?
  if(LastMonth != month())
  {
    // Average out the temperature and humidity reading
    HeatingStats.Monthly[0][LastMonth-1].Temperature /= HeatingStats.Monthly[0][LastMonth-1].Samples;
    HeatingStats.Monthly[0][LastMonth-1].Humidity /= HeatingStats.Monthly[0][LastMonth-1].Samples;
    HeatingStats.Monthly[0][LastMonth-1].Samples = 0;

    // If this also a new year (LastMonth == 12) then move current year data into last year's fields
    // and clear current year values
    if(LastMonth == 12)
    {
      memcpy(&HeatingStats.Monthly[1], &HeatingStats.Monthly[0], sizeof(HeatingStats.Monthly[0]));
      memset(&HeatingStats.Monthly[0], NULL, sizeof(HeatingStats.Monthly[0]));
    }

    // Write back to EEPROM
    LastMonth = month();
    EEPROM.put(STATS_OFFSET, HeatingStats);
    HeatingStats.Monthly[0][LastMonth-1].Temperature = Last_T;
    HeatingStats.Monthly[0][LastMonth-1].Humidity = Last_H;
    HeatingStats.Monthly[0][LastMonth-1].Samples = 1;
    EEPROM.commit();
  }  

  // Time to do a auto-save on the statistical data?
  if(CurrentTime - DataSave > 15 * OneMinute)
  {
    EEPROM.put(STATS_OFFSET, HeatingStats);
    EEPROM.commit();
    DataSave = CurrentTime;    
  }
}

//
// Update LCD display with new temperatue, humidity, time and RSSI signal level
//
void UpdateTemperature(void)
{
  // Update display
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.setTextColor(BLACK, WHITE);
  display.println(FormatText((strlen(Config.CommonName) == 0 ? Location[Config.DeviceLocation] : Config.CommonName), CENTER));
  display.println();
#ifdef OLED_Display
  display.println();
#endif  
  display.printf(" %c  ", (Schedule.Enabled ? 'S' : ' '));
#ifdef OLED_Display  
  display.setTextSize(3);
#else
  display.setTextSize(2);
#endif  
  if(Last_T < 10.0) display.print(" ");
  display.print(Last_T, 1);
  display.setTextSize(1);
  if(HeatOn) display.setTextColor(WHITE, BLACK);
  display.printf(" %c ", (Config.Celcius ? 'C' : 'F'));
  display.setTextColor(BLACK, WHITE);
  display.println();
  display.println();
#ifdef OLED_Display
  display.println();
#endif  
  if(WiFiConnected)
  {
    renderRSSIIcon();
    if(strlen(googleText) != 0)    
      display.println(FormatText(googleText, CENTER));
    else
      display.println((DweetFailed ? FormatText("dweet failed", CENTER) : (SensorError ? FormatText("sensor", CENTER) : "")));
    strcpy(googleText, "");
  }
  else
    display.println(FormatText("Lost WiFi", CENTER));
#ifdef OLED_Display
  display.printf("%s ", dayShortStr(weekday()));
  display.printf("%s-%02d %s  ", monthShortStr(month()), day(), getTime(true));
#else    
  display.printf("%s %s  ", getTime(true), dayShortStr(weekday()));
#endif  
  display.print(Last_H, 0);
  display.println("%");
  display.display();
}

//
// Get current time
//
char *getTime(bool Short)
{
  static char   Buffer[10];

  // Default buffer
  if(Short) strcpy(Buffer, "00:00"); else strcpy(Buffer, "00:00:00");
  
  // Check if I have the time set
  if(timeStatus() != timeSet) 
  {
    Serial.println("[getTime] TimeStatus() not set!");
    if(!getNTPTime()) return Buffer;    // Unable to get time from NTP server
  }

  // Return short or long time format
  if(Short)
    sprintf(Buffer, "%02d:%02d", hour(), minute());
  else
    sprintf(Buffer, "%02d:%02d:%02d", hour(), minute(), second());
  return Buffer;
}

//
// Get current date
//
char *getDate(bool Short)
{
  static char   Buffer[50];
  String        Date;

  // Default buffer
  strcpy(Buffer, "??? ??? ?? ????");
  
  // Check if I have the time set
  if(timeStatus() != timeSet) 
    if(!getNTPTime()) return Buffer;    // Unable to get time from NTP server

  // I have a valid time...
  if(Short)
    Date = String(dayShortStr(weekday())) + " " + String(monthShortStr(month())) + " " + String(day()) + " " + String(year());
  else
    Date = String(dayStr(weekday())) + ", " + String(monthStr(month())) + " " + String(day()) + ", " + String(year());
  strcpy(Buffer, Date.c_str());
  return Buffer;
}

//
// Return the total uptime
//
void UpTime(char *Buf)
{
  int d, h, m, s;
  unsigned long CT = millis();

  d = CT / OneDay;
  d += (Config.RollOver * 49);
  CT -= (d * OneDay);
  h = CT / OneHour;
  CT -= (h * OneHour);
  m = CT / OneMinute;
  CT -= (m * OneMinute);
  s = CT / OneSecond;
  sprintf(Buf, "%u day%s %02d:%02d:%02d", d, (d == 1 ? "" : "s"), h, m, s);
}

//
// Format text for a 14 character display (PCD8544) or 21 characters SSD1306 or SH1106
//
char *FormatText(char *Text, int Mode)
{
  static char buf[25];
#ifdef OLED_Display  
  strcpy(buf, "                     ");
  int i = (22 - strlen(Text)) / 2;
  if(Mode == CENTER) memcpy(&buf[i], Text, strlen(Text));
  if(Mode == LEFT) sprintf(buf, "%-21s", Text);
  if(Mode == RIGHT) sprintf(buf, "%21s", Text);
#else
  strcpy(buf, "              ");
  int i = (14 - strlen(Text)) / 2;
  if(Mode == CENTER) memcpy(&buf[i], Text, strlen(Text));
  if(Mode == LEFT) sprintf(buf, "%-14s", Text);
  if(Mode == RIGHT) sprintf(buf, "%14s", Text);
#endif  
  return buf;
}

//
// Start an Access Point server and present a web page for configuration
// NOTE:  The default IP address for the server is 192.168.4.1
//
void AP_Config(void)
{
  bool    Done = false;
  bool    Post = false;
  bool    Icon = false;
  bool    Reboot = false;
  bool    ClearStats = true;
  int     Count;
  int     Pos, Cntr, net, i;
  char    buf[32];
  String  Msg, value;
    
  // Create server object on port 80
  WiFiServer server(80);
  WiFiClient client;
  
  // Config access point.
  String AP_SSID = "Thermostat-" + String(FusedMAC());
  Serial.printf("\nConfiguring Access point \"%s\"...  ", AP_SSID.c_str());
  if(!WiFi.softAP(AP_SSID.c_str(), NULL)) 
  {
    Serial.println("Failed!\nUnable to initialize.");
    while(1);
  }

  // Start web server
  Serial.println("OK!\nStarting web server and waiting for a client...\n");
  server.begin();

  // Display first time run info on LCD display
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK, WHITE);
  display.setCursor(0, 0);
  sprintf(buf, " %s \n", FusedMAC());
#ifdef OLED_Display
  display.print(FormatText(buf, CENTER));
  display.println();
#else
  display.print(buf);
#endif  
  display.println();
  display.println(FormatText("Connect to", CENTER));
  display.println(FormatText("Thermostat AP", CENTER));
  display.println(FormatText("then browser", CENTER));
  display.println(FormatText("192.168.4.1", CENTER));
  display.display();
  
  // Loop here until a client has connected and sent a good config record
  while(!Done)
  {
#ifdef BUILTIN_LED
    for(i=0; i<2; i++) 
    {
      // This is just to visualize that I'm waiting for a client to connect
      digitalWrite(LED_BUILTIN, LED_ON); delay(100); 
      digitalWrite(LED_BUILTIN, LED_OFF); delay(100);
    }
#endif    
    delay(1000);
    client = server.available();
    if(client) 
    {
      // We have a new client!
      ClearStats = (Config.Init == DEVICETYPE && Config.CRC == Calc_CRC());
#ifdef BUILTIN_LED
      digitalWrite(LED_BUILTIN, LED_ON);
#endif    
      
      String currentLine = "";
      while(client.connected()) 
      {
        // If data is available, read bytes
        if(client.available()) 
        {
          char c = client.read();
          if(c == '\n') 
          {
            // If the current line is blank, and you get two newline characters in a row,
            // that's the end of the client HTTP request header
            if(currentLine.length() == 0)
            {
              // Is was for the icon, send it and break out
              if(Icon) { SendIcon(client); break; }
              
              // Send HTTP response code
              // and a content-type so the client knows what's coming, then a blank line
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type: text/html");
              client.println("Connection: close");
              client.println();

              // Send HTML page with the configuration info
              client.print("<!DOCTYPE html><html><head><title>Thermostat Setup</title>");
              client.printf("</head><body><center><H1><U>Thermostat Configuration</U></H1><H3><font color=\"#FF0000\">Device = %s<font color=\"#000000\">", FusedMAC());
              client.printf("</H3>%s / Firmware %s", ARDUINO_VARIANT, FIRMWARE);
              client.printf("<br>Temperature = %s C, Humidity = %s%%<br><br>", String(temperature, 1).c_str(), String(humidity, 0).c_str());

              // If this was a POST continue reading to get parameters sent back
              if(Post)
              {
                // Break down POST into components
                memset(&Config, NULL, sizeof(Config));
                Msg = client.readStringUntil('\n');
//Serial.println(Msg);
                Pos = Msg.indexOf("SSID=");
                if(Pos != -1) value = Msg.substring(Pos+5, Msg.indexOf("&PASS"));
                if(value == "") 
                  Post = false;
                else
                  strcpy(Config.WiFi_SSID, value.c_str());
//Serial.println(Config.WiFi_SSID);                  
                Pos = Msg.indexOf("PASS=");
                if(Pos != -1) value = Msg.substring(Pos+5, Msg.indexOf("&LOC"));
                strcpy(Config.WiFi_PASS, value.c_str());
//Serial.println(Config.WiFi_PASS);                  
                Pos = Msg.indexOf("LOC=");
                if(Pos != -1) value = Msg.substring(Pos+4, Msg.indexOf("&COMMON"));
                Config.DeviceLocation = value.toInt();
//Serial.println(Config.DeviceLocation);                  
                Pos = Msg.indexOf("COMMON=");
                if(Pos != -1) value = Msg.substring(Pos+7, Msg.indexOf("&DEGREES"));
                strcpy(Config.CommonName, value.c_str());
//Serial.println(Config.CommonName);                  

                // Make sure common name entered is not defined as a location
                for(Pos=0; Pos<MaxLocations; Pos++) if(strcmp(Location[Pos], Config.CommonName) == 0) Post = false;
                if(!Post) client.print("<center><font color=\"#FF0000\">Common Name can not be a Location Name!<font color=\"#000000\"><br><br>");
                Pos = Msg.indexOf("DEGREES=");
                if(Pos != -1) value = Msg.substring(Pos+8, Msg.indexOf("&TEMP"));
                Config.Celcius = value.toInt();
//Serial.println(Config.Celcius);                  
                Pos = Msg.indexOf("TEMP=");
                if(Pos != -1) value = Msg.substring(Pos+5, Msg.indexOf("&WATTS"));
                Config.MinTemp = value.toFloat();
//Serial.println(Config.MinTemp);                  
                if(Config.MinTemp < (Config.Celcius ? 5 : 41) || Config.MinTemp > (Config.Celcius ? 30 : 86))
                {
                  client.print("<center><font color=\"#FF0000\">Invalid minimum temperature!<font color=\"#000000\"><br>Must be between 5 and 30 for Celcius<br>between 41 and 86 for Farenheit.</center><br><br>");
                  Post = false;
                }
                Pos = Msg.indexOf("WATTS=");
                if(Pos != -1) value = Msg.substring(Pos+6, Msg.indexOf("&COST"));
                Config.Watts = value.toFloat();
//Serial.println(Config.Watts);                  
                Pos = Msg.indexOf("COST=");
                if(Pos != -1) value = Msg.substring(Pos+5, Msg.indexOf("&MASTER"));
                Config.kWh_Cost = value.toFloat();
//Serial.println(Config.kWh_Cost);                  
                Pos = Msg.indexOf("MASTER=");
                if(Pos != -1) value = Msg.substring(Pos+7, Msg.indexOf("&IO_USER"));
                strcpy(Config.Master, value.c_str());
//Serial.println(Config.Master);                  
                Pos = Msg.indexOf("IO_USER=");
                if(Pos != -1) value = Msg.substring(Pos+8, Msg.indexOf("&IO_KEY"));
                strcpy(Config.IO_USER, value.c_str());
//Serial.println(Config.IO_USER);                  
                Pos = Msg.indexOf("IO_KEY");
                if(Pos != -1) value = Msg.substring(Pos+7);
                Pos = value.indexOf("&CLEAR");
                if(Pos != -1) value = value.substring(0, Pos);
                strcpy(Config.IO_KEY, value.c_str());
//Serial.println(Config.IO_KEY);    
                if(ClearStats)
                {              
                  Pos = Msg.indexOf("CLEAR=on");
                  if(Pos != -1) ClearStats = true; else ClearStats = false;
                } else ClearStats = true;
//Serial.println(ClearStats);                  
                
                // If Post is still TRUE, try to connect to WiFi network
                if(Post)
                {
                  client.printf("<hr><center>Connecting to \"%s\"...", Config.WiFi_SSID);
                  Serial.printf("Connecting to \"%s\".", Config.WiFi_SSID);
                  Cntr = 0;
                  Pos = WiFi.begin(Config.WiFi_SSID, Config.WiFi_PASS);
                  while(Pos != WL_CONNECTED)
                  {
                    delay(1500);
                    Pos = WiFi.status();
                    Serial.print(".");
                    if(++Cntr > 10 && Pos != WL_CONNECTED) break;
                  }
                  if(Cntr > 10)
                  {
                    client.println("&nbsp&nbspFailed!</center><br>");
                    Serial.println("Failed!");
                  }
                  else
                  {
                    // Access Point connected.  Check if location is already defined on another device
                    client.printf("&nbsp&nbspOK!<br><br>");
                    client.flush();
                    Serial.println("OK!\nChecking for duplicate device location...");
                    Count = 0;
                    if(findService(Location[Config.DeviceLocation], &Count))
                    {
                      // Location already in use
                      Serial.printf("Location \"%s\" already used!\n", Location[Config.DeviceLocation]);
                      client.printf("Location \"<b>%s</b>\" already in use!</center><br>", Location[Config.DeviceLocation]);
                      ClearStats = (Config.Init == DEVICETYPE && Config.CRC == Calc_CRC());
                    }   
                    else
                    {
                      // Everything was good...
                      Serial.printf("Location \"%s\" not in use.\n", Location[Config.DeviceLocation]);
                      client.printf("Location '<b>%s</b>' added to the network.<br>", Location[Config.DeviceLocation]);
                      if(strlen(Config.CommonName) != 0) client.printf("You can reference it as \"<b>%s</b>\" with Google Home.<br>", Config.CommonName);
                      if(Count == 0 && strlen(Config.IO_USER) == 0 && strlen(Config.IO_KEY) == 0) 
                      {
                        client.print("<br>This is now your \"<font color=\"#FF0000\">MASTER<font color=\"#000000\">\" device for Google Home.<br>");
                        client.print("Please write down the device number above.<br>");
                        client.print("You will need it when adding new devices.<br>");
                        strcpy(Config.Master, FusedMAC());
                      }
                      client.print("<br>Now go and create your <a href=\"https://ifttt.com\" target=\"_blank\">IFTTT</a> applet.<br>");
                      client.printf("<br>Your <b>dweet.io</b> name is <a href=\"https://dweet.io/follow/%s\" target=\"_blank\">%s</a>", DWEETNAME, DWEETNAME);
                      client.print("<br><br>Device IP address is: <a href=\"http://"); 
                      client.print(WiFi.localIP());
                      client.print("\" target=\"_blank\">");
                      client.print(WiFi.localIP());
                      client.printf("</a><br><br>Rebooting %s</center></body></html>", ARDUINO_VARIANT);
                      client.println();
                      client.println();
                      client.flush();
                      
                      // Save config record
                      Config.Init = DEVICETYPE;
                      Config.Contrast = 60;
                      Config.CRC = Calc_CRC();                        
                      EEPROM.put(CONFIG_OFFSET, Config);

                      // Do I need to clear HeatingStats and Schedule?
                      if(ClearStats)
                      {
                        Serial.println("Clearing statistics and schedule.");
                        memset(&HeatingStats, NULL, sizeof(HeatingStats));
                        memset(&Schedule, NULL, sizeof(Schedule));
                        EEPROM.put(STATS_OFFSET, HeatingStats);
                        EEPROM.put(SCHEDULE_OFFSET, Schedule);
                      }
                      EEPROM.commit();
                      Reboot = true;
                      break;
                    }
                  }
                }
              }
            
              // Get list of available WiFi networks
              net = WiFi.scanNetworks();
              client.print("<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/\">");
              client.print("<center><table border=\"1\" width=\"40%\"><tr><td align=\"right\">WiFi Network&nbsp</td><td>\n<select name=\"SSID\">");
              if(net <= 0) 
                client.println("<option value=\"\">No networks found!</option>");
              else 
                for(i=0; i<net; ++i) client.printf("<option value=\"%s\">%s</option>\n", WiFi.SSID(i).c_str(), WiFi.SSID(i).c_str());
              client.print("</select>\n&nbsp&nbsp&nbsp<a href=\"/\" title=\"Refresh WiFi network list\">Refresh</a></td></tr>");
              client.print("<tr><td align=\"right\">Passphrase&nbsp</td><td><input type=\"text\" name=\"PASS\" value=\"\" placeholder=\"Network Password\"></td></tr>");
              client.print("<tr><td align=\"right\">Location&nbsp</td><td>\n<select name=\"LOC\">");
              for(i=0; i<MaxLocations; ++i) client.printf("<option value=\"%d\">%s</option>\n", i, Location[i]);
              client.print("</select>\n</td></tr>");
              client.print("<tr><td align=\"right\">Common Name&nbsp</td><td><input type=\"text\" name=\"COMMON\" value=\"\" size=20 placeholder=\"Google Home Name\"></td></tr>");
              client.print("<tr><td align=\"right\">Degrees&nbsp</td><td><input name=\"DEGREES\" type=\"radio\" checked value=\"1\">Celcius&nbsp<input name=\"DEGREES\" type=\"radio\" value=\"0\">Farenheit</td></tr>");
              client.printf("<tr><td align=\"right\">Min Temp&nbsp</td><td><input type=\"number\" name=\"TEMP\" min=\"%d\" max=\"%d\" step=\"0.5\"></td></tr>", MinCelcius, MaxFarenheit);
              client.print("<tr><td align=\"right\">Heater Watts&nbsp</td><td><select name=\"WATTS\">");
              client.print("<option value=\"500\">500</option>\n");
              client.print("<option value=\"750\">750</option>\n");
              client.print("<option value=\"1000\">1000</option>\n");
              client.print("<option value=\"1250\">1250</option>\n");
              client.print("<option value=\"1500\">1500</option>\n");
              client.print("<option value=\"2000\">2000</option>\n");
              client.print("<option value=\"2500\">2500</option>\n</select></td></tr>");
              client.print("<tr><td align=\"right\">kWh Cost&nbsp</td><td><input type=\"number\" name=\"COST\" min=\"0\" max=\"99.9\" step=\"0.1\" value=\"0\">&nbspcents</td></tr>");
              client.print("<tr><td align=\"right\">Master Device&nbsp</td><td><input type=\"text\" name=\"MASTER\" value=\"\" size=16 placeholder=\"Device ID\"></td></tr>");
              client.print("<tr><td align=\"right\">User Name&nbsp</td><td><input type=\"text\" name=\"IO_USER\" value=\"\" placeholder=\"Adafruit User Name\"></td></tr>");
              client.print("<tr><td align=\"right\">API Key&nbsp</td><td><input type=\"text\" name=\"IO_KEY\" size=32 value=\"\" placeholder=\"Adafruit API Key\"></td></tr></table>");
              if(ClearStats) client.printf("<br><input type=\"checkbox\" name=\"CLEAR\"%s>&nbsp&nbspClear statistics and schedule?<br>", (ClearStats ? "" : " checked"));
              client.print("<br><input type=\"submit\"><br></form>");
              client.printf("<br><hr>Created by <a href=\"mailto:claudegbeaudoin@hotmail.com?Subject=Google Thermostat\">Claude G. Beaudoin</a></center></body></html>");
              client.println();
              client.println();
              client.flush();
              break;
            } else currentLine = "";
          } else if(c != '\r') currentLine += c;

          // Check to see if the client request was "POST" or if the browser wants the favicon
          if(currentLine.endsWith("POST /")) Post = true;
          if(currentLine.endsWith("GET /favicon.ico")) Icon = true;
        }
      }
        
      // Close the connection
      client.flush();
      delay(2000);
      client.stop();
      if(Reboot)
      {
        // Disconnect AP and reboot
        Serial.println("Configuration saved.\nRebooting... ");
        WiFi.softAPdisconnect();
        WiFi.disconnect();
        delay(2000);
        ESP.restart(); 
        while(1);                 
      }
      Icon = Post = false;
    }
#ifdef BUILTIN_LED
    digitalWrite(LED_BUILTIN, LED_OFF);
#endif    
  }
}

//
// Post data to dweet.io
//
void DweetPost(void)
{
  String  postData, response, MyLocation, ForLocation;
  int     httpCode, i;
  float   temp;
  char    buf[64], buf1[32];
  
  // If not connected to WiFi, just exit
  if(!WiFiConnected) { DweetTime = millis(); return; }
  
#ifdef BUILTIN_LED
  digitalWrite(LED_BUILTIN, LED_ON);
#endif  
  DweetFailed = true;
  MyLocation = String(Location[Config.DeviceLocation]);
  MyLocation.toUpperCase();
  
  // First check if Google home has posted an updated temperature configuration if using dweet.io
  if(strlen(Config.IO_USER) == 0 || strlen(Config.IO_KEY) == 0)
  {
    postData = "http://dweet.io/get/latest/dweet/for/Thermo-" + String(Config.Master) + "-Google";
    http.setReuse(true);
    http.setTimeout(10000);
    http.begin(postData);
    httpCode = http.GET();
    if(httpCode == HTTP_CODE_OK) 
    {
      // Get response and parse it to get the MinTemp field
      response = http.getString();
  
      // Get the created data from response
      i = response.indexOf("\"created\"");
      if(i != -1)
      {
        // Save the date/time the message was created
        strcpy(buf, response.substring(i+11, response.indexOf("Z\",\"")).c_str());
  
        // Is the created date different then last one I received?
        if(strcmp(buf, Config.LastUpdated) != 0)
        {
          // Yup, get the new min temp and the destination location
          strcpy(Config.LastUpdated, buf);
          response.toUpperCase();
          i = response.indexOf("LOCATION");
          if(i != -1)
          {
            response = response.substring(i);
            i = response.indexOf("\",");
            if(i != -1) 
            {
              ForLocation = response.substring(11, i);
              // Check if there's an apostrophe "'" in the location and remove spaces around it
              // Google Home will send "John ' s room" instead of "John's room"
              i = ForLocation.indexOf("'");
              if(i != -1) ForLocation = ForLocation.substring(0, i-1) + "'" + ForLocation.substring(i+2);
            }
          }
  
          // Is this request for MyLocation or for all thermostats
          if(MyLocation == ForLocation || (ForLocation == "HOME" && Config.DeviceLocation < ExcludeLocation) || strcmp(Config.CommonName, ForLocation.c_str()) == 0)
          {
            // Yup it's for me, is this a MinTemp request?
            // This is in response to a "Ok Google, set $ temperature to # degrees" command
#ifdef BackLight
            digitalWrite(BackLight, HIGH);
            DisplayTime = millis();
#endif          
            i = response.indexOf("MINTEMP");
            if(i != -1) 
            { 
              // Extract requested temperature
              response = response.substring(i);
              i = response.indexOf("}");
              if(i != -1) response = response.substring(9, i);
              Serial.printf("[%s] Google = MinTemp \"%s\"\n", getTime(false), response.c_str());
              if(response.length() < 5) sprintf(googleText, "-> %s <-", response.c_str());
              float MinTemp = response.toFloat();
    
              // Validate new temperature
              if(MinTemp < (Config.Celcius ? MinCelcius : MinFarenheit) || MinTemp > (Config.Celcius ? MaxCelcius : MaxFarenheit))
                strcpy(googleText, "temp error");        
              else
              {
                // Save new temperature and disable scheduling
                Config.MinTemp = RequestedTemp = MinTemp;
                Schedule.Enabled = false;
                EEPROM.put(SCHEDULE_OFFSET, Schedule);
              }
            }
  
            // Am I being ask to run the programmed schedule?
            // This is in response to a "Ok Google, run $ heating schedule" command
            i = response.indexOf("SCHEDULE");
            if(i != -1)
            {
              // Check if there's a schedule
              temp = RequestedTemp;
              RequestedTemp = ScheduledTemp(0.0F);
              if(RequestedTemp != 0.0F)
              {
                Schedule.Enabled = true;
                EEPROM.put(SCHEDULE_OFFSET, Schedule);
                EEPROM.commit();        
              } else RequestedTemp = temp;
            }
          }
  
          // Save date/time of last message from Google Home
          Config.CRC = Calc_CRC();                        
          EEPROM.put(CONFIG_OFFSET, Config);
          EEPROM.commit();
  
          // Update new temperature
          DweetFailed = false;
          UpdateTemperature();
        }
      }
    }
    else
    {
      // Failed reading the Google Home feed.
      Serial.printf("[%s] Dweet get error = %d (%s)\n", getTime(false), httpCode, http.errorToString(httpCode).c_str());
  
      // Exit from here if the connection is refused.  Otherwise do nothing
      if(httpCode == HTTPC_ERROR_CONNECTION_REFUSED)
      {
#ifdef BUILTIN_LED
        digitalWrite(LED_BUILTIN, LED_OFF);
#endif  
        http.setReuse(false);
        http.end();
        DweetTime = millis();
        return;
      }
    }
  }
  
  // Now post current values
  DweetFailed = true;
  UpTime(buf1);
  postData = "http://dweet.io/dweet/for/" + String(DWEETNAME);
  postData += "?Location=" + String(Location[Config.DeviceLocation]);
  if(strlen(Config.CommonName) != 0) postData += "&CommonName=" + String(Config.CommonName);
  postData += "&Temperature=" + String(Last_T, 1);
  postData += "&Humidity=" + String(Last_H, 0) + "%";
  postData += "&Heat=" + String(HeatOn ? "ON" : "OFF");
  postData += "&Schedule=" + String(Schedule.Enabled ? "Enabled" : "Disabled");
  postData += "&Requested=" + String(RequestedTemp, 1) + String(Config.Celcius ? " C" : " F");
  postData += "&Cost=" + String("$ ") + String(Calc_HeatingCost(Config.HeatingTime + (HeatOn ? (CurrentTime - HeatOnTime) / OneSecond : 0)));
  postData += "&Heater_Size=" + String(Config.Watts) + " watts";
  postData += "&kWh_Cost=" + String(Config.kWh_Cost) + String(" cents");
  postData += "&kWh_Used=" + String(Calc_kWh(Config.HeatingTime + (HeatOn ? (CurrentTime - HeatOnTime) / OneSecond : 0))) + " kWh";
  postData += "&Firmware=" + String(FIRMWARE);
  postData += "&IP=" + WiFi.localIP().toString();
  postData += "&UpTime=" + String(buf1);  
  
  // Replace any spaces in postData with '%20'
  postData.replace(" ", "%20");
  http.setReuse(false);
  http.setTimeout(10000);
  http.begin(postData);
  httpCode = http.GET();
  if(httpCode == HTTP_CODE_OK) 
    DweetFailed = false; 
  else
    Serial.printf("[%s] Dweet post error = %d\n", getTime(false), httpCode);
  
  // End connection and save dweet time
  http.end();
  DweetTime = millis();
#ifdef BUILTIN_LED
  digitalWrite(LED_BUILTIN, LED_OFF);
#endif  
}

//
// This function is called whenever a Google Command is received from Adafruit IO
//
void GoogleCommand(AdafruitIO_Data *data)
{
  int     i;
  float   temp;
  String  MyLocation, ForLocation, CommonName, value;

  // Save initial values
  MyLocation = String(Location[Config.DeviceLocation]);
  MyLocation.toUpperCase();
  CommonName = String(Config.CommonName);
  CommonName.toUpperCase();
  value = data->value();
  value.toUpperCase();

  // Check if there's an apostrophe "'" in the location and remove spaces around it
  // Google Home will send "John ' s room" instead of "John's room"
  i = ForLocation.indexOf("'");
  if(i != -1) ForLocation = ForLocation.substring(0, i-1) + "'" + ForLocation.substring(i+2);

  // Extract destination location from message if there is one.
  // If there's none, then it's for all thermostats and it was 
  // changed/set by the Adafruit dashboard
  i = value.indexOf("=");
  if(i != -1)
  {
    ForLocation = value.substring(0, i);
    temp = value.substring(i+1).toFloat();
  }
  else
  {
    ForLocation = "HOME";
    temp = value.toFloat();

    // If temp is same as current RequestedTemp just exit
    if(temp == RequestedTemp) return;
  }

  // Is this request for MyLocation or for all thermostats
  if((MyLocation == ForLocation || (ForLocation == "HOME" && Config.DeviceLocation < ExcludeLocation) || CommonName == ForLocation) && strcmp(Config.LastUpdated, getTime(false)) != 0)
  {
    Serial.printf("[%s] MASTER = \"%s\"\n", getTime(false), value.c_str());
    CurrentTime = DisplayTime = millis();
    if(value.indexOf("SCHEDULE") != -1)
    {
      // I'm being asked to run the schedule.  Check if there's a valid temperature first...
      temp = RequestedTemp;
      RequestedTemp = ScheduledTemp(0.0F);
      if(RequestedTemp != 0.0F)
      {
        // There was one
        Schedule.Enabled = true;
        EEPROM.put(SCHEDULE_OFFSET, Schedule);
        EEPROM.commit();        

        // Save new temp to feed
        Serial.printf("[%s] Scheduled temperature now set to %s %c\n", getTime(false), String(RequestedTemp, 1).c_str(), (Config.Celcius ? 'C' : 'F'));
        IO_Schedule->save("ON");
      } 
      else 
      {
        // There's no schedule
        RequestedTemp = temp;
      }
    }
    else
    {
      if(temp < (Config.Celcius ? MinCelcius : MinFarenheit) || temp > (Config.Celcius ? MaxCelcius : MaxFarenheit))
        strcpy(googleText, "temp error");        
      else
      {
        // Save new temperature and disable scheduling
        sprintf(googleText, "-> %s <-", String(temp, 1).c_str());
        Config.MinTemp = RequestedTemp = temp;
        Schedule.Enabled = false;
        EEPROM.put(SCHEDULE_OFFSET, Schedule);
        EEPROM.commit();
        if(ForLocation == "HOME") IO_Schedule->save("OFF");
      }
    }

    // Update display with new value
    strcpy(Config.LastUpdated, getTime(false));
    Config.CRC = Calc_CRC();                        
    EEPROM.put(CONFIG_OFFSET, Config);
    UpdateTemperature();
  }
}

//
// This function receives data from the Adafruit Schedule feed
//
void IOSchedule(AdafruitIO_Data *data)
{
  String  value;
  float   temp;
  
  // Save initial value
  value = data->value();
  value.toUpperCase();

  // Make sure this is a different message
  if(strcmp(Config.LastUpdated, getTime(false)) == 0) return;
  Serial.printf("[%s] SCHEDULE = \"%s\"\n", getTime(false), value.c_str());

  // I'm being asked to run the schedule.  Check if there's a valid temperature first...
  temp = RequestedTemp;
  RequestedTemp = ScheduledTemp(0.0F);
  if(RequestedTemp != 0.0F && value == "ON")
  {
    // There was one
    Schedule.Enabled = true;
    EEPROM.put(SCHEDULE_OFFSET, Schedule);
    EEPROM.commit();        
    Serial.printf("[%s] Scheduled temperature now set to %s %c\n", getTime(false), String(RequestedTemp, 1).c_str(), (Config.Celcius ? 'C' : 'F'));
  } 
  else 
  {
    // There's no schedule or value was "OFF"
    RequestedTemp = Config.MinTemp;
    Schedule.Enabled = false;
    EEPROM.put(SCHEDULE_OFFSET, Schedule);
    EEPROM.commit();        
    Serial.printf("[%s] Temperature now set to %s %c\n", getTime(false), String(RequestedTemp, 1).c_str(), (Config.Celcius ? 'C' : 'F'));
  }

  // Update display with new value
  strcpy(Config.LastUpdated, getTime(false));
  Config.CRC = Calc_CRC();                        
  EEPROM.put(CONFIG_OFFSET, Config);
  UpdateTemperature();
}

//
// This function receives data from the Adafruit Location feed
//
void IOCommand(AdafruitIO_Data *data)
{
  // Current I'm doing nothing with this data
  float value = data->toFloat();
}

//
// Calculate kWh used by heater
//
float Calc_kWh(unsigned long HeatTime)
{
  float energyUsed = 0.00;
  
  // energyUsed = wattage * timeUsed (in hours) / 1000   (to get kWh used)
  energyUsed = (Config.Watts * (HeatTime / 3600.0F)) / 1000.0F;
  return(energyUsed);
}

//
// Calculate heating cost so far for this device
//
float Calc_HeatingCost(unsigned long HeatTime)
{
  float Cost = 0.00;
  
  // Do I have a valid kWh cost?
  if(Config.kWh_Cost == 0.00F) return(0.00F);

  // cost = energyUsed * rate;
  // There could be more complex rate fees in different areas...
  // Like delivery charge, meter charge, grease the pockets of Power Utility charge, etc, etc, etc...
  Cost = Calc_kWh(HeatTime) * (Config.kWh_Cost / 100.0);
  return(Cost);
}

//
// Return fused MAC address (it is stored backwards)
//
char *FusedMAC(void)
{
  static char buf[15];
  
  uint64_t chipid = ESP.getEfuseMac();
  sprintf(buf, "%04X", (uint16_t)(chipid>>32));
  sprintf(&buf[4], "%08X", (uint32_t)chipid);
  return buf;
}

//
// Convert RSSI value to percentage
//
int rssiToQualityPercentage(void)
{
  int quality, rssi;
  rssi = WiFi.RSSI();
  if(rssi <= -100) 
    quality = 0;
  else 
    if(rssi >= -50) quality = 100; quality = 2 * (rssi + 100);
  return quality;
}

//
// Display RSSI icon
//
void renderRSSIIcon(void)
{
#ifdef OLED_Display  
  #define RSSIICON_STARTX      99
  #define RSSIICON_STARTY      41
  #define RSSIICON_STARTHEIGHT 4
#else  
  #define RSSIICON_STARTX      69 
  #define RSSIICON_STARTY      27 
  #define RSSIICON_STARTHEIGHT 3
#endif
  #define RSSIICON_BARWIDTH    2

  int quality = rssiToQualityPercentage();
  if(quality != 0) display.fillRect(RSSIICON_STARTX, RSSIICON_STARTY, RSSIICON_BARWIDTH, RSSIICON_STARTHEIGHT, BLACK);
  if(quality >= 45) display.fillRect(RSSIICON_STARTX + 3, RSSIICON_STARTY - 1, RSSIICON_BARWIDTH, RSSIICON_STARTHEIGHT + 1, BLACK);      
  if(quality >= 70) display.fillRect(RSSIICON_STARTX + 6, RSSIICON_STARTY - 2, RSSIICON_BARWIDTH, RSSIICON_STARTHEIGHT + 2, BLACK);      
  if(quality >= 90) display.fillRect(RSSIICON_STARTX + 9, RSSIICON_STARTY - 3, RSSIICON_BARWIDTH, RSSIICON_STARTHEIGHT + 3, BLACK);
}

//
// Find OTA services
//
bool findService(char *ScanName, int *Count)
{
  bool  Found = false;
  int   cntr = 0;
  char  buf[32], buf1[32];

  // Build name to search for on network
  sprintf(buf, "Thermo-%s", ScanName);
  strcpy(buf1, "Thermo-OTA");
  
  // Do I need to start the mDNS service?
  if(ArduinoOTA.getHostname().length() == 0) 
  {
    Serial.print("Starting mDNS service... ");
    if(!MDNS.begin(buf1))
    {
     // Return TRUE as the code will think that I've found another device with the same location name
     Serial.println("Failed!");    
     *Count = cntr;
     return(true);
    }
    else
     Serial.println("OK!");    
  }

  // Scan for devices that have Arduino OTA enabled
  for(int j=0; j<3; j++)
  {
    delay(500);
    MDNS_Services = MDNS.queryService("arduino", "tcp");
    if(MDNS_Services != 0) 
    {
      for(int i = 0; i < MDNS_Services; i++) 
      {
        // Did I find a thermostat?
        Serial.printf("  %d: %s (%s)\n", i+1, MDNS.hostname(i).c_str(), MDNS.IP(i).toString().c_str());
        if(MDNS.hostname(i).indexOf("Thermo-") != -1) ++cntr;
        if(MDNS.hostname(i).indexOf(buf) != -1) Found = true;
      }
    } 
    if(MDNS_Services != 0) break; else Serial.printf("Scan %d, no services found...\n", j+1);
  }
  
  // Return scan result
  *Count = cntr;
  return(Found);
}

//
// Get time from NTP time server 
//
time_t getNTPTime(void) 
{
  const int NTP_PACKET_SIZE = 48;
  byte packetBuffer[ NTP_PACKET_SIZE];

  // time.nist.gov NTP server
  IPAddress timeServer(129, 6, 15, 28); 

  // Clear packet buffer
  memset(packetBuffer, 0, NTP_PACKET_SIZE);

  // Initialize values needed to form NTP request
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision

  // 8 bytes of zero for Root Delay & Root Dispersion (bytes 4 to 11)
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // Send UDP packet
  WiFiUDP Udp;
  if(!Udp.begin(2390)) { Serial.println("[getNTPTime] begin() failed!"); return(0); }
  if(!Udp.beginPacket(timeServer, 123)) { Serial.println("[getNTPTime] beginPacket() failed!"); return(0); }
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();

  // Wait for reply
  for(int i=0; i<2; i++)
  {
    if(Udp.parsePacket())
    {
      Udp.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);

      // combine the four bytes (two words) into a long integer
      // this is NTP time (seconds since Jan 1 1900):
      unsigned long secsSince1900 = highWord << 16 | lowWord;

      // Convert NTP time into unix time
      // Unix time starts on Jan 1 1970. In seconds, that's 2208988800L
      unsigned long UTC_Time = secsSince1900 - 2208988800UL;

      // All done, set time and exit
      return(myTZ.toLocal(UTC_Time));
    }

    // Wait a second
    delay(1000);
  }

  // Failed to receive time after 3 attemps
  return(0);
}

//
// Draw the Arduino Logo on the display
//
void ArduinoLogo(int Pause)
{
  // Draw on PCD8544 or SH1106 display
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(BLACK, WHITE);
#ifdef OLED_Display  
  display.println(FormatText(FIRMWARE, CENTER));
#endif  
  display.println(FormatText("Powered by", CENTER));
  #ifdef Arduino_Logo_width && OLED_Display
    // Method 1:  Display the XBM logo
    display.drawXBitmap((display.width() - Arduino_Logo_width)/2, display.height() - Arduino_Logo_height, Arduino_Logo_bits, Arduino_Logo_width, Arduino_Logo_height, BLACK);  
  #else
    // Method 2:  Draw circles
    int i = display.width();
    int h = (display.height()/2);
    display.println();
    display.println();
    display.println();
    display.println();
  #ifdef OLED_Display  
      display.println(FormatText("ARDUINO", CENTER));
  #else
      display.println(FormatText(FIRMWARE, CENTER));
  #endif    
    display.drawCircle((i/2)-8, h, 9, BLACK);
    display.drawCircle((i/2)-8, h, 8, BLACK);
    display.drawCircle((i/2)-8, h, 7, BLACK);
    display.drawCircle((i/2)+8, h, 9, BLACK);
    display.drawCircle((i/2)+8, h, 8, BLACK);
    display.drawCircle((i/2)+8, h, 7, BLACK);
    display.drawLine((i/2)-12, h, (i/2)-4, h, BLACK); 
    display.drawLine((i/2)+4, h, (i/2)+12, h, BLACK); 
    display.drawLine((i/2)+8, h-4, (i/2)+8, h+4, BLACK); 
  #endif    
    display.display();
  if(Pause > 0) delay(Pause);
}

//
// Calculate CRC-32 of Config record
//
unsigned int Calc_CRC(void)
{
  const uint polynomial = 0x04C11DB7;
  uint crc = 0;
  char *ptr = (char *)&Config;
  byte b;
  
  for(int j=0; j<sizeof(Config)-sizeof(Config.CRC); ptr++, j++)
  {
    b = (byte)*(ptr);
    crc ^= (uint)(b << 24);
    for(int i = 0; i < 8; i++)
    {
      if ((crc & 0x80000000) != 0)
        crc = (uint)((crc << 1) ^ polynomial);
      else            
        crc <<= 1;
    }
  }
  return crc;
}

//
// Process a web client request
//
void  ProcessClient(void)
{
  bool        Icon = false;
  bool        Post = false;
  bool        Reset = false;
  bool        Conf = false;
  bool        NameError = false;
  bool        Scheduling = false;
  bool        isValid = false;
  int         chk, ChartType;
  String      Val, Line = "";
  char        buf[30];
  WiFiClient  WebClient = WebServer.available();

  // Do I have a client to process?
  if(!WebClient) return;
  
  // I have a client connecting
  DisplayTime = CurrentTime;
  strcpy(googleText, "WebClient");
  UpdateTemperature();

  // Save current stats
  EEPROM.put(STATS_OFFSET, HeatingStats);
  EEPROM.commit();
  
  // Loop here while the client is still connected
  ChartType = HOURLY_TEMP;
  while(WebClient.connected())
  {
    if(WebClient.available()) 
    {
      Line = WebClient.readStringUntil('\n');
      if(Line == "\r")
      {
        // Is browser asking for the favorite icon?
        if(Icon) { SendIcon(WebClient); break; }

        // Send the HTML header information and requested chart
        if(Reset || Conf || Post) ChartType = CHART_NONE;
        WebPageHead(WebClient, Location[Config.DeviceLocation], ChartType);
        WebClient.printf("<center><H3>Device = <a href=\"https://dweet.io/follow/Thermo-");
        WebClient.printf("%s\" target=\"_blank\">%s</a>", FusedMAC(), FusedMAC());            
        if(strcmp(Config.Master, FusedMAC()) == 0) WebClient.print(" / MASTER");
        WebClient.println("</H3></center>");

        // Was this a schedule POST command?
        if(Post)
        {
          // Extract info from posted data
          Line = WebClient.readStringUntil('\n');
          for(int i=0; i<4; i++)
          {
            for(int j=0; j<7; j++)
            {
              // Get temperature, hour and minute
              sprintf(buf, "TEMP_%d.%d", i, j);
              Val = Line.substring(Line.indexOf(buf)+9);
              if(Val.indexOf("&") != -1) Val = Val.substring(0, Val.indexOf("&"));
              Schedule.Day[j].SetTemp[i] = Val.toFloat();
              sprintf(buf, "HOUR_%d.%d", i, j);
              Val = Line.substring(Line.indexOf(buf)+9);                
              if(Val.indexOf("&") != -1) Val = Val.substring(0, Val.indexOf("&"));
              if(Val != "-1")
              {
                Schedule.Day[j].SetHour[i] = Val.toInt();
                sprintf(buf, "MINUTE_%d.%d", i, j);
                Val = Line.substring(Line.indexOf(buf)+11);                
                if(Val.indexOf("&") != -1) Val = Val.substring(0, Val.indexOf("&"));
                if(Val != "-1")
                {
                  Schedule.Day[j].SetMinute[i] = Val.toInt();
                  Schedule.Day[j].Valid[i] = isValid = true;
                }
                else 
                  Schedule.Day[j].Valid[i] = false;
              } else Schedule.Day[j].Valid[i] = false;
            }
          }

          // Am I copying Monday to all week days?
          if(isValid && Line.indexOf("MONDAY=") != -1)
          {
            for(int j=2; j<6; j++)
            {
              for(int i=0; i<4; i++)
              {
                Schedule.Day[j].SetHour[i] = Schedule.Day[1].SetHour[i];
                Schedule.Day[j].SetMinute[i] = Schedule.Day[1].SetMinute[i];
                Schedule.Day[j].SetTemp[i] = Schedule.Day[1].SetTemp[i];
                Schedule.Day[j].Valid[i] = Schedule.Day[1].Valid[i];
              }
            }
          }

          // Am I copying this schedule to all thermostats?
          if(isValid && Line.indexOf("COPY=") != -1) BroadcastSchedule(WebClient, Line);

          // Am I enabling the schedule?
          if(isValid && Line.indexOf("ENABLED=") != -1) 
          {
            Schedule.Enabled = true; 
            RequestedTemp = ScheduledTemp(Config.MinTemp);
          }
          else 
          {
            // Schedule is disabled, set temperature to Config.MinTemp
            Schedule.Enabled = false;
            RequestedTemp = Config.MinTemp;
          }

          // If schedule is valid, save to EEPROM memory
          if(isValid)
          {
            EEPROM.put(SCHEDULE_OFFSET, Schedule);
            EEPROM.commit();
          }

          // Send back updated schedule
          ChartType = CHART_NONE; 
          Scheduling = true;
        }
        
        // Did they try to change the common name and got an error?
        if(NameError) WebClient.print("<center><font color=\"#FF0000\">Common Name can not be a Location Name!<font color=\"#000000\"><br><br>");
        
        // Send back the requested chart or schedule
        if(!Scheduling)
          WebPageChart(WebClient, ChartType);
        else
          SendSchedule(WebClient, isValid);
          
        // Send page bottom info
        WebClient.printf("<hr><center>%s / Firmware %s<br>", ARDUINO_VARIANT, FIRMWARE);
        WebClient.printf("Temperature %s %c, Humidity %s%%, Heating is %s<br>Requested %s %c, Scheduling is %s<br>", 
          String(Last_T, 1).c_str(), (Config.Celcius ? 'C' : 'F'), String(Last_H, 0).c_str(), (HeatOn ? "On" : "Off"),
          String(RequestedTemp, 1).c_str(), (Config.Celcius ? 'C' : 'F'), (Schedule.Enabled ? "enabled" : "disabled"));

        // Insert buttons for charts
        if(!Reset && !Conf)
        {
          WebClient.println("<form id=\"form\" method=\"get\" enctype=\"application/x-www-form-urlencoded\" action=\"/\">");
          WebClient.println("<br><center><input name=\"TYPE\" type=\"radio\" checked value=\"0\">Temperature&nbsp<input name=\"TYPE\" type=\"radio\" value=\"1\">Cost&nbsp<input name=\"TYPE\" type=\"radio\" value=\"2\">kWh");
          WebClient.println("<br><input type=\"button\" onclick=\"myFunction('/Hourly=')\" value=\"Hourly\">");
          WebClient.println("<input type=\"button\" onclick=\"myFunction('/Daily=')\" value=\"Daily\">");
          WebClient.println("<input type=\"button\" onclick=\"myFunction('/Monthly=')\" value=\"Monthly\">");
          WebClient.println("<input type=\"button\" onclick=\"myFunction('/Compare=')\" value=\"Comparison\">");
          if(!Scheduling) WebClient.println("<input type=\"button\" onclick=\"window.location.href='/Schedule'\" value=\"Schedule\">");
          WebClient.println("<script>function myFunction(Value) { window.location.href=Value + document.getElementById(\"form\").elements.namedItem(\"TYPE\").value; }</script>");
        }
        if(Reset || Conf) 
        {
          if(Reset) 
            WebClient.println("<hr><br><center><H1><font color=\"#FF0000\">Performing Factory Reset!<font color=\"#000000\"></H1></center>");
          else
            WebClient.println("<hr><br><center><H1><font color=\"#FF0000\">Entering Configuration Mode!<font color=\"#000000\"></H1></center>");
        }
        else          
          WebClient.println("<hr>Created by <a href=\"mailto:claudegbeaudoin@hotmail.com?Subject=Google Thermostat\">Claude G. Beaudoin</a></center>");
              
        // Finish HTML page
        WebClient.print("</body></html>");
        WebClient.println();
        WebClient.println();
        break;
      } 
      else 
      {
        // Check what the client request was for
        if(Line.indexOf("GET") != -1) 
        {
          Val = Line.substring(0, Line.indexOf("HTTP")-1);
          Serial.printf("[%s] WebRequest = %s\n", getTime(false), Val.c_str());
          Line.toUpperCase();
          if(Line.indexOf("/HOURLY=0") != -1) ChartType = HOURLY_TEMP;
          if(Line.indexOf("/HOURLY=1") != -1) ChartType = HOURLY_COST;
          if(Line.indexOf("/HOURLY=2") != -1) ChartType = HOURLY_KWH;
          if(Line.indexOf("/DAILY=0") != -1) ChartType = DAILY_TEMP;
          if(Line.indexOf("/DAILY=1") != -1) ChartType = DAILY_COST;
          if(Line.indexOf("/DAILY=2") != -1) ChartType = DAILY_KWH;
          if(Line.indexOf("/MONTHLY=0") != -1) ChartType = MONTHLY_TEMP;
          if(Line.indexOf("/MONTHLY=1") != -1) ChartType = MONTHLY_COST;
          if(Line.indexOf("/MONTHLY=2") != -1) ChartType = MONTHLY_KWH;
          if(Line.indexOf("/COMPARE=0") != -1) ChartType = COMPARE_TEMP;
          if(Line.indexOf("/COMPARE=1") != -1) ChartType = COMPARE_COST;
          if(Line.indexOf("/COMPARE=2") != -1) ChartType = COMPARE_KWH;
          if(Line.indexOf("/SCHEDULE") != -1) { ChartType = CHART_NONE; Scheduling = true; }
          if(Line.indexOf("/FAVICON.ICO") != -1) Icon = true;
          if(Line.indexOf("/FACTORY") != -1) Reset = true;
          if(Line.indexOf("/CONFIG") != -1) Conf = true;
          if(Line.indexOf("/COMMON.NAME=") != -1)
          {
            // Change location Common Name.  Common Name can not be a location name
            Val = Val.substring(Val.indexOf("=")+1);
            Val.replace("%22", ""); Val.replace("%20", " ");
            for(chk=0; chk<MaxLocations; chk++) if(strcmp(Location[chk], Val.c_str()) == 0) NameError = true;
            if(Val.length() < sizeof(Config.CommonName) && !NameError) 
            {
              strcpy(Config.CommonName, Val.c_str());
              Config.CRC = Calc_CRC();
              EEPROM.put(CONFIG_OFFSET, Config);
              EEPROM.commit();
            } 
          }
#ifdef BackLight
          if(Line.indexOf("/CONTRAST=") != -1)
          {
            // Extract value and validate it
            int Value = Line.substring(Line.indexOf("=")+1).toInt();
            if(Value >= 35 && Value <= 70)
            {
              Config.Contrast = Value;
              Config.CRC = Calc_CRC();
              EEPROM.put(CONFIG_OFFSET, Config);
              EEPROM.commit();
              display.setContrast(Config.Contrast);
            }
          }
#endif          
        }
        if(Line.indexOf("POST") != -1) 
        {
          Val = Line.substring(0, Line.indexOf("HTTP")-1);
          Serial.printf("[%s] WebRequest = %s\n", getTime(false), Val.c_str());
          Post = true;
        }
      }
    }
  };

  // Disconnect client
  WebClient.println();
  WebClient.flush();
  WebClient.stop();

  // Was a factory reset asked for?
  if(Reset)
  {
    // Perform a factory reset
    Serial.printf("[%s] Performing a factory reset...\n\n", getTime(false));
    delay(2000);
    memset(&Config, NULL, sizeof(Config));
    memset(&HeatingStats, NULL, sizeof(HeatingStats));
    memset(&Schedule, NULL, sizeof(Schedule));
    EEPROM.put(CONFIG_OFFSET, Config);
    EEPROM.put(STATS_OFFSET, HeatingStats);
    EEPROM.put(SCHEDULE_OFFSET, Schedule);
    EEPROM.commit();
    WiFi.disconnect();
    ESP.restart();
    while(1);    
  }

  // Am I reconfiguring?
  if(Conf)
  {
    // Set SoftReboot to 10 and reboot
    Serial.printf("[%s] Restarting into configuration...\n\n", getTime(false));
    delay(2000);
    Config.SoftReboot = 10;
    Config.CRC = Calc_CRC();
    EEPROM.put(CONFIG_OFFSET, Config);
    EEPROM.commit();
    WiFi.disconnect();
    ESP.restart();
    while(1);    
  }
  strcpy(googleText, "");
  UpdateTemperature();  
}

//
// Send back the favorite icon as a document not found
//
void SendIcon(WiFiClient WebClient)
{
  String HTML = "<!DOCTYPE html><html><head></head><body>Not Found</body></html>";
  
  WebClient.println("HTTP/1.1 404 Not Found");
  WebClient.println("Content-type: text/html");
  WebClient.printf("Content-Length: %d\n", HTML.length());
  WebClient.println("Connection: close");
  WebClient.println();
  WebClient.printf("%s\n", HTML.c_str());
  WebClient.println();  
}

//
// Send the web page header information to client
//
void  WebPageHead(WiFiClient WebClient, char *PageTitle, int ChartType)
{
  // Send back response header
  WebClient.println("HTTP/1.1 200 OK");
  WebClient.println("Content-type: text/html");
  WebClient.println("Cache-Control: no-cache");
  WebClient.println("Connection: close");
  WebClient.println();
  WebClient.printf("<!DOCTYPE html><html><head><title>%s Thermostat</title>", PageTitle);
  WebClient.println("<style type=\"text/css\"> body { color:#000000; background-color:#C0C0C0; } a  { color:#0000FF; } a:visited { color:#800080; } a:hover { color:#008000; } a:active { color:#FF0000; }</style>");
  if(ChartType != CHART_NONE)
  {
    WebClient.println("<META HTTP-EQUIV=\"refresh\" CONTENT=\"300\">"); // Auto refresh page every 300 seconds (5 minutes)
    WebClient.println("<link  id='GoogleFontsLink' href='https://fonts.googleapis.com/css?family=Cabin' rel='stylesheet' type='text/css'>");
    WebClient.println("<link  id='GoogleFontsLink' href='https://fonts.googleapis.com/css?family=Open Sans' rel='stylesheet' type='text/css'>");
    WebClient.println("<link  id='GoogleFontsLink' href='https://fonts.googleapis.com/css?family=Indie Flower' rel='stylesheet' type='text/css'>");
    WebClient.println("<script>WebFontConfig = { google: {families: [\"Cabin\",\"Open Sans\",\"Indie Flower\",]},active: function() { DrawTheChart(ChartData,ChartOptions,\"chart-01\",\"Bar\")}};</script>");
    WebClient.println("<script asyn src=\"https://livegap.com/charts/js/webfont.js\"></script>");
    WebClient.println("<script src=\"https://livegap.com/charts/js/Chart.min.js\"></script>");
    WebClient.println("<script>function DrawTheChart(ChartData,ChartOptions,ChartId,Type)");
    WebClient.println("{eval('var myLine = new Chart(document.getElementById(ChartId).getContext(\"2d\")).'+Type+'(ChartData,ChartOptions);document.getElementById(ChartId).getContext(\"2d\").stroke();')}</script>");
  }
  WebClient.println("</head><body>");
}

//
// Send the HTML code to generate the chart
//
void  WebPageChart(WiFiClient WebClient, int ChartType)
{
  int     i, j, MaxDays, ChartWidth, ChartHeight;
  char    buf[50];
  float   Temperature, Humidity, ScaleMin, ScaleMax;
  String  Title, SubTitle, Labels, LeftAxisLabel, yAxisUnit, Legend, DataSet1, Set1Title, DataSet2, Set2Title, DataSet3, Type1, Type2, Type3;
  time_t  Today = now();
  
  // Build data based on the chart type
  MaxDays = 22;       // You can change this up to 45 days.
  ChartWidth = 800;   // Defines the chart width
  ChartHeight = 400;  // Defines the chart height
  Title = String(Location[Config.DeviceLocation]);
  if(strlen(Config.CommonName) != 0) Title += " (" + String(Config.CommonName) + ")";
  Title += " Thermostat";
  switch(ChartType)
  {
    case HOURLY_TEMP:
    case HOURLY_COST:
    case HOURLY_KWH:
      // Build labels and datasets
      ScaleMin = 99.9;
      ScaleMax = 0.0;
      for(i=0, j=hour(); i<24; i++) 
      { 
        sprintf(buf, "\"%02d:00\"%c", j, (i != 23 ? ',' : ']')); 
        Labels += String(buf); 
        if(HeatingStats.Hourly[j].Samples != 0) 
        {
          Temperature = HeatingStats.Hourly[j].Temperature / HeatingStats.Hourly[j].Samples;
          Humidity = HeatingStats.Hourly[j].Humidity / HeatingStats.Hourly[j].Samples;
        }
        else
        {
          Temperature = HeatingStats.Hourly[j].Temperature;
          Humidity = HeatingStats.Hourly[j].Humidity;
        }         
        if(String(Humidity, 0) == "nan" || String(Humidity, 0) == "inf") Humidity = 0.00;        
        if(String(Temperature, 1) == "nan" || String(Temperature, 1) == "inf") Temperature = 0.00;        
        DataSet1 += "\"" + String(Humidity, 0) + "\"" + String((i != 23 ? "," : "]"));
        DataSet2 += "\"" + String(Temperature, 1) + "\"" + String((i != 23 ? "," : "]"));
        if(ChartType == HOURLY_COST) 
        {
          if(Calc_HeatingCost(HeatingStats.Hourly[j].HeatingTime) < ScaleMin) ScaleMin = Calc_HeatingCost(HeatingStats.Hourly[j].HeatingTime);
          if(Calc_HeatingCost(HeatingStats.Hourly[j].HeatingTime) > ScaleMax) ScaleMax = Calc_HeatingCost(HeatingStats.Hourly[j].HeatingTime);
          DataSet3 += "\"" + String(Calc_HeatingCost(HeatingStats.Hourly[j].HeatingTime), 2) + "\"" + String((i != 23 ? "," : "]"));
        }
        if(ChartType == HOURLY_KWH) 
        {
          if(Calc_kWh(HeatingStats.Hourly[j].HeatingTime) < ScaleMin) ScaleMin = Calc_kWh(HeatingStats.Hourly[j].HeatingTime);
          if(Calc_kWh(HeatingStats.Hourly[j].HeatingTime) > ScaleMax) ScaleMax = Calc_kWh(HeatingStats.Hourly[j].HeatingTime);
          DataSet3 += "\"" + String(Calc_kWh(HeatingStats.Hourly[j].HeatingTime), 3) + "\"" + String((i != 23 ? "," : "]"));
        }
        if(ChartType == HOURLY_TEMP)
        {
          if(Temperature < ScaleMin) ScaleMin = Temperature;
          if(Humidity < ScaleMin) ScaleMin = Humidity;
          if(Temperature > ScaleMax) ScaleMax = Temperature;
          if(Humidity > ScaleMax) ScaleMax = Humidity;
        }
        if(--j < 0) j=23; 
      }
      SubTitle = "Hourly ";
      Type1 = Type2 = "Line";
      if(ChartType == HOURLY_TEMP) { SubTitle += "Temperature"; yAxisUnit = (Config.Celcius ? "Celcius" : "Farenheit"); LeftAxisLabel = "% Humidity"; }
      if(ChartType == HOURLY_COST) { SubTitle += "Heating Cost"; yAxisUnit = "$ $ $"; LeftAxisLabel = ""; DataSet1 = DataSet3; }
      if(ChartType == HOURLY_KWH) { SubTitle += "kWh Consumption"; yAxisUnit = "kWh"; LeftAxisLabel = ""; DataSet1 = DataSet3; }
      Legend = "false";
      Set1Title = "Humidity";
      Set2Title = "Temperature";
      break;
    case DAILY_TEMP:
    case DAILY_COST:
    case DAILY_KWH:
      // Build labels and datasets
      ScaleMin = 99.9;
      ScaleMax = 0.0;
      for(i=0; i<MaxDays; i++) 
      { 
        if(i == 0 || i == MaxDays-1) 
          sprintf(buf, "\"%s-%02d\"%c", monthShortStr(month(Today)), day(Today), (i != MaxDays-1 ? ',' : ']'));
        else 
          { if(i % 3 == 0) sprintf(buf, "\"%s-%02d\",", monthShortStr(month(Today)), day(Today)); else strcpy(buf, "\"\","); }
        Labels += String(buf); 
        if(HeatingStats.Daily[i].Samples != 0) 
        {
          Temperature = HeatingStats.Daily[i].Temperature / HeatingStats.Daily[i].Samples;
          Humidity = HeatingStats.Daily[i].Humidity / HeatingStats.Daily[i].Samples;
        }
        else
        {
          Temperature = HeatingStats.Daily[i].Temperature;
          Humidity = HeatingStats.Daily[i].Humidity;
        }         
        if(String(Humidity, 0) == "nan" || String(Humidity, 0) == "inf") Humidity = 0.00;        
        if(String(Temperature, 1) == "nan" || String(Temperature, 1) == "inf") Temperature = 0.00;        
        DataSet1 += "\"" + String(Humidity, 0) + "\"" + String((i != MaxDays-1 ? "," : "]"));
        DataSet2 += "\"" + String(Temperature, 1) + "\"" + String((i != MaxDays-1 ? "," : "]"));
        if(ChartType == DAILY_COST) 
        {
          if(Calc_HeatingCost(HeatingStats.Daily[i].HeatingTime) < ScaleMin) ScaleMin = Calc_HeatingCost(HeatingStats.Daily[i].HeatingTime);
          if(Calc_HeatingCost(HeatingStats.Daily[i].HeatingTime) > ScaleMax) ScaleMax = Calc_HeatingCost(HeatingStats.Daily[i].HeatingTime);
          DataSet3 += "\"" + String(Calc_HeatingCost(HeatingStats.Daily[i].HeatingTime), 2) + "\"" + String((i != MaxDays-1 ? "," : "]"));
        }
        if(ChartType == DAILY_KWH) 
        {
          if(Calc_kWh(HeatingStats.Daily[i].HeatingTime) < ScaleMin) ScaleMin = Calc_kWh(HeatingStats.Daily[i].HeatingTime);
          if(Calc_kWh(HeatingStats.Daily[i].HeatingTime) > ScaleMax) ScaleMax = Calc_kWh(HeatingStats.Daily[i].HeatingTime);
          DataSet3 += "\"" + String(Calc_kWh(HeatingStats.Daily[i].HeatingTime), 3) + "\"" + String((i != MaxDays-1 ? "," : "]"));
        }
        if(ChartType == DAILY_TEMP)
        {
          if(Temperature < ScaleMin) ScaleMin = Temperature;
          if(Humidity < ScaleMin) ScaleMin = Humidity;
          if(Temperature > ScaleMax) ScaleMax = Temperature;
          if(Humidity > ScaleMax) ScaleMax = Humidity;
        }
        Today -= SECS_PER_DAY;
      }
      SubTitle = "Daily ";
      Type1 = Type2 = "Line";
      if(ChartType == DAILY_TEMP) { SubTitle += "Temperature"; yAxisUnit = (Config.Celcius ? "Celcius" : "Farenheit"); LeftAxisLabel = "% Humidity"; }
      if(ChartType == DAILY_COST) { SubTitle += "Heating Cost"; yAxisUnit = "$ $ $"; LeftAxisLabel = ""; DataSet1 = DataSet3; }
      if(ChartType == DAILY_KWH) { SubTitle += "kWh Consumption"; yAxisUnit = "kWh"; LeftAxisLabel = ""; DataSet1 = DataSet3; }
      Legend = "false";
      Set1Title = "Humidity";
      Set2Title = "Temperature";
      break;
    case MONTHLY_TEMP:
    case MONTHLY_COST:
    case MONTHLY_KWH:
      // Build labels and datasets
      ScaleMin = 99.9;
      ScaleMax = 0.0;
      for(i=0, j=month(); i<12; i++) 
      { 
        sprintf(buf, "\"%s\"%c", monthStr(j), (i != 11 ? ',' : ']'));
        Labels += String(buf); 
        if(HeatingStats.Monthly[0][j-1].Samples != 0) 
        {
          Temperature = HeatingStats.Monthly[0][j-1].Temperature / HeatingStats.Monthly[0][j-1].Samples;
          Humidity = HeatingStats.Monthly[0][j-1].Humidity / HeatingStats.Monthly[0][j-1].Samples;
        }
        else
        {
          Temperature = HeatingStats.Monthly[0][j-1].Temperature;
          Humidity = HeatingStats.Monthly[0][j-1].Humidity;
        }         
        if(String(Humidity, 0) == "nan" || String(Humidity, 0) == "inf") Humidity = 0.00;        
        if(String(Temperature, 1) == "nan" || String(Temperature, 1) == "inf") Temperature = 0.00;        
        DataSet1 += "\"" + String(Humidity, 0) + "\"" + String((i != 11 ? "," : "]"));
        DataSet2 += "\"" + String(Temperature, 1) + "\"" + String((i != 11 ? "," : "]"));
        if(ChartType == MONTHLY_COST) 
        {
          if(Calc_HeatingCost(HeatingStats.Monthly[0][j-1].HeatingTime) < ScaleMin) ScaleMin = Calc_HeatingCost(HeatingStats.Monthly[0][j-1].HeatingTime);
          if(Calc_HeatingCost(HeatingStats.Monthly[0][j-1].HeatingTime) > ScaleMax) ScaleMax = Calc_HeatingCost(HeatingStats.Monthly[0][j-1].HeatingTime);
          DataSet3 += "\"" + String(Calc_HeatingCost(HeatingStats.Monthly[0][j-1].HeatingTime), 2) + "\"" + String((i != 11 ? "," : "]"));
        }
        if(ChartType == MONTHLY_KWH) 
        {
          if(Calc_kWh(HeatingStats.Monthly[0][j-1].HeatingTime) < ScaleMin) ScaleMin = Calc_kWh(HeatingStats.Monthly[0][j-1].HeatingTime);
          if(Calc_kWh(HeatingStats.Monthly[0][j-1].HeatingTime) > ScaleMax) ScaleMax = Calc_kWh(HeatingStats.Monthly[0][j-1].HeatingTime);
          DataSet3 += "\"" + String(Calc_kWh(HeatingStats.Monthly[0][j-1].HeatingTime), 3) + "\"" + String((i != 11 ? "," : "]"));
        }
        if(ChartType == MONTHLY_TEMP)
        {
          if(Temperature < ScaleMin) ScaleMin = Temperature;
          if(Humidity < ScaleMin) ScaleMin = Humidity;
          if(Temperature > ScaleMax) ScaleMax = Temperature;
          if(Humidity > ScaleMax) ScaleMax = Humidity;
        }
        if(--j < 1) j = 12;
      }
      SubTitle = "Monthly ";
      Type1 = Type2 = "Line";
      if(ChartType == MONTHLY_TEMP) { SubTitle += "Temperature"; yAxisUnit = (Config.Celcius ? "Celcius" : "Farenheit"); LeftAxisLabel = "% Humidity"; }
      if(ChartType == MONTHLY_COST) { SubTitle += "Heating Cost"; yAxisUnit = "$ $ $"; LeftAxisLabel = ""; DataSet1 = DataSet3; }
      if(ChartType == MONTHLY_KWH) { SubTitle += "kWh Consumption"; yAxisUnit = "kWh"; LeftAxisLabel = ""; DataSet1 = DataSet3; }
      Legend = "false";
      Set1Title = "Humidity";
      Set2Title = "Temperature";
      break;
    case COMPARE_TEMP:
    case COMPARE_COST:
    case COMPARE_KWH:
      // Build labels and datasets
      ScaleMin = 99.9;
      ScaleMax = 0.0;
      for(i=0, j=month(); i<12; i++) 
      { 
        sprintf(buf, "\"%s\"%c", monthShortStr(j), (i != 11 ? ',' : ']'));
        Labels += String(buf); 
        if(ChartType == COMPARE_TEMP)
        {
          if(HeatingStats.Monthly[0][j-1].Samples != 0) 
            Temperature = HeatingStats.Monthly[0][j-1].Temperature / HeatingStats.Monthly[0][j-1].Samples;
          else
            Temperature = HeatingStats.Monthly[0][j-1].Temperature;
          Humidity = HeatingStats.Monthly[1][j-1].Temperature;
        }
        if(ChartType == COMPARE_COST) 
        {
          Temperature = Calc_HeatingCost(HeatingStats.Monthly[0][j-1].HeatingTime);
          Humidity = Calc_HeatingCost(HeatingStats.Monthly[1][j-1].HeatingTime);
        }
        if(ChartType == COMPARE_KWH) 
        {
          Temperature = Calc_kWh(HeatingStats.Monthly[0][j-1].HeatingTime);
          Humidity = Calc_kWh(HeatingStats.Monthly[1][j-1].HeatingTime);
        }
        if(String(Humidity, 0) == "nan" || String(Humidity, 0) == "inf") Humidity = 0.00;        
        if(String(Temperature, 1) == "nan" || String(Temperature, 1) == "inf") Temperature = 0.00;        
        DataSet1 += "\"" + String(Temperature, 1) + "\"" + String((i != 11 ? "," : "]"));
        DataSet2 += "\"" + String(Humidity, 1) + "\"" + String((i != 11 ? "," : "]"));
        if(Temperature < ScaleMin) ScaleMin = Temperature;
        if(Humidity < ScaleMin) ScaleMin = Humidity;
        if(Temperature > ScaleMax) ScaleMax = Temperature;
        if(Humidity > ScaleMax) ScaleMax = Humidity;
        if(--j < 1) j = 12;
      }
      SubTitle = "Monthly ";
      if(ChartType == COMPARE_TEMP) { SubTitle += "Temperature"; yAxisUnit = (Config.Celcius ? "Celcius" : "Farenheit"); }
      if(ChartType == COMPARE_COST) { SubTitle += "Cost"; yAxisUnit = "$ $ $"; LeftAxisLabel = ""; }
      if(ChartType == COMPARE_KWH) { SubTitle += "kWh"; yAxisUnit = "kWh"; LeftAxisLabel = ""; }
      SubTitle += " Comparison";
      LeftAxisLabel = "";
      Legend = "true";
      Set1Title = String(year());
      Set2Title = String(year()-1);
      Type1 = Type2 = "Bar";
      break;
    case CHART_NONE:
      return;
  }

  // Make sure min and max are not the same
  if(String(ScaleMin, 2) == String(ScaleMax, 2)) ScaleMax += 0.5;
  
  // Fill in the chart data
  WebClient.printf("<center><canvas id=\"chart-01\" width=\"%d\" height=\"%d\" style=\"background-color:rgba(255,255,255,1.00);border-radius:5px;width:%dpx;height:%dpx;padding-left:0px;padding-right:0px;padding-top:0px;padding-bottom:0px\"></canvas>", ChartWidth, ChartHeight, ChartWidth, ChartHeight);
  WebClient.printf("<script>function MoreChartOptions(){} var ChartData = {labels : [%s", Labels.c_str());
  WebClient.printf(",datasets : \n[{type: \"%s\", fill:false, fillColor:\"rgba(255,0,0,1)\", strokeColor:\"rgba(59,105,78,0.73)\", pointColor:\"rgba(46,204,113,1)\", markerShape:\"circle\", ", Type1.c_str());
  WebClient.printf("pointStrokeColor:\"rgba(59,105,78,1)\", data:[%s,title:\"%s\"}\n", DataSet1.c_str(), Set1Title.c_str());

  switch(ChartType)
  {
    case HOURLY_TEMP:
    case DAILY_TEMP:
    case MONTHLY_TEMP:
    case COMPARE_TEMP:
    case COMPARE_COST:
    case COMPARE_KWH:
      WebClient.printf(",{type: \"%s\", fill:false, fillColor:\"rgba(46,204,113,1)\", strokeColor:\"rgba(10,61,31,0.47)\", pointColor:\"rgba(255,0,0,1)\", markerShape:\"circle\", ", Type2.c_str());
      WebClient.printf("pointStrokeColor:\"rgba(46,204,113,1)\", data:[%s,title:\"%s\"}", DataSet2.c_str(), Set2Title.c_str());
      break;
  }

  // Send the meat of the chart data
  WebClient.println("]};\nChartOptions = {decimalSeparator:\".\",thousandSeparator:\",\",spaceLeft:12,spaceRight:12,spaceTop:12,spaceBottom:12,");
  WebClient.println("scaleLabel:\"<%=value+''%>\",yAxisMinimumInterval:'none',scaleShowLabels:true,scaleShowLine:true,scaleLineStyle:\"solid\",scaleLineWidth:2,scaleLineColor:\"rgba(15,15,15,0.37)\",");
  WebClient.println("scaleOverlay :false,scaleOverride :false,scaleSteps:10,scaleStepWidth:10,scaleStartValue:0,inGraphDataShow:true,inGraphDataTmpl:'<%=v3%>',inGraphDataFontFamily:\"'Open Sans'\",");
  WebClient.println("inGraphDataFontStyle:\"normal bold\",inGraphDataFontColor:\"rgba(179,179,179,1)\",inGraphDataFontSize:10,inGraphDataPaddingX:0,inGraphDataPaddingY:5,inGraphDataAlign:\"center\",");
  WebClient.println("inGraphDataVAlign:\"bottom\",inGraphDataXPosition:2,inGraphDataYPosition:3,inGraphDataAnglePosition:2,inGraphDataRadiusPosition:2,inGraphDataRotate:0,inGraphDataPaddingAngle:0,");
  WebClient.println("inGraphDataPaddingRadius:0, inGraphDataBorders:false,inGraphDataBordersXSpace:1,inGraphDataBordersYSpace:1,inGraphDataBordersWidth:1,inGraphDataBordersStyle:\"solid\",");
  WebClient.printf("inGraphDataBordersColor:\"rgba(0,0,0,1)\",legend:%s,maxLegendCols:5,legendBlockSize:15,legendFillColor:'rgba(255,255,255,0.00)',legendColorIndicatorStrokeWidth:1,legendPosX:-2,", Legend.c_str());
  WebClient.println("legendPosY:4,legendXPadding:0,legendYPadding:0,legendBorders:false,legendBordersWidth:1,legendBordersStyle:\"solid\",legendBordersColors:\"rgba(102,102,102,1)\",");
  WebClient.println("legendBordersSpaceBefore:5,legendBordersSpaceLeft:5,legendBordersSpaceRight:5,legendBordersSpaceAfter:5,legendSpaceBeforeText:5,legendSpaceLeftText:5,legendSpaceRightText:5,");
  WebClient.println("legendSpaceAfterText:5,legendSpaceBetweenBoxAndText:5,legendSpaceBetweenTextHorizontal:5,legendSpaceBetweenTextVertical:5,legendFontFamily:\"'Indie Flower'\",");
  WebClient.println("legendFontStyle:\"normal normal\",legendFontColor:\"rgba(59,59,59,1)\",legendFontSize:21,yAxisFontFamily:\"'Indie Flower'\",yAxisFontStyle:\"normal bold\",");
  WebClient.printf("yAxisFontColor:\"rgba(46,204,113,1)\",yAxisFontSize:16,yAxisLabel : \"%s\",yAxisUnitFontFamily:\"'Indie Flower'\",yAxisUnitFontStyle:\"normal bold\",", LeftAxisLabel.c_str());
  WebClient.printf("yAxisUnitFontColor:\"rgba(255,0,0,1)\",yAxisUnitFontSize:16,yAxisUnit : \"%s\",showYAxisMin:false,rotateLabels:\"smart\",xAxisBottom:true,yAxisLeft:true,yAxisRight:false,", yAxisUnit.c_str());
  WebClient.println("graphTitleSpaceBefore:5,graphTitleSpaceAfter:-3, graphTitleBorders:false,graphTitleBordersXSpace:1,graphTitleBordersYSpace:1,graphTitleBordersWidth:1,graphTitleBordersStyle:\"solid\",");
  WebClient.printf("graphTitleBordersColor:\"rgba(0,0,0,1)\",graphTitle : \"%s\",graphTitleFontFamily:\"'Cabin'\",graphTitleFontStyle:\"normal lighter\",graphTitleFontColor:\"rgba(0,0,0,0.51)\",\n", Title.c_str());
  WebClient.println("graphTitleFontSize:28,graphSubTitleSpaceBefore:5,graphSubTitleSpaceAfter:5, graphSubTitleBorders:false,graphSubTitleBordersXSpace:1,graphSubTitleBordersYSpace:1,");
  WebClient.printf("graphSubTitleBordersWidth:1,graphSubTitleBordersStyle:\"solid\",graphSubTitleBordersColor:\"rgba(0,0,0,1)\",graphSubTitle : \"%s\",graphSubTitleFontFamily:\"'Open Sans'\",\n", SubTitle.c_str());
  WebClient.println("graphSubTitleFontStyle:\"normal normal\",graphSubTitleFontColor:\"rgba(102,102,102,1)\",graphSubTitleFontSize:16,footNoteSpaceBefore:5,footNoteSpaceAfter:0,footNoteBorders:false,");
  WebClient.println("footNoteBordersXSpace:1,footNoteBordersYSpace:1,footNoteBordersWidth:1,footNoteBordersStyle:\"solid\",footNoteBordersColor:\"rgba(0,0,0,1)\",footNote:\"Chart by livegap.com\",");
  WebClient.println("footNoteFontFamily:\"'Open Sans'\",footNoteFontStyle:\"normal normal\",footNoteFontColor:\"rgba(102,102,102,1)\",footNoteFontSize:12,scaleFontFamily:\"'Cabin'\",");
  WebClient.println("scaleFontStyle:\"normal normal\",scaleFontColor:\"rgba(3,3,3,1)\",scaleFontSize:16,pointLabelFontFamily:\"'Open Sans'\",pointLabelFontStyle:\"normal normal\",");
  WebClient.println("pointLabelFontColor:\"rgba(102,102,102,1)\",pointLabelFontSize:12,angleShowLineOut:true,angleLineStyle:\"solid\",angleLineWidth:1,angleLineColor:\"rgba(0,0,0,0.1)\",");
  WebClient.println("percentageInnerCutout:50,scaleShowGridLines:true,scaleGridLineStyle:\"solid\",scaleGridLineWidth:1,scaleGridLineColor:\"rgba(140,145,6,0.13)\",scaleXGridLinesStep:1,");
  WebClient.println("scaleYGridLinesStep:0,segmentShowStroke:true,segmentStrokeStyle:\"solid\",segmentStrokeWidth:2,segmentStrokeColor:\"rgba(255,255,255,1.00)\",datasetStroke:true,datasetFill:false,");
  WebClient.println("datasetStrokeStyle:\"solid\",datasetStrokeWidth:3,bezierCurve:true,bezierCurveTension:0.4,pointDotStrokeStyle:\"solid\",pointDotStrokeWidth:2,pointDotRadius:5,pointDot:true,");
  WebClient.printf("scaleTickSizeBottom:5,scaleTickSizeTop:5,scaleTickSizeLeft:5,scaleTickSizeRight:5,graphMin:%s,graphMax:%s,barShowStroke:false,barBorderRadius:0,barStrokeStyle:\"solid\",\n", String(ScaleMin, 2).c_str(), String(ScaleMax, 2).c_str());
  WebClient.println("barStrokeWidth:1,barValueSpacing:15,barDatasetSpacing:0,scaleShowLabelBackdrop:true,scaleBackdropColor:'rgba(255,255,255,0.75)',scaleBackdropPaddingX:2,scaleBackdropPaddingY:2,");
  WebClient.println("animationEasing:'linear',animateRotate:true,animateScale:false,animationByDataset:true,animationLeftToRight:true,animationSteps:85,animation:true,");
  WebClient.println("onAnimationComplete:function(){ MoreChartOptions() }}; DrawTheChart(ChartData,ChartOptions,\"chart-01\",\"Bar\");</script></center>");  
}

//
// Send schedule
//
void  SendSchedule(WiFiClient WebClient, bool isValid)
{
  int     i, j, x;
  bool    today;
  String  Title;
    
  Title = String(Location[Config.DeviceLocation]);
  if(strlen(Config.CommonName) != 0) Title += " (" + String(Config.CommonName) + ")";
  Title += " Thermostat";
  WebClient.printf("<center><h2><font color=\"#FF0000\">%s<font color=\"#000000\"></h2>", Title.c_str());
  WebClient.println("<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/\">");
  WebClient.println("<table border=\"1\"><tr><td></td>");
  for(i=0; i<7; i++)
  {
    today = (weekday() == i+1);
    WebClient.printf("<td align=\"center\" colspan=2><font color=\"%s\">%s%s&nbsp%s%s<font color=\"#000000\"></td>", 
      (today ? "#0000FF" : "#000000"), (today ? "<b>" : ""), dayStr(i+1), (today ? getTime(true) : ""), (today ? "</b>" : ""));
  }
  WebClient.println("</tr><td></td>");
  for(i=0; i<7; i++) WebClient.print("<td align=\"center\">Time</td><td align=\"center\">Temp</td>");
  WebClient.println("</tr>");
  for(j=0; j<4; j++)
  {
    WebClient.printf("<tr><td align=\"right\">Set %d</td>", j+1);
    for(i=0; i<7; i++) 
    {
      WebClient.printf("<td align=\"left\"><select name=\"HOUR_%d.%d\">", j, i);
      WebClient.printf("<option value=\"-1\" %s>--</option>", (!Schedule.Day[i].Valid[j] ? "selected" : ""));
      for(x=0; x<24; x++) WebClient.printf("<option value=\"%d\"%s>%02d</option>", x, ((Schedule.Day[i].Valid[j] && Schedule.Day[i].SetHour[j] == x) ? " selected" : ""), x);
      WebClient.printf("</select>&nbsp:&nbsp<select name=\"MINUTE_%d.%d\">", j, i);
      WebClient.printf("<option value=\"-1\"%s>--</option>", (!Schedule.Day[i].Valid[j] ? " selected" : ""));
      WebClient.printf("<option value=\"0\"%s>00</option>", ((Schedule.Day[i].Valid[j] && Schedule.Day[i].SetMinute[j] == 0) ? " selected" : ""));
      WebClient.printf("<option value=\"15\"%s>15</option>", ((Schedule.Day[i].Valid[j] && Schedule.Day[i].SetMinute[j] == 15) ? " selected" : ""));
      WebClient.printf("<option value=\"30\"%s>30</option>", ((Schedule.Day[i].Valid[j] && Schedule.Day[i].SetMinute[j] == 30) ? " selected" : ""));
      WebClient.printf("<option value=\"45\"%s>45</option></select></td>", ((Schedule.Day[i].Valid[j] && Schedule.Day[i].SetMinute[j] == 45) ? " selected" : ""));
      WebClient.printf("<td align=\"left\"><input type=\"number\" name=\"TEMP_%d.%d\" min=\"%d\" max=\"%d\" step=\"0.5\" value=\"%s\"></td>", 
        j, i, (Config.Celcius ? MinCelcius : MinFarenheit), (Config.Celcius ? MaxCelcius : MaxFarenheit), (Schedule.Day[i].Valid[j] ? String(Schedule.Day[i].SetTemp[j], 1).c_str() : ""));
    }
    WebClient.println("</tr>");
  }
  WebClient.printf("</table><br>Enable Schedule&nbsp<input type=\"checkbox\" name=\"ENABLED\"%s>&nbsp&nbspCopy Monday to all weekdays&nbsp<input type=\"checkbox\" name=\"MONDAY\">", (Schedule.Enabled ? " checked" : ""));
  WebClient.println("&nbsp&nbspCopy schedule to all thermostats&nbsp<input type=\"checkbox\" name=\"COPY\">");
  WebClient.printf("<br><br><input type=\"submit\"></form>%s</center>", (isValid ? "<br><br>Schedule Updated!" : ""));
}

// 
// Find requested temperature from schedule
//
float ScheduledTemp(float Default)
{
  float temp = Default;
  unsigned long Time1, Time2;

  // Set Time1 to current time
  Time1 = ((weekday()-1) * OneDay) + (hour() * OneHour) + (minute() * OneMinute);
  for(int i=0; i<weekday(); i++)
  {
    for(int j=0; j<4; j++)
    {
      // Set Time2 to requested time
      if(Schedule.Day[i].Valid[j]) 
      {
        Time2 = (i * OneDay) + (Schedule.Day[i].SetHour[j] * OneHour) + (Schedule.Day[i].SetMinute[j] * OneMinute); 
        if(Time1 >= Time2) temp = Schedule.Day[i].SetTemp[j];
      }
    }
  }

  // Return temperature
  return(temp);
}

//
// Broadcast schedule to all thermostats in local network
//
void  BroadcastSchedule(WiFiClient WebClient, String Line)
{
  int     httpCode, Cntr, i;
  String  postData, response;

  // Update the mDNS list
  Cntr = 0;
  findService(Location[Config.DeviceLocation], &Cntr);          

  // If no mDNS services were found, just exit
  if(MDNS_Services == 0 || Cntr == 0) 
  {
    Serial.printf("[%s] No thermostats to broadcast too!\n", getTime(false));
    WebClient.println("<center><font color=\"#FF0000\">No other thermostats found!<font color=\"#000000\"></center>");
    return;
  }
  
  // Find the thermostats and post the schedule to them
  for(i=Cntr=0; i<MDNS_Services; i++)
  {
    if(MDNS.hostname(i).indexOf("Thermo-") != -1)
    {
      // Got one, post schedule
      // TODO: Don't post to locations that are not in HOME location
      Serial.printf("[%s] Broadcasting to \"%s\"... ", getTime(false), MDNS.hostname(i).c_str());
      WebClient.printf("<center>Broadcasting to \"%s\"...&nbsp&nbsp", MDNS.hostname(i).c_str());
      postData = "http://" + MDNS.IP(i).toString() + "/Schedule";
      http.setReuse(false);
      http.begin(postData);
      
      // Remove the "&COPY=on" portion so that the other thermostats won't try to broadcast as well.
      httpCode = http.POST(Line.substring(0, Line.indexOf("&COPY")));
      if(httpCode == HTTP_CODE_OK) 
      {
        Serial.println("OK!");
        WebClient.print("OK!</center>");
        ++Cntr;
      }
      else
      {
        Serial.printf("Failed! (%s)\n", http.errorToString(httpCode).c_str());
        WebClient.printf("Failed! (%s)</center>", http.errorToString(httpCode).c_str());
      }
      
      // Close connection
      http.end();
    }
  }
}

//
// END OF CODE
//

