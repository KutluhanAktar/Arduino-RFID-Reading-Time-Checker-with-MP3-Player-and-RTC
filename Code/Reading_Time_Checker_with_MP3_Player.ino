         ////////////////////////////////////////////////////  
        //       RFID Book Reading Time Checker with      //
       //               MP3 Player and RTC               //
      //           -------------------------            //
     //                 Arduino Nano                   //           
    //               by Kutluhan Aktar                // 
   //                                                //
  ////////////////////////////////////////////////////

//
//
//
//
// Connections
// Arduino Nano v3:           
//                                Serial MP3 Player Module (OPEN-SMART)
// D8 --------------------------- TX
// D7 --------------------------- RX
//                                Nokia 5110 Screen
// D2 --------------------------- SCK (Clk)
// D3 --------------------------- MOSI (Din) 
// D4 --------------------------- DC 
// D5 --------------------------- RST
// D6 --------------------------- CS (CE)
//                                DS3231
// SDA(or A4) ------------------- SDA  
// SCL(or A5) ------------------- SCL
//                                MFRC522
// D9  -------------------------- RST
// D10 -------------------------- SDA
// D11 -------------------------- MOSI
// D12 -------------------------- MISO
// D13 -------------------------- SCK
//                                LEFT_BUTTON
// A0 --------------------------- S
//                                OK_BUTTON
// A1 --------------------------- S
//                                RIGHT_BUTTON
// A2 --------------------------- S
//                                EXIT_BUTTON
// A3 --------------------------- S



// Include required libraries:
#include <SoftwareSerial.h>
#include <LCD5110_Basic.h>
#include <DS3231.h>
#include <EEPROM.h>     
#include <SPI.h>        
#include <MFRC522.h>

// Define the RX and TX pins to establish UART communication with the MP3 Player Module.
#define MP3_RX 8 // to TX
#define MP3_TX 7 // to RX

// Define the required MP3 Player Commands.
// You can inspect all given commands from the project page: 
// Select storage device to TF card
static int8_t select_SD_card[] = {0x7e, 0x03, 0X35, 0x01, 0xef}; // 7E 03 35 01 EF
// Play the song with the directory: /01/001xxx.mp3
static int8_t play_first_song[] = {0x7e, 0x04, 0x41, 0x00, 0x01, 0xef}; // 7E 04 41 00 01 EF
// Play the song with the directory: /01/002xxx.mp3
static int8_t play_second_song[] = {0x7e, 0x04, 0x41, 0x00, 0x02, 0xef}; // 7E 04 41 00 02 EF
// Play the song.
static int8_t play[] = {0x7e, 0x02, 0x01, 0xef}; // 7E 02 01 EF
// Pause the song.
static int8_t pause[] = {0x7e, 0x02, 0x02, 0xef}; // 7E 02 02 EF

// Define the Serial MP3 Player Module.
SoftwareSerial MP3(MP3_RX, MP3_TX);

// Define the Nokia screen.
LCD5110 myGLCD(2,3,4,5,6);

extern uint8_t SmallFont[];
extern uint8_t MediumNumbers[];
extern uint8_t BigNumbers[];
// Define images for the graphics screen.
extern uint8_t clock[];
extern uint8_t spidey[];

// Initiate the DS3231 RTC Module using SDA and SCL pins.
DS3231  rtc(SDA, SCL);

// Define a time variable to get data from DS3231 properly.
Time t;

// Create MFRC522 instance.
#define SS_PIN 10
#define RST_PIN 9

MFRC522 mfrc522(SS_PIN, RST_PIN);

// Define MFRC5222 module key input.
MFRC522::MIFARE_Key key;

// Stores scanned ID read from the RFID Module.
byte readCard[4];
// Define registeredUID and lastRead strings.
String registeredUID, lastRead;

// Define the control buttons.
#define B_Exit A3
#define B_Right A2
#define B_OK A1
#define B_Left A0

// Define the data holders.
int Right, OK, Left, Exit;
int alarmHour =0;
int alarmMin = 0;
int selected = 0;

// Define the interface options:
volatile boolean Init_Checker = false;
volatile boolean Set_Checker = false;
volatile boolean Clock = false;
volatile boolean New_Card = false;
volatile boolean Activated = false;
volatile boolean Alarm_Mode = false;

void setup() {
  pinMode(B_Exit, INPUT);
  pinMode(B_Right, INPUT);
  pinMode(B_OK, INPUT);
  pinMode(B_Left, INPUT);
  
  // Initiate the serial moinitor.
  Serial.begin(9600);
  // Initiate the Serial MP3 Player Module.
  MP3.begin(9600);
  // Select the SD Card.
  send_command_to_MP3_player(select_SD_card, 5);
  // Start the rtc object.
  rtc.begin();
  // MFRC522 Hardware uses SPI protocol
  SPI.begin();
  // Initialize MFRC522 Hardware           
  mfrc522.PCD_Init();    
  // Initiate the Nokia screen.
  myGLCD.InitLCD();
  myGLCD.setFont(SmallFont);
  myGLCD.print("Initializing...", 0, 8);
  delay(1000);
  myGLCD.clrScr();

}

void loop() {
  read_control_buttons();

  select_menu_commands();
  
  home_screen();

  // Adjust the interface view depending on the selected option.
  if(Init_Checker == true){
    do{
      myGLCD.invertText(true);
      myGLCD.print("A.Init Checker", 0, 16);
      myGLCD.invertText(false);
      // If selected, initiate the alarm mode and activate the alarm when the registered UID is detected by the RFID Module (MFRC522).
      if(OK == true){
        myGLCD.clrScr();
        Activated = true;
        myGLCD.print("Approximate", 0, 0);
        myGLCD.print("the registered", 0, 8);
        myGLCD.print("card or key", 0, 16);
        myGLCD.print("to activate", 0, 24);
        myGLCD.print("the alarm", 0, 32);
        myGLCD.print("mode :)", 0, 40);
        // Get the registered UID in the EEPROM.
        get_the_registered_UID();
        while(Activated == true){
          // Read control variables.
          read_control_buttons();
          // Get the last detected UID from MFRC522.
          get_the_last_detected_UID();
          // If the detected UID corresponds to the registered UID in the EEPROM, then activate the alarm mode.
          if(lastRead == registeredUID){
            myGLCD.clrScr();
            Alarm_Mode = true;
            // ---------- Alarm Mode -------------
            if(Alarm_Mode == true){
              // Draw the alarm mode icon.
              myGLCD.drawBitmap(22, 0, clock, 40, 40);
              boolean song = false;
              while(Alarm_Mode == true){
                // Read control variables.
                read_control_buttons();
                // Get the date and time information from the DS3231 RTC Module.
                get_time_rtc();
                // If the RTC Module hits the given reading alarm time, then start the MP3 Player.
                if(t.hour == alarmHour && t.min == alarmMin && song == false){
                  // Send commands to the MP3 Player Module.
                  send_command_to_MP3_player(play_second_song, 6);
                  // Draw the notification icon - I used a Spider-Man logo because the MP3 Player plays his cartoon's sountrack :)
                  myGLCD.clrScr();
                  myGLCD.drawBitmap(22, 0, spidey, 40, 40);
                  song = true;
                }  
                // MP3 Player Settings:
                // Play or pause selected songs for fun while the module is in the alarm mode :)
                if(Right == true) send_command_to_MP3_player(play_first_song, 6);
                if(Left == true) send_command_to_MP3_player(pause, 4);
                if(OK == true) send_command_to_MP3_player(play, 4);
                // Exit.
                if(Exit == true){ Alarm_Mode = false; myGLCD.clrScr(); lastRead = ""; registeredUID = "";}
              }
            }
            // -----------------------------------
          }else{
            // Blank the lastRead string after false readings.
            lastRead = "";
          }
          // Exit.
          if(Exit == true){ Activated = false; myGLCD.clrScr(); registeredUID = "";}
        }
      }
    }while(Init_Checker == false);
  }
  if(Set_Checker == true){
    do{
      myGLCD.invertText(true);
      myGLCD.print("B.Set Checker", 0, 24);
      myGLCD.invertText(false);
      // If selected, set the reading alarm to get notified when the RTC hits the given time (such as 18 : 37).
      if(OK == true){
        myGLCD.clrScr();
        Activated = true;
        while(Activated == true){
          // Read control variables.
          read_control_buttons();
          // Display the reading alarm.
          myGLCD.setFont(BigNumbers);
          myGLCD.printNumI(alarmHour, LEFT, 0);
          myGLCD.printNumI(alarmMin, RIGHT, 0);
          myGLCD.setFont(SmallFont);
          myGLCD.print(".", CENTER, 0);
          myGLCD.print(".", CENTER, 16);
          myGLCD.print("DEC< Set >INC", CENTER, 40);
          // Set the reading alarm.
          if(Right == true) alarmMin++;
          if(Left == true) alarmMin--;
          if(alarmMin < 0){ alarmMin = 59; alarmHour--; myGLCD.clrScr(); }
          if(alarmMin > 59){ alarmMin = 0; alarmHour++; myGLCD.clrScr(); }
          if(alarmHour < 0) alarmHour = 23;
          if(alarmHour > 23) alarmHour = 0;
          delay(100);
          // Exit.
          if(Exit == true){ Activated = false; myGLCD.clrScr(); }
        }
      }
    }while(Set_Checker == false);
  }
  if(Clock == true){
    do{
      myGLCD.invertText(true);
      myGLCD.print("C.Clock", 0, 32);
      myGLCD.invertText(false);
      // If selected, display date and time on the screen.
      if(OK == true){
        myGLCD.clrScr();
        Activated = true;
        while(Activated == true){
          // Read control variables.
          read_control_buttons();
          // Get the date and time information from the DS3231 RTC Module.
          get_time_rtc();
          myGLCD.setFont(BigNumbers);
          myGLCD.printNumI(t.hour, LEFT, 0);
          myGLCD.printNumI(t.min, RIGHT, 0);
          myGLCD.setFont(SmallFont);
          myGLCD.print(".", CENTER, 0);
          myGLCD.print(".", CENTER, 16);
          myGLCD.print(String(rtc.getTemp()) + "'C", CENTER, 32);
          String d = String(t.date) + "/" + rtc.getMonthStr() + "/" + String(t.year);
          myGLCD.print(d, CENTER, 40);
          // Exit.
          if(Exit == true){ Activated = false; myGLCD.clrScr(); }
        }
      }
    }while(Clock == false);
  }
  if(New_Card == true){
    do{
      myGLCD.invertText(true);
      myGLCD.print("D.New Card", 0, 40);
      myGLCD.invertText(false);
      // If selected, approximate a new card or key tag to scan and save the new UID to EEPROM.
      if(OK == true){
        myGLCD.clrScr();
        Activated = true;
        myGLCD.print("Approximate", 0, 0);
        myGLCD.print("a new card or", 0, 8);
        myGLCD.print("key tag to", 0, 16);
        myGLCD.print("scan and save", 0, 24);
        myGLCD.print("the new UID to.", 0, 32);
        myGLCD.print("EEPROM :)", 0, 40);
        while(Activated == true){
          // Read control variables.
          read_control_buttons();
          // Register a new UID (card or tag) to save it to the EEPROM.
          register_new_card();
          // Exit.
          if(Exit == true){ Activated = false; myGLCD.clrScr(); }
        }
      }
    }while(New_Card == false);
  }
}

void read_control_buttons(){
  // Read the control buttons:
  Right = digitalRead(B_Right);
  OK = digitalRead(B_OK);
  Left = digitalRead(B_Left);
  Exit = digitalRead(B_Exit);
    
}

void send_command_to_MP3_player(int8_t command[], int len){
  Serial.print("\nMP3 Command => ");
  for(int i=0;i<len;i++){ MP3.write(command[i]); Serial.print(command[i], HEX); }
  delay(1000);
}

void get_time_rtc(){
  //  The following lines can be uncommented to set the date and time manually.
  //  rtc.setDOW(WEDNESDAY);     // Set Day-of-Week to Wednesday
  //  rtc.setTime(18, 13, 15);     // Set the time to 12:00:00 (24hr format)
  //  rtc.setDate(17, 10, 2018);   // Set the date to October 17st, 2018

  // Get the current hour and minute from DS3231. // t.hour; and t.min;
  t = rtc.getTime();
}

void register_new_card(){
  // Detect the new card UID. 
  if ( ! mfrc522.PICC_IsNewCardPresent()) { return 0; }
  if ( ! mfrc522.PICC_ReadCardSerial()) { return 0; }

  // Display the new UID.
  for (int i = 0; i < mfrc522.uid.size; i++) { 
    readCard[i] = mfrc522.uid.uidByte[i];
  }
  
  // Save the new UID to EEPROM. 
  // If you want to save another card, use i+4(...) instead i.
  for (int i = 0; i < mfrc522.uid.size; i++){
    EEPROM.write(i, readCard[i]);
  }
 
  mfrc522.PICC_HaltA();
  // Return the success message.
  myGLCD.clrScr();
  myGLCD.print("Registered", 0, 8); 
  myGLCD.print("Successfully!", 0, 24);
  
}

void get_the_registered_UID(){
  // Get the registered UID from EEPROM.
  for(int i=0;i<4;i++){
    registeredUID += EEPROM.read(i) < 0x10 ? " 0" : " ";
    registeredUID += String(EEPROM.read(i), HEX);
  }
  // Arrange registeredUID for comparing.
  registeredUID.trim();
  registeredUID.toUpperCase();

  Serial.println("\nRegistered UID => " + registeredUID);
}

void get_the_last_detected_UID(){
  if ( ! mfrc522.PICC_IsNewCardPresent()){ return 0; }
  if ( ! mfrc522.PICC_ReadCardSerial()){ return 0; }
  for(int i=0;i<mfrc522.uid.size;i++){
    lastRead += mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ";
    lastRead += String(mfrc522.uid.uidByte[i], HEX);
   }
   // Arrange lastRead for comparing.
   lastRead.trim();
   lastRead.toUpperCase();

   Serial.println("\nLast Detected UID => " + lastRead);
}

void home_screen(){
  // Define options.
  myGLCD.print("Menu Commands:", 0, 0);
  myGLCD.print("A.Init Checker", 0, 16);
  myGLCD.print("B.Set Checker", 0, 24);
  myGLCD.print("C.Clock", 0, 32);
  myGLCD.print("D.New Card", 0, 40);
}

void select_menu_commands(){
  // Increase or decrease the option number using Right and Left buttons.
  if(Right == true) selected++;
  if(Left == true) selected--;
  if(selected < 0) selected = 4;
  if(selected > 4) selected = 1;
  delay(300);
  // Depending on the selected option number, change boolean status.
  switch(selected){
    case 1:
      Init_Checker = true;
      Set_Checker = false;
      Clock = false;
      New_Card = false;
    break;
    case 2:     
      Init_Checker = false;
      Set_Checker = true;
      Clock = false;
      New_Card = false;
    break;
    case 3:
      Init_Checker = false;
      Set_Checker = false;
      Clock = true;
      New_Card = false;
    break;
    case 4:
      Init_Checker = false;
      Set_Checker = false;
      Clock = false;
      New_Card = true;
    break;
  }
}
