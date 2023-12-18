#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "I2SSampler.h"
#include "I2SMEMSSampler.h"
#include "ADCSampler.h"
#include "I2SOutput.h"
#include "SDCard.h"
#include "SPIFFS.h"
#include "WAVFileWriter.h"
#include "WAVFileReader.h"
#include "configure.h"
#include "esp_timer.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "vivo V21e"
#define WIFI_PASSWORD "mostafa55"

// Insert Firebase project API Key
#define API_KEY "AIzaSyB0j7mJ2lz0tZQ3pefqQ59Re5f5NW3NT4s"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "alerty-b8707-default-rtdb.europe-west1.firebasedatabase.app" 

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

bool signupOK = false;
String prediction;

static const char *TAG = "app";

extern "C"
{
  void app_main(void);
}

void record(I2SSampler *input, const char *fname)
{
  int16_t *samples = (int16_t *)malloc(sizeof(int16_t) * 1024);
  ESP_LOGI(TAG, "Start recording");
  input->start();
  // open the file on the sdcard
  File file = SD.open(fname, FILE_WRITE);
  if (file != NULL) {
    Serial.println("file was created successfuly");
  }
  // create a new wave file writer
  WAVFileWriter *writer = new WAVFileWriter(file, input->sample_rate());
  Serial.println("Recording...");
  long long int start = esp_timer_get_time();
  while (esp_timer_get_time()-start<12000000)
  {
    int samples_read = input->read(samples, 1024);
    int64_t start = esp_timer_get_time();
    writer->write(samples, samples_read);
    int64_t end = esp_timer_get_time();
    ESP_LOGI(TAG, "Wrote %d samples in %lld microseconds", samples_read, end - start);
  }
  // stop the input
  input->stop();
  // and finish the writing
  writer->finish();
  file.close();
  delete writer;
  free(samples);
  ESP_LOGI(TAG, "Finished recording");
  Serial.println("Finished recording");
  if (Firebase.ready())
      {
          Serial.println("\nUpload file...\n");
          // MIME type should be valid to avoid the download problem.
          // The file systems for flash and SD/SDMMC can be changed in FirebaseFS.h
          if (!Firebase.Storage.upload(&fbdo, "alerty-b8707.appspot.com" /* Firebase Storage bucket id */, "/test.wav" /* path to local file */, mem_storage_type_sd /* memory storage type, mem_storage_type_flash and mem_storage_type_sd */, "test.wav" /* path of remote file stored in the bucket */, "audio/wav" /* mime type */, fcsUploadCallback /* callback function */))
              Serial.println(fbdo.errorReason());
      }
}

void setup()
{
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  ESP_LOGI(TAG, "Starting up");
  if(!SD.begin(5)){
    Serial.println("Card Mount Failed");
    }
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }
}
void fcsUploadCallback(FCS_UploadStatusInfo info)
{
    if (info.status == firebase_fcs_upload_status_init)
    {
        Serial.printf("Uploading file %s (%d) to %s\n", info.localFileName.c_str(), info.fileSize, info.remoteFileName.c_str());
    }
    else if (info.status == firebase_fcs_upload_status_upload)
    {
        Serial.printf("Uploaded %d%s, Elapsed time %d ms\n", (int)info.progress, "%", info.elapsedTime);
    }
    else if (info.status == firebase_fcs_upload_status_complete)
    {
        Serial.println("Upload completed\n");
        FileMetaInfo meta = fbdo.metaData();
        Serial.printf("Name: %s\n", meta.name.c_str());
        Serial.printf("Bucket: %s\n", meta.bucket.c_str());
        Serial.printf("contentType: %s\n", meta.contentType.c_str());
        Serial.printf("Size: %d\n", meta.size);
        Serial.printf("Generation: %lu\n", meta.generation);
        Serial.printf("Metageneration: %lu\n", meta.metageneration);
        Serial.printf("ETag: %s\n", meta.etag.c_str());
        Serial.printf("CRC32: %s\n", meta.crc32.c_str());
        Serial.printf("Tokens: %s\n", meta.downloadTokens.c_str());
        Serial.printf("Download URL: %s\n\n", fbdo.downloadURL().c_str());
    }
    else if (info.status == firebase_fcs_upload_status_error)
    {
        Serial.printf("Upload failed, %s\n", info.errorMsg.c_str());
    }
}
void loop()
{
  ESP_LOGI(TAG, "Creating microphone");
  #ifdef USE_I2S_MIC_INPUT
    I2SSampler *input = new I2SMEMSSampler(I2S_NUM_0, i2s_mic_pins, i2s_mic_Config);
  #else
    I2SSampler *input = new ADCSampler(ADC_UNIT_1, ADC1_CHANNEL_7, i2s_adc_config);
  #endif
  record(input, "/test.wav");
  delay(10000);
  if (Firebase.ready() && signupOK) {
    if (Firebase.RTDB.getString(&fbdo, "/predictions/prediction")) {
      if (fbdo.dataType() == "string") {
        prediction = fbdo.stringData();
        Serial.println(prediction);
      }
    }
  }

}