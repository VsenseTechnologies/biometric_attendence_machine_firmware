#include<Arduino.h>
#include<SPIFFS.h>
#include<Vsense_Finger_Print.h>
#include<RTClib.h>
#include<WiFi.h>
#include<PubSubClient.h>
#include <ArduinoJson.h>
#include<BluetoothSerial.h>
#include<vector>
#include<arduino_base64.hpp>

//timeouts in seconds
#define WIFI_CONNECTION_TIME_OUT 10
#define BROKER_CONNECTION_TIME_OUT 10
#define BROKER_SIDE_PARSE_ERROR_TIME_OUT 20
#define BROKER_SIDE_DB_ERROR_TIME_OUT 20
#define INVALID_UNIT_ERROR_TIME_OUT 5
#define FINGER_PRINT_DELETE_ERROR_TIME_OUT 50
#define FINGER_PRINT_INSERT_ERROR_TIME_OUT 50
#define ATT_REQ_TIMEOUT 10

//wifi config
#define WIFI_CONFIG_FILE_PATH "/wifi_config.json"

//broker config
#define BROKER_HOST "biometric.mqtt.vsensetech.in"
#define BROKER_PORT 1883
#define BROKER_USER_NAME ""
#define BROKER_PASSWORD ""
#define KEEP_ALIVE_INTERVAL 5
#define UNIT_ID "vs24al002"

//finger print sensor config
#define FINGER_PRINT_SENSOR_COMMUNICATION_BAUDRATE 57600

//publish topics
#define UNIT_SUBSCRIBE_TOPIC "vs24al002"
#define CONNECTION_TOPIC UNIT_SUBSCRIBE_TOPIC "/connected"
#define DISCONNECTION_TOPIC UNIT_SUBSCRIBE_TOPIC "/disconnected"
#define DELETE_SYNC_TOPIC UNIT_SUBSCRIBE_TOPIC "/deletesync"
#define DELETE_SYNC_ACK_TOPIC UNIT_SUBSCRIBE_TOPIC "/deletesyncack"
#define INSERT_SYNC_TOPIC UNIT_SUBSCRIBE_TOPIC "/insertsync"
#define INSERT_SYNC_ACK_TOPIC UNIT_SUBSCRIBE_TOPIC "/insertsyncack"
#define ATTENDENCE_TOPIC UNIT_SUBSCRIBE_TOPIC "/attendence"

//gpio pins
#define BUZZER_PIN 4
#define GREEN_LED1_PIN 14
#define GREEN_LED2_PIN 27
#define RED_LED_PIN 26

//intializing the hardware serial communication (UART2)
HardwareSerial serial_port(2);

//initializing the finger print sensor by specifying UART2
Adafruit_Fingerprint finger=Adafruit_Fingerprint(&serial_port); 

//initializing the RTC module
RTC_DS3231 rtc;

//creating the finger print reading handler
TaskHandle_t finger_print_reading_handler;

//initializing the tcp client
WiFiClient tcp_client;

//initializing the mqtt client by specifying the custom tcp client
PubSubClient mqtt_client(tcp_client);

//initializing the json document by specifying the memory in bytes
DynamicJsonDocument json_doc(2048);

//initializing the bluetooth
BluetoothSerial SerialBlueTooth;

//input blue tooth message buffer
String input_bluetooth_message="";

//mqtt status
uint8_t is_broker_connected=0;
uint8_t is_deletes_synchronized=0;
uint8_t is_inserts_synchronized=0;
uint8_t is_delete_sync_msg_published=0;
uint8_t is_insert_sync_msg_published=0;

//bluetooth connection status
uint8_t is_bluetooth_connected=0;
//attedence_request_status
uint8_t attendence_request_status=0;

void turn_all_indicaters_off() {
  digitalWrite(GREEN_LED1_PIN,0);
  digitalWrite(GREEN_LED2_PIN,0);
  digitalWrite(RED_LED_PIN,0);
  digitalWrite(BUZZER_PIN,0);
}

