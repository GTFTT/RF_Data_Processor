#ifndef RF_Data_Processor_h
#define RF_Data_Processor_h

//Standard stuff
#include "Arduino.h"

#include <SPI.h>                  /* to handle the communication interface with the modem*/
#include <nRF24L01.h>             /* to handle this particular modem driver*/
#include <RF24.h>                 /* the library which helps us to control the radio modem*/
#include <ArduinoJson.h>          /* Process JSON documents, serialization, packs and other*/

#define SCK  13
#define MISO 12
#define MOSI 11
#define CSN  8
#define CE   7

/*
  RF_Data_Processor - Use to convert data into special strings structure and decode it later into values.
  There are also available methods to send raw data.

  The protocol used by this library is not the best yet, it transmits data by packages of max size 32 bytes.
  It is not so efficient but it have to work anyway. Future protocol have to be changed to be faster,
  less weight and more compact;

  Current protocol: {"m": [1, 2, 3], "d": ""}
    - "m" stands for meta, contains data in that order(type of payload, package number, package id)
    - "d" stands for data, there is actually piece of JSON document we wont to transmit
  
  Future protocol: | serialized meatdata here... | serialized data here... |

  Created by:    GT
  Creation date: 03.05.2021
  Last edited:   17.05.2021
*/
class RF_Data_Processor
{
  public:
    /*Initialize default values*/
    RF_Data_Processor(void);

    /*Must be called to initialize all required radio variables*/
    void initializeRadio(char readingAddress[]="00009", char writingAddress[]="00008");//Default reading and writing pipe addresses

    /*Makes all needed operations to prepare radio for writing*/
    void setupRadioForWriting(void);

    /*Makes all needed operations to prepare radio for reading*/
    void setupRadioForReading(void);

    /*Set writing pipe adderes, provide array with 5 characters and one closing character(char[5] = "00008")*/
    void setWritingPipeAddress(char address[]);

    /*Set reading pipe adderes, provide array with 5 characters and one closing character(char[5] = "00009")*/
    void setReadingPipeAddress(char address[]);

    /*Call this to receive message instantly and return it back to the user*/
    String receive(void);

    /*Receive JSON via radio comunication and procees it to generetae full result*/
    String receiveJson(void);

    /*Manually provide JSON and procees it to generetae full result*/
    String pushJsonPack(String pack);

    /*Send json in special format, it will be decoded later after receiving*/
    void sendJson(char message[], int messageSize);

    /*Send raw array of characters*/
    void send(char message[], int messageSize);
    
    /*Send raw string*/
    void send(String message);

    String getLastJson(void);
    bool available(void);
  private:
    static RF24* _radio; //Static pointer is used because it does not disapper

    int TRANSMITTING_DELAY; //Time for transmitting a message
    int COUNT_OF_ATTEMPTS; //How meny time to try to send one pack a message
    int RECEIVING_DELAY; //Time for receiving a message
    int SETUP_DELAY; //Time for setup modem
    int lastPackId = 1; //Last id of sent pack(99 is maximum value and 1 is minimum)
    byte WritingPipeAddress[6] = {}; /* Address to which data to be transmitted*/
    byte ReadingPipeAddress[6] = {}; /* Address from which we receive messages */

    int lastJsonPackId;
    int lastJsonNumber;
    int lastJsonCode;
    String jsonBuffer;
    String lastJson;
    bool newJsonAvailable;

    void clearJsonBuffer(void);
    void generateJsonFromBuffer(void);
    
    /*Each time this function is called it increments lastPackId, but the maximum pack id is 99 and min value is 1*/
    int getLastPackId(void);
};

#endif