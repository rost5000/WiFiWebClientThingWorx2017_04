
/*
  Web client by ThingWorx
 
 */


#include <SPI.h>
#include <WiFi.h>
#include <Servo.h> 
#include "rgb_lcd.h";
#include <ArduinoJson.h>


WiFiClient client;
char ssid[] = "rost5000"; //  your network SSID (name) 
char pass[] = "664998196";    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;            // your network key Index number (needed only for WEP)
int status = WL_IDLE_STATUS;

// Параметры IoT сервера
char iot_server[] = "ptc.k36.org";
IPAddress iot_address(52, 3, 52, 231);
char appKey[] = "2ab91b18-2bd2-4ddf-b4e5-e7e25749274e";
char thingName[] = "Test_things_rost5000";
char serviceName[] = "MyTestService";

// Параметры сенсоров для IoT сервера
#define sensorCount 4                                     // How many values you will be pushing to ThingWorx
char* sensorNames[] = {"in1", "temperature", "noise", "activeWork"};
float sensorValues[sensorCount];

// Частота опроса датчиков
#define SENS_UPDATE_TIME 50
// Частота вывода данных на сервер IoT
#define IOT_UPDATE_TIME 50

#define PRINT_DELTATIME 5000

long prevTimeShowLCD = 0;
int currentID = 0;

// Таймер обновления опроса датчиков
long timer_sens = 0;

// Таймер обновления вывода на сервер IoT
long timer_iot = 0;

// Таймер ожидания прихода символов с сервера
long timer_iot_timeout = 0;

// Максимальное время ожидания ответа от сервера
#define IOT_TIMEOUT1 500
#define IOT_TIMEOUT2 100

// Размер приемного буффера
#define BUFF_LENGTH 64

// Приемный буфер
char buff[BUFF_LENGTH] = "";

// Тестовое состояние счетчика из ThingWorx
float test_state = 0 ;

// Переменные температуры
const int B=4275; 
#define LIGHT A0 //A0
#define TEMP A1  //A1
#define NOISE A2 //A2
#define SERVO 5  //D5
#define STATUS 2 //D2
#define LAMP_LIGHT 3 //D3
#define ERROR_LIGHT 4 //D4

//#define MICR A2  //A2
//#define BUTTON 2 //D2
//#define LED_1 3  //D3
//#define LED_2 4  //D4


bool statusWorkBreanch = false;
bool statusLampLight = false;
bool statusLampError = false;
bool isLampErrorOn = false;

rgb_lcd*lcd = new rgb_lcd();
// Сервопривод
Servo myservo;  // create servo object to control a servo 
                // a maximum of eight servo objects can be created 
 
//int serv_pos = 0;    // variable to store the servo position 

const int maxRotadeServo = 180;
const int minRotadeServo = 0;
int currentServo = 0;
int readedServo = 0;

void check_servo() {
  if(readedServo == currentServo)
    return;
  if(readedServo > maxRotadeServo || readedServo < minRotadeServo){
    Serial.print("Incorrect variable Servo max Value: ");
    Serial.print(maxRotadeServo);
    Serial.print(" min value: ");
    Serial.println(minRotadeServo);
    Serial.print("Readed: ");
    Serial.println(readedServo);
    return;
  }
  currentServo = readedServo;
  myservo.write(currentServo);
}
void setup() {
  //Initialize serial and wait for port to open:
  pinMode(LAMP_LIGHT, OUTPUT);
  pinMode(ERROR_LIGHT, OUTPUT);
  Serial.begin(9600); 
  lcd->begin(16, 2);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  
  // Проверка работоспособности сервопривода
  myservo.attach(SERVO); 
  check_servo();

  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present"); 
    // don't continue:
    while(true);
  } 

  String fv = WiFi.firmwareVersion();
  if( fv != "1.1.0" )
    Serial.println("Please upgrade the firmware");
  
  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) { 
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:    
    status = WiFi.begin(ssid, pass);
  
    // wait 10 seconds for connection:
    delay(1000);
  } 
  Serial.println("Connected to wifi");
  printWifiStatus();

  for(int i = 0; i < sensorCount; i++)
    sensorValues[i] = 0;
}

void loop() {
  // Опрос датчиков
  if (millis() > timer_sens + SENS_UPDATE_TIME)
  {
    // Опрос датчиков
    readSensor();
    // Сброс таймера
    timer_sens = millis();
  }
    
  // Вывод данных на сервер IoT
  if (millis() > timer_iot + IOT_UPDATE_TIME)
  {
    // Вывод данных на сервер IoT
    sendDataIot();
    // Сброс таймера
    check_servo();
    timer_iot = millis();

   //printStatusLCD();
  }

  if(millis() > prevTimeShowLCD + PRINT_DELTATIME){
      printStatusLCD(sensorValues[currentID], sensorNames[currentID]);
      currentID = (currentID + 1) % sensorCount;
    }
}


void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

// Чтение датчиков температуры и освещения
void readSensor()
{
  //Чтение датчика освещенности
  sensorValues[0] = analogRead(LIGHT);
  
  //sensorValues[0] = sensorValues[0]+ 1;
  //sensorValues[1] = sensorValues[1]+ 1;
  
  //Чтение датчика температуры
  int a = analogRead(TEMP);
  
  float R = 1023.0/((float)a)-1.0;
  R = 100000.0*R;
  float temperature=1.0/(log(R/100000.0)/B+1/298.15)-273.15;//convert to temperature via datasheet ;

  Serial.print("temperature = ");
  Serial.print(temperature);
  sensorValues[1] = temperature; 
  Serial.print(", active: ");  
  Serial.print(statusWorkBreanch = digitalRead(STATUS) == HIGH? !statusWorkBreanch: statusWorkBreanch);
  sensorValues[3] = statusWorkBreanch? 1: 0;
  Serial.print(", NOISE: ");
  Serial.print(sensorValues[2] = analogRead(NOISE));

  if(isLampErrorOn)digitalWrite(ERROR_LIGHT, statusLampError? HIGH: LOW);
  else digitalWrite(ERROR_LIGHT, LOW);
  isLampErrorOn = !isLampErrorOn;
  
  digitalWrite(LAMP_LIGHT, statusLampLight? HIGH: LOW);
  

  Serial.println();
}



// Подключение к серверу IoT ThingWorx
void sendDataIot()
{
  // Подключение к серверу
  Serial.println("Connecting to IoT server...");
  if (client.connect(iot_address, 80))
  {
    // Проверка установления соединения
    if (client.connected())
    {
      // Отправка заголовка сетевого пакета
      Serial.println("Sending data to IoT server...\n");
      Serial.print("POST /Thingworx/Things/");
      client.print("POST /Thingworx/Things/");
      Serial.print(thingName);
      client.print(thingName);
      Serial.print("/Services/");
      client.print("/Services/");
      Serial.print(serviceName);
      client.print(serviceName);
      Serial.print("?appKey=");
      client.print("?appKey=");
      Serial.print(appKey);
      client.print(appKey);
      Serial.print("&method=post&x-thingworx-session=true");
      client.print("&method=post&x-thingworx-session=true");
      // Отправка данных с датчиков
      for (int idx = 0; idx < sensorCount; idx ++)
      {
        Serial.print("&");
        client.print("&");
        Serial.print(sensorNames[idx]);
        client.print(sensorNames[idx]);
        Serial.print("=");
        client.print("=");
        Serial.print(sensorValues[idx]);
        client.print(sensorValues[idx]);
      }
      // Закрываем пакет
      Serial.println(" HTTP/1.1");
      client.println(" HTTP/1.1");
      Serial.println("Accept: text/html");
      client.println("Accept: text/html");
      Serial.print("Host: ");
      client.print("Host: ");
      Serial.println(iot_server);
      client.println(iot_server);
      Serial.println("Content-Type: application/json");
      client.println("Content-Type: application/json");
      
      Serial.println();
      client.println();

      // Ждем ответа от сервера
      timer_iot_timeout = millis();
      while ((client.available() == 0) && (millis() < timer_iot_timeout + IOT_TIMEOUT1));

      // Выводим ответ о сервера, и, если медленное соединение, ждем выход по таймауту
      int iii = 0;
      bool currentLineIsBlank = true;
      bool flagJSON = false;
      timer_iot_timeout = millis();
      while ((millis() < timer_iot_timeout + IOT_TIMEOUT2) && (client.connected()))
      {
        while (client.available() > 0)
        {
          char symb = client.read();
          Serial.print(symb);
          if (symb == '{')
          {
            flagJSON = true;
          }
          else if (symb == '}')
          {
            flagJSON = false;
          }
          if (flagJSON == true)
          {
            buff[iii] = symb;
            iii ++;
          }
          timer_iot_timeout = millis();
        }
      }
      buff[iii] = '}';
      buff[iii + 1] = '\0';
      Serial.println(buff);
      client.println("Connection: close");
      client.stop();

      
      // Расшифровываем параметры
      StaticJsonBuffer<BUFF_LENGTH> jsonBuffer;
      JsonObject& json_array = jsonBuffer.parseObject(buff);
      test_state = json_array["test_state"];
      readedServo = json_array["manageServo"];
      Serial.print("manageServo:   ");
      Serial.print(readedServo);
      Serial.print(" Light: ");
      Serial.print(statusLampLight = json_array["lampLight"] == 1);
      Serial.print(" Error: ");
      Serial.print(statusLampError = json_array["errorLamp"] == 1);
      Serial.println();
      

      // Делаем управление устройствами
      /*controlDevices();

      */
      Serial.println("Packet successfully sent!");
      Serial.println();
    }
  }
}

void printStatusLCD(float value, char*valueName){
    if(!statusWorkBreanch){
      lcd->setRGB(0,255,0);
      lcd->noDisplay();
      return;
    }
     else{
      
      if(sensorValues[2]>200 && sensorValues[2]<799)
      {
         lcd->setRGB(255,104,0);
      }
        else
         if(sensorValues[2]>800)
      {
         lcd->setRGB(255,0,5);
       }
        else 
        {
          lcd->setRGB(255,255,255);
        }
      lcd->display();
      lcd->clear();
      lcd->print(valueName);
      lcd->setCursor(1, 1);
      lcd->print(value);
     }
  }