void file_system_mount_fail_indicater() { 
  digitalWrite(BUZZER_PIN,1);
  delay(2000);
  digitalWrite(BUZZER_PIN,0);
  delay(2000);
}

void fingerprint_sensor_not_found_indicater(){
  digitalWrite(BUZZER_PIN,1);
  delay(1000);
  digitalWrite(BUZZER_PIN,0);
  delay(1000);
}

void rtc_module_not_found_indicater() {
  digitalWrite(BUZZER_PIN,1);
  delay(500);
  digitalWrite(BUZZER_PIN,0);
  delay(500);
}

void bluetooth_connection_indicater(){
  digitalWrite(GREEN_LED1_PIN,1);
  digitalWrite(GREEN_LED2_PIN,1);
  digitalWrite(RED_LED_PIN,1);
}

void bluetooth_disconnection_indicater() {
  digitalWrite(GREEN_LED1_PIN,0);
  digitalWrite(GREEN_LED2_PIN,0);
  digitalWrite(RED_LED_PIN,0);
}

void  wifi_config_file_read_error_indicater() {
    digitalWrite(BUZZER_PIN,1);
    delay(200);
    digitalWrite(BUZZER_PIN,0);
    delay(200);
}

void broker_connection_indicater() {
    digitalWrite(GREEN_LED1_PIN,1);
}

void broker_disconnection_indicater() {
  digitalWrite(GREEN_LED1_PIN,0);
}

void wifi_connection_indicater(){
  digitalWrite(GREEN_LED2_PIN,1);
}

void wifi_disconnection_indicater(){
  digitalWrite(GREEN_LED2_PIN,0);
}

void broker_json_message_parse_error_indicater(){
    digitalWrite(BUZZER_PIN,1);
    digitalWrite(RED_LED_PIN,1);
    delay(3000);
    digitalWrite(BUZZER_PIN,0);
    digitalWrite(RED_LED_PIN,0);
}

void invalid_unit_indicater(){
  digitalWrite(RED_LED_PIN,1);
  delay(1000);
  digitalWrite(RED_LED_PIN,0);
  delay(1000);
}

void broker_side_parse_error_indicater() {
  digitalWrite(RED_LED_PIN,1);
  delay(250);
  digitalWrite(RED_LED_PIN,0);
  delay(250);
}

void broker_side_database_error_indicater(){
  digitalWrite(RED_LED_PIN,1);
  delay(250);
  digitalWrite(RED_LED_PIN,0);
  delay(250);
}

void finger_print_delete_failure_indicater() {
  digitalWrite(RED_LED_PIN,1);
  delay(100);
  digitalWrite(RED_LED_PIN,0);
  delay(100);
}

void finger_print_insert_failure_indicater() {
  digitalWrite(RED_LED_PIN,1);
  digitalWrite(BUZZER_PIN,1);
  delay(100);
  digitalWrite(RED_LED_PIN,0);
  digitalWrite(BUZZER_PIN,0);
  delay(100);
}


void finger_print_sensor_error_indicater() {
  digitalWrite(BUZZER_PIN,1);
  delay(200);
  digitalWrite(BUZZER_PIN,0);
  delay(15);
  digitalWrite(BUZZER_PIN,1);
  delay(200);
  digitalWrite(BUZZER_PIN,0);
  delay(20);
  digitalWrite(BUZZER_PIN,1);
  delay(200);
  digitalWrite(BUZZER_PIN,0);
  delay(15);
}

void attendence_success_indicater() {
  digitalWrite(GREEN_LED2_PIN,1);
  digitalWrite(BUZZER_PIN,1);
  delay(1000);
  digitalWrite(GREEN_LED2_PIN,0);
  digitalWrite(BUZZER_PIN,0);
}

void attendence_error_indicater() {
  digitalWrite(RED_LED_PIN,1);
  digitalWrite(BUZZER_PIN,1);
  delay(200);
  digitalWrite(BUZZER_PIN,0);
  delay(15);
  digitalWrite(BUZZER_PIN,1);
  delay(200);
  digitalWrite(BUZZER_PIN,0);
  delay(20);
  digitalWrite(BUZZER_PIN,1);
  delay(200);
  digitalWrite(BUZZER_PIN,0);
  delay(15);
  digitalWrite(RED_LED_PIN,0);
}

void attendence_request_timeout_indicater() {
  digitalWrite(RED_LED_PIN,1);
  delay(1000);
  digitalWrite(RED_LED_PIN,0);
}

void reset_all_online_status() {
  is_broker_connected=0;
  is_deletes_synchronized=0;
  is_inserts_synchronized=0;
  is_delete_sync_msg_published=0;
  is_insert_sync_msg_published=0;
  attendence_request_status=0;
}

bool writeFile(const char *path,const char *message){
  //opening the file 
  File file =SPIFFS.open(path,FILE_WRITE);

  //checking if the file opened or not
  if(!file) {
    Serial.println("failed to open file");
    return false;
  }

  //writing the message to file
  if(!file.print(message)) {
    //closing the file
    file.close();
    return false;
  }

  //closing the file
  file.close();
  return true;
} 


String readFile(const char *path){
  //opening the file from file system
  File file=SPIFFS.open(path);
  //checking file is opened or not
  if(!file){
    Serial.println("failed to open file");
    return "";
  }
  String data="";
  //checking for the file content
  while(file.available()){
    //pushing the file content to data
    data+=char(file.read());
  }

  //closing the file
  file.close();
  //returning the red data
  return data;
}

void send_bluetooth_response(const char *json) {
  for(uint8_t i=0;json[i];i++) {
    SerialBlueTooth.write(json[i]);
  }
}

void wifi_config_handler(String &input_json_message){

  //getting the user id from the file system
  String file_system_json_data=readFile(WIFI_CONFIG_FILE_PATH);

  //checking if file system data was empty or not
  if(file_system_json_data == "") {

    const char *json_response="{\"est\":\"1\",\"ety\":\"1\"}";

    //sending the bluetooth response
    send_bluetooth_response(json_response);

    //clearing the input bluetooth message
    input_bluetooth_message.clear();

    //closing the bluetooth connection
    SerialBlueTooth.disconnect();

    return;
  }

  //clearing the prev json buffer
  json_doc.clear();

  //decoding the file system json data
  DeserializationError error1=deserializeJson(json_doc,file_system_json_data.c_str());

  //checking for the json decode error
  if(error1) {
    const char *json_response="{\"est\":\"1\",\"ety\":\"\2\"}";

    //sending the bluetooth response
    send_bluetooth_response(json_response);

    //clearing the input bluetooth message
    input_bluetooth_message.clear();

    //closing the bluetooth connection
    SerialBlueTooth.disconnect();

    return;
  }
  
  //getting thr user id from file system json data
  String stored_user_id=json_doc["uid"];

  //clearing the prev json buffer
  json_doc.clear();

  //decoding the input bluetooth json message
  DeserializationError error2=deserializeJson(json_doc,input_json_message.c_str());

  //checking for json decode error
  if(error2) {
 
    const char *json_response="{\"est\":\"1\",\"ety\":\"3\"}";
    
    //sending the json response
    send_bluetooth_response(json_response);

    //clearing the input bluetooth message
    input_bluetooth_message.clear();

    //closing the bluetooth connection
    SerialBlueTooth.disconnect();

    return;
  }

  //getting the user id from the input bluetooth json message
  String input_user_id=json_doc["uid"];

  //checking for the input user id and file system user id
  if(stored_user_id != input_user_id) {

    const char *json_response="{\"est\":\"1\",\"ety\":\"4\"}";

    //sending the bluetooth response
    send_bluetooth_response(json_response);

    //clearing the input bluetooth message
    input_bluetooth_message.clear();

    //closing the bluetooth connection
    SerialBlueTooth.disconnect();

    return;
  }

  //writing the new configuration to the file system
  bool file_write_status=writeFile(WIFI_CONFIG_FILE_PATH,input_json_message.c_str());

  //checking for the file write status
  if(!file_write_status) {

    const char *json_response = "{\"est\":\"1\",\"ety\":\"5\"}";

    //sending the json response
    send_bluetooth_response(json_response);
    
    //clearing the input bluetooth message
    input_bluetooth_message.clear();

    //closing the bluetooth connection
    SerialBlueTooth.disconnect();

    return;
  }

  const char *json_response="{\"est\":\"0\"}";

  //sending the json response
  send_bluetooth_response(json_response);
  //clearing the input bluetooth message
  input_bluetooth_message.clear();

  //closing the bluetooth connection
  SerialBlueTooth.disconnect();
}


void connect_to_wifi() {
  //setting the timeout counter
  uint8_t timeout_counter=WIFI_CONNECTION_TIME_OUT;
  //disconnecting from the prev wifi
  WiFi.disconnect();
  //indicating wifi disconnection
  wifi_disconnection_indicater();
  //loading the wifi ssid and password from file system
  String json_data=readFile(WIFI_CONFIG_FILE_PATH);

  if(json_data == "") {
    Serial.println("failed to read the wifi_config.json file");
    while(1) {
       wifi_config_file_read_error_indicater();
    }
  }

  //cleairng the prev json buffer
  json_doc.clear();
  //decoding the json data
  DeserializationError error=deserializeJson(json_doc,json_data.c_str());
  //checking for the json decode error
  if(error) {
    Serial.println("json decode error");
    return;
  }

  //getting the wifi ssid and password
  const char *ssid=json_doc["ssid"];
  const char *password=json_doc["pwd"];

  //connectig to the wifi
  WiFi.begin(ssid,password);

  //checking for wifi connection status
  while(WiFi.status()!=WL_CONNECTED && timeout_counter > 0) {

    //checking for the bluetooth connection
    if(SerialBlueTooth.connected()) {
      return;
    }

    Serial.println("connecting to wifi -> "+String(timeout_counter));
    vTaskDelay(pdMS_TO_TICKS(1000));
    timeout_counter--;
  }

}

void connect_to_broker() {
  //setting the time out counter
  uint8_t timeout_counter=BROKER_CONNECTION_TIME_OUT;
  //checking if wifi is connected or not
  if(WiFi.status() == WL_CONNECTED) {
    //setting the broker configuration
    mqtt_client.setServer(BROKER_HOST,BROKER_PORT);
    //setting the message handler
    mqtt_client.setCallback(mqtt_message_handler);
    //setting the keep alive interval
    mqtt_client.setKeepAlive(KEEP_ALIVE_INTERVAL);
        
    //checking if broker is connected or not
    while(!mqtt_client.connected() && timeout_counter > 0) {

      //checking if bluetooth is connected
      if(SerialBlueTooth.connected()) {
        return;
      }
           
      //clearing the prev json buffer
      json_doc.clear();

      //setting the unit id
      json_doc["uid"]=UNIT_ID;

      //disconnection topic
      const char *disconnection_topic=DISCONNECTION_TOPIC;

      //disconnecton message
      String disconnection_message;
        
      //encoding to json format
      serializeJson(json_doc,disconnection_message);
         
      //connecting to the mqtt broker
      mqtt_client.connect(UNIT_ID,BROKER_USER_NAME,BROKER_PASSWORD,disconnection_topic,1,false,disconnection_message.c_str(),false);

      Serial.println("connecting to broker -> "+String(timeout_counter));

      vTaskDelay(pdMS_TO_TICKS(1000));
    
      timeout_counter--;
    }

    if(mqtt_client.connected()) {
      mqtt_client.subscribe(UNIT_SUBSCRIBE_TOPIC );
      return;
    }
   }
 }

void update_connection_status_to_broker() {
  //clearing the prev json buffer
  json_doc.clear();

  json_doc["uid"]=UNIT_ID;

  String json_message;
  //encoding to the json format
  serializeJson(json_doc,json_message);
  //publishing the json message to the broker 
  mqtt_client.publish(CONNECTION_TOPIC,json_message.c_str());

  Serial.println("published connection status to broker");
}

void synchronize_with_deletes() {
  //publishing deletes sync message
  mqtt_client.publish(DELETE_SYNC_TOPIC,"");
}


void synchronize_with_inserts(){
  //publishing inserts sync message
  mqtt_client.publish(INSERT_SYNC_TOPIC,"");
}

void online_error_indication_handler(uint8_t error_type) {
  //disconnecting from the broker
  mqtt_client.disconnect();

  //indicating the broker disconnection
  broker_disconnection_indicater();

  //disconnecting from the wifi
  WiFi.disconnect();

  //indicating the wifi disconnection
  wifi_disconnection_indicater();

  uint8_t error_timeout_counter;

  switch(error_type) {
    case 0:
      error_timeout_counter = BROKER_SIDE_PARSE_ERROR_TIME_OUT;
      while(error_timeout_counter > 0 ){
        broker_side_parse_error_indicater();
        error_timeout_counter--;
      }
      break;
    case 1:
      error_timeout_counter = BROKER_SIDE_DB_ERROR_TIME_OUT;
      while(error_timeout_counter > 0) {
        broker_side_database_error_indicater();
        error_timeout_counter--;
      }
      break;
    case 2:
      error_timeout_counter = INVALID_UNIT_ERROR_TIME_OUT;
      while(error_timeout_counter > 0) {
        invalid_unit_indicater();
        error_timeout_counter--;
      }
      break;
    case 3:
      attendence_error_indicater();
      break;
    case 4:
      error_timeout_counter=FINGER_PRINT_DELETE_ERROR_TIME_OUT;
      while(error_timeout_counter > 0) {
        finger_print_delete_failure_indicater();
        error_timeout_counter--;
      }
      break;
    case 5:
      error_timeout_counter = FINGER_PRINT_INSERT_ERROR_TIME_OUT;
      while(error_timeout_counter > 0) {
        finger_print_insert_failure_indicater();
        error_timeout_counter--;
      }
      break;
  }
}

void broker_connection_update_ack_handler() {
  //indicating the broker connection
  broker_connection_indicater();
  //updating the broker connection status
  is_broker_connected=1;
}

bool delete_student_handler(uint16_t student_unit_id) {
  //deleting the finger print from the sensor 
  if(finger.deleteModel(student_unit_id) == FINGERPRINT_OK) {
    return true;
  } else {
    return false;
  }
}

bool insert_student_handler(uint16_t student_unit_id, const char * finger_print_data) {

  Serial.println(finger_print_data);

  //declaring the buffer memory to store the finger print data
  unsigned char *buffer = new int[512];

  //decoding from base64 format to binary
  base64_decoder(finger_print_data,buffer);

  if(finger.write_template_to_sensor(512,buffer)){
    if(finger.storeModel(student_unit_id) == FINGERPRINT_OK) {
      Serial.println("successfull");
      return true;
    } else {
      return false;
    }
  } else  {
    return false;
  }
          
}

void attendence_request_ack_handler() {
  //indicating the attendence success message
  attendence_success_indicater();
}


void mqtt_message_handler(char *topic, byte *payload,unsigned int length) {
  //clearing the prev json buffer
  json_doc.clear();

  //converting the payload from bytes to string
  String json_message((char *)payload,length);

  Serial.println(json_message.c_str());

  //decoding the json format
  DeserializationError error = deserializeJson(json_doc,json_message.c_str());

  if(error){
    Serial.println("broker json message parse error");
    //indicating the broker json message parse error
    broker_json_message_parse_error_indicater();
    return;
  }
  
  uint8_t message_type=json_doc["mty"];
  uint8_t error_status=json_doc["est"];
  uint8_t error_type;
  uint8_t is_students_empty;
  uint16_t student_unit_id;


  switch(message_type) {
    case 0:
      if(error_status == 0) {
        broker_connection_update_ack_handler();
        //turning the wifi indication off
        wifi_disconnection_indicater();
      } else {
        error_type=json_doc["ety"];
        //indicating the error
        online_error_indication_handler(error_type);
      }
      break;
    case 1:
      if(error_status == 0) {
        is_students_empty=json_doc["ste"];
        if(!is_students_empty) {
          student_unit_id=json_doc["sti"];
          if(delete_student_handler(student_unit_id)) {
            //publishing the message
            mqtt_client.publish(DELETE_SYNC_ACK_TOPIC,"");
            //updating the delete sync message publish status
            is_delete_sync_msg_published=0;
          } else {
            online_error_indication_handler(4);
            //updating the delete sync message publish status
            is_delete_sync_msg_published=0;
          }
        } else {
          //updating the deletes sync status
          is_deletes_synchronized=1;
          //updating the delte sync message publish status
          is_delete_sync_msg_published=0;
        } 
      } else {
        error_type=json_doc["ety"];
        //indicating the error

        online_error_indication_handler(error_type);
      } 
      break;
    case 2:
      if(error_status == 0) {
        is_students_empty=json_doc["ste"];
        if(!is_students_empty) {
          student_unit_id=json_doc["suid"];
          const char *finger_print_data = json_doc["fpd"];
          if(insert_student_handler(student_unit_id,finger_print_data)){
            mqtt_client.publish(INSERT_SYNC_ACK_TOPIC,"");
            //sync message publish status
            is_insert_sync_msg_published=0;
          } else {
            online_error_indication_handler(5);
            //updating the insert sync message publish status
            is_insert_sync_msg_published=0;
          }
        } else {
          //updating the inserts sync status
          is_inserts_synchronized=1;
          //updating the inserts sync message publish status
          is_insert_sync_msg_published=0;
        }
      } else {
        error_type=json_doc["ety"];
        online_error_indication_handler(error_type);
      }
      break;
    case 3:
      if(error_status == 0) {
        attendence_request_ack_handler();
        //updating the attendence publish request status
        attendence_request_status=0;
      } else  {
        error_type=json_doc["ety"];
        online_error_indication_handler(error_type);
        //updating the attendence publish request status
        attendence_request_status=0;
      }
      break;
  }
}


void base64_decoder(const char *input,unsigned char *output) {
  base64::decode(input,output);
}


void publish_attendence_log(uint16_t student_unit_id) {

    //clearing the prev json buffer
    json_doc.clear();

    //getting the current time
    DateTime now=rtc.now();

    //converting current time to string format
    String timestamp =now.timestamp();

    json_doc["uid"]=UNIT_ID;
    json_doc["suid"]=String(student_unit_id);
    json_doc["tm"]=timestamp;

    String json_message;

    //encoding to json format
    serializeJson(json_doc,json_message);

    //publishing the attendence log
    mqtt_client.publish(ATTENDENCE_TOPIC,json_message.c_str());
}

bool take_finger_print() {
  //taking the finger print image
  int sensor_status=finger.getImage();
  Serial.println("sensor status -> "+String(sensor_status));
  //checking for the sensor status
  if(sensor_status == FINGERPRINT_NOFINGER) {
    return false;
  }

  if(sensor_status == FINGERPRINT_PACKETRECIEVEERR) {
    finger_print_sensor_error_indicater();
    delay(500);
    return false;
  }

  if(sensor_status == FINGERPRINT_IMAGEFAIL) {
    finger_print_sensor_error_indicater();
    delay(500);
    return false; 
  }

  //converting the raw image to template
  sensor_status=finger.image2Tz();

  if(sensor_status == FINGERPRINT_IMAGEMESS) {
    finger_print_sensor_error_indicater();
    delay(500);
    return false;
  }

  if(sensor_status == FINGERPRINT_PACKETRECIEVEERR) {
    finger_print_sensor_error_indicater();
    delay(500);
    return false;
  }

  if(sensor_status == FINGERPRINT_FEATUREFAIL) {
    finger_print_sensor_error_indicater();
    delay(500);
    return false;
  }

  if(sensor_status == FINGERPRINT_INVALIDIMAGE) {
    finger_print_sensor_error_indicater();
    delay(500);
    return false;
  }
  
  //searching for the finger print match
  sensor_status=finger.fingerSearch();

  if(sensor_status == FINGERPRINT_PACKETRECIEVEERR) {
    finger_print_sensor_error_indicater();
    delay(500);
    return false;
  }

  if(sensor_status == FINGERPRINT_NOTFOUND) {
    finger_print_sensor_error_indicater();
    delay(500);
    return false;
  }
 
  if(sensor_status == FINGERPRINT_OK) {
    return true;
  } else {
    finger_print_sensor_error_indicater();
    delay(500);
    return false;
  }
}

void finger_print_reading_task(void *parameter) {
  uint8_t attendence_request_timeout_counter;
  while(1) {
    if(is_broker_connected && is_deletes_synchronized && is_inserts_synchronized && !attendence_request_status && !SerialBlueTooth.connected()) {
      bool is_valid_finger = take_finger_print();
      //checking weather finger is valid or not
      if(is_valid_finger) { 
        //publishing the attendence log to broker
        publish_attendence_log(finger.fingerID);
        //updating the attendence request status
        attendence_request_status=1;
        //updating the attendence request time out counter
        attendence_request_timeout_counter=ATT_REQ_TIMEOUT;
      }
    }

    if(attendence_request_status && attendence_request_timeout_counter > 0 ) {
      //decrementing the attendence request timeout counter
      attendence_request_timeout_counter--;
      //creating the 1 sec delay
      delay(1000);
    }else{
      //checking the broker respose
      if(attendence_request_status) {
        //updating the attendence request status
        attendence_request_status=0;
        //indicating the broker time out error
        attendence_request_timeout_indicater();
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void setup() {
  
  //gpio configurations
  pinMode(BUZZER_PIN,OUTPUT);
  pinMode(GREEN_LED1_PIN,OUTPUT);
  pinMode(GREEN_LED2_PIN,OUTPUT);
  pinMode(RED_LED_PIN,OUTPUT);

  //turning all the indicaters off
  turn_all_indicaters_off();

  //setting serial communication baudrate
  Serial.begin(9600);

  //mounting the SPIFFS file system
  if(!SPIFFS.begin(false)){
    Serial.println("SPIFFS Mount Failed");
    //indicating SPIFFS file system mount failure
    while(1){
        file_system_mount_fail_indicater();
    }
  }

  Serial.println("SPIFFS file system mounted successfully");

  //starting the bluetooth by specifying the bluetooth name 
  SerialBlueTooth.begin(UNIT_ID);

  // starting the finger print reading task on a new core
  xTaskCreatePinnedToCore(
    finger_print_reading_task,
    "finger print reading task",
    10000,
    NULL,
    1,
    &finger_print_reading_handler,
    0
  );

  //starting the finger print sensor communication by setting the baudrate
  finger.begin(FINGER_PRINT_SENSOR_COMMUNICATION_BAUDRATE);

  //verifying the finger print sensor connection
  if(!finger.verifyPassword()){
    Serial.println("finger print sensor not detected");
    while(1){
       fingerprint_sensor_not_found_indicater();
    }
  }

  Serial.println("finger print sensor found");

  // //verifying the rtc module connection
  if(!rtc.begin()){
    Serial.println("RTC module not found");
    while(1) {
      rtc_module_not_found_indicater();
    }
  }

  Serial.println("RTC module found");

  //setting wifi to station mode
  WiFi.mode(WIFI_STA);
  
  //connecting to wifi
  connect_to_wifi();

  //checking if wifi connected or not
  if(WiFi.status() == WL_CONNECTED){
    //indicating the wifi connection
    wifi_connection_indicater();
    Serial.println("wifi connected");
  }else{
    //indicating the wifi disconnection
    wifi_disconnection_indicater();
    Serial.println("wifi connection failed");
  }

  //connecting to broker
  connect_to_broker();

  if(mqtt_client.connected()) {
    Serial.println("connected to broker");
  }else{
    Serial.println("failed to connect to broker");
  }
}

void loop() {
  
    //checking for bluetooth connection
    while(SerialBlueTooth.connected()) {

       if(!is_bluetooth_connected) {
         //disconnecting from the prev wifi
         WiFi.disconnect();
         //reset all status
         reset_all_online_status();
         //turning all the indicaters off
         turn_all_indicaters_off();
         //indicating the bluetooth connection
         bluetooth_connection_indicater();
         //updating the bluetooth connection status
         is_bluetooth_connected=1;
       }

       //checking for the bluetooth message
       if(SerialBlueTooth.available()) {
         char incoming_char=SerialBlueTooth.read();
         if(incoming_char != '*') {
           input_bluetooth_message+=incoming_char;
         }else{
           wifi_config_handler(input_bluetooth_message);
         }
       }

     }

  //checking for the bluetooth disconnection
  if(!SerialBlueTooth.connected() && is_bluetooth_connected) {
    //indicating the bluetooth disconnection
    bluetooth_disconnection_indicater();
    //updating the bluetooth disconnection status
    is_bluetooth_connected=0;
  }
  
  //checking the wifi status
  if(!SerialBlueTooth.connected() && WiFi.status()!=WL_CONNECTED) {
    //reset all status
    reset_all_online_status();
    //disconnecting from the broker
    mqtt_client.disconnect();
    //indicating the broker disconnection
    broker_disconnection_indicater();
    //connecting to the wifi
    connect_to_wifi();
    
    //checking if wifi is connected or not
    if(WiFi.status() == WL_CONNECTED) {
      //indicating the wifi connection
      wifi_connection_indicater();
      Serial.println("connected to wifi");
    }else{
      //indicating the wifi disconnection
      wifi_disconnection_indicater();
      Serial.println("wifi connection failed");
    }
  }

  //checking the broker connection status
  if(!SerialBlueTooth.connected() && WiFi.status() == WL_CONNECTED && !mqtt_client.connected()) {
    //reset all status
    reset_all_online_status();
    //indicating the broker disconnection
    broker_disconnection_indicater();
    //connecting to broker
    connect_to_broker();

    //checking if connected to broker or not
    if(mqtt_client.connected()) {
      Serial.println("connected to broker");
    }else{
      Serial.println("failed to connect to broker");
    }
  }

  //checking for the broker connection update status 
  if(!SerialBlueTooth.connected() && mqtt_client.connected() && !is_broker_connected) {
    //updating the connection status to broker
    update_connection_status_to_broker();
    
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

 //checking if synchronized with deletes or not
  if(!SerialBlueTooth.connected() && is_broker_connected && !is_deletes_synchronized && !is_delete_sync_msg_published) {
    //sync with deletes
    synchronize_with_deletes();
    //updating the delete sync message publish status
    is_delete_sync_msg_published=1;
    Serial.println("syncing with deletes...");
  }

  //checking if synchronized with inserts or not
  if(!SerialBlueTooth.connected() && is_broker_connected && is_deletes_synchronized && !is_inserts_synchronized && !is_insert_sync_msg_published) {
    //sync with inserts
    synchronize_with_inserts();
    //updating the insert sync message publish status
    is_insert_sync_msg_published=1;
    Serial.println("syncing with inserts...");
  }

  //checking for the incoming mqtt messages
  mqtt_client.loop();
}