#include "RF_Data_Processor.h"

RF_Data_Processor::RF_Data_Processor() {
  TRANSMITTING_DELAY = 20;
  RECEIVING_DELAY = 10;
  SETUP_DELAY = 60;
  COUNT_OF_ATTEMPTS = 5;

  lastJsonPackId = 0;
  lastJsonNumber = 0;
  lastJsonCode = 0;
  jsonBuffer = "";
  lastJson = "";
  newJsonAvailable = false;
}

//----- Public ----------------------------------------------------------------------------------------------

void RF_Data_Processor::initializeRadio(char readingAddress[], char writingAddress[]) {
  setReadingPipeAddress(readingAddress);
  setWritingPipeAddress(writingAddress);

  _radio->begin();
}


void RF_Data_Processor::setWritingPipeAddress(char address[]) {
  for(int i = 0; i < sizeof(WritingPipeAddress); i++) {
    WritingPipeAddress[i] = (byte) address[i];
  }
}


void RF_Data_Processor::setReadingPipeAddress(char address[]) {
  for(int i = 0; i < sizeof(ReadingPipeAddress); i++) {
    ReadingPipeAddress[i] = (byte) address[i];
  }
}

void RF_Data_Processor::setupRadioForWriting() {
  if(_radio != NULL) {
    _radio -> stopListening();
    _radio -> openWritingPipe(WritingPipeAddress);
    delay(SETUP_DELAY);
  }
}

void RF_Data_Processor::setupRadioForReading() {
  if(_radio != NULL) {
    _radio -> startListening();
    _radio -> openReadingPipe(1, ReadingPipeAddress );
    delay(SETUP_DELAY);
  }
}

String RF_Data_Processor::receiveJson(void) {
  String pack = receive(); //Get data from RF
  pushJsonPack(pack); //Proceed
  return pack; //Return pack
}

void RF_Data_Processor::pushJsonPack(String pack) {
  if(pack == "") return "";

  DynamicJsonDocument docPack(64);
  deserializeJson(docPack, pack);

  const int type = docPack["m"][0];
  const int packNumber = docPack["m"][1];
  const int packId = docPack["m"][2];

  const char* data = docPack["d"];

  //If packNumber does not equal per+1 then clear buffer
  //If packId not equal prev, then clear buffer
  if(type != 1 && type != 2 && (packNumber != lastJsonNumber+1 || packId != lastJsonPackId)) {
    clearJsonBuffer();
  } else {
    
    if(type == 1) { // If operType == 1 - put data into buffer and send it for processing
      jsonBuffer = (String) data;
      generateJsonFromBuffer();
    }
    
    if(type == 2) { // if operType == 2 - put data into buffer
      jsonBuffer = (String) data;
    }
    
    if(type == 3) { // If operType == 3 - append data to the buffer
      jsonBuffer += (String) data;
    }
    
    if(type == 4) { // If operType == 4 - append data to the buffer and call processing
      jsonBuffer += (String) data;
      generateJsonFromBuffer();
    }

    //Set global values for last json pack info JSONs
    lastJsonPackId = packId;
    lastJsonCode = type;
    lastJsonNumber = packNumber;
  }

  return (String) data;
}


String RF_Data_Processor::getLastJson(void) {
  newJsonAvailable = false;
  return String(lastJson);
}

bool RF_Data_Processor::available(void) {
  return newJsonAvailable;
}

String RF_Data_Processor::receive(void) {
  String result = "";
    while (_radio -> available()) {
        char text[32] = "";
        _radio -> read(&text, sizeof(text));
        result = result + text;
    }
    return result;
}

void RF_Data_Processor::sendJson(char* message, int messageSize) {
  //CONSTANT VALUES FOR PACKS---------------------------------
  int code = 1;
  int packNo = 1;
  const size_t ARRAY_CAPACITY_FOR_PACK = JSON_ARRAY_SIZE(3);// compute the required size
  //----------------------------------------------------------

  //PREPARE---------------------------------------------------
  bool isInitPack = true;
  bool sending = true;
  int lastIndex = 0;
  char payload[messageSize];
  for(int g = 0; g < messageSize; g++)payload[g] = (message[g] != '\"')? message[g]: '$'; //Copy message to another array and replace all forbidden characters
  String payloadString = String(payload);
  //----------------------------------------------------------

  while(sending) {
    // 1. Generate empty json
    DynamicJsonDocument emptyDocPack(64);
    //generate meta
    StaticJsonDocument< ARRAY_CAPACITY_FOR_PACK > initMetaArrayDoc;// allocate the memory for the document
    JsonArray initMetaArray = initMetaArrayDoc.to<JsonArray>();// create an empty array
    initMetaArray.add(0); //Any number between 0 and 9, needed just to calculate size
    initMetaArray.add(packNo);
    initMetaArray.add(lastPackId);
    emptyDocPack["m"] = initMetaArrayDoc;
    emptyDocPack["d"] = "";

    // 2. Measure its actual size in bytes and calculate required data
    int emptyPackSize = measureJson(emptyDocPack);
    int availablePayloadSize = 32 - emptyPackSize - 2;//Minus two because we will transform it into an array later

    // 3. Generate data snippet(from payload)
    String payloadSnippet = payloadString.substring(lastIndex, lastIndex + availablePayloadSize);
    lastIndex = lastIndex + availablePayloadSize;

    // 4. Create actually object to save data input
    DynamicJsonDocument docPack(64);
    StaticJsonDocument< ARRAY_CAPACITY_FOR_PACK > metaArrayDoc;//allocate the memory for the document
    JsonArray metaArray = metaArrayDoc.to<JsonArray>();// create an empty array
    //Set code( 1-one message is a full pack, 2-beginning of the message, 3-somewhere in the middle, 4-end of the message(last pack))
    if(availablePayloadSize >= payloadString.length()) {
      metaArray.add(1);
    } else if(isInitPack) {
      metaArray.add(2);
      isInitPack = false;
    }  else if(lastIndex >= payloadString.length()) {
      metaArray.add(4);
    } else {
      metaArray.add(3);
    }
    metaArray.add(packNo);
    metaArray.add(lastPackId);
    docPack["m"] = metaArrayDoc;
    docPack["d"] = payloadSnippet; //Insert payload snippet
    
    // 5. Serialize pack
    String finishedPack;
    serializeJson(docPack, finishedPack);
    char finishedPackArr[finishedPack.length()+1];
    finishedPack.toCharArray(finishedPackArr, sizeof(finishedPackArr));

    // 6. Send finished pack
    send(finishedPackArr, sizeof(finishedPackArr));
    
    // 7. If all payload was sent - stop sending
    if(lastIndex >= payloadString.length()) sending = false;
    packNo++;
  }
  
  //Maximum pack id is 99
  getLastPackId();
}

void RF_Data_Processor::send(String message) {
  //If message is longer than 32 bytes we have to send many packs
  for(int i = 0; i < ceil(message.length()/32.0); i++) {
    String snippetToSend = message.substring(i*32, (i+1)*32);

    char chArr[32];
    snippetToSend.toCharArray(chArr, sizeof(chArr));
    _radio -> write(&chArr, sizeof(chArr));
    delay(TRANSMITTING_DELAY); //Time to perform transmitting
  }
}

void RF_Data_Processor::send(char* message, int messageSize) {
  int countOfPacks = ceil(messageSize/32.0);

  //If message is longer than 32 bytes we have to send many packs
  for(int i = 0; i < countOfPacks; i++) {
    int numOfBytesToSend = ((i+1) == countOfPacks)? messageSize%32: 32;
    char* pointer = message + (i*32);

    bool wasPackSend = false;

    //Send pack, if fails repeat, if to many attempts then stop trying
    for(int countOfAttempts = 0; countOfAttempts < COUNT_OF_ATTEMPTS; countOfAttempts++) {
      wasPackSend = _radio -> write(pointer, numOfBytesToSend);
      delay(TRANSMITTING_DELAY); //Time to perform transmitting
      if(wasPackSend) return;
    }

    //Stop sending if it fails
    if(!wasPackSend) return;
  }
}


//----- Private ---------------------------------------------------------------------------------------------

void RF_Data_Processor::clearJsonBuffer(void) {
  jsonBuffer = "";
  lastJsonPackId = -1;
  lastJsonCode = -1;
  lastJsonNumber = -1;
}

void RF_Data_Processor::generateJsonFromBuffer(void) {
  jsonBuffer.replace("$", "\"");
  lastJson = String(jsonBuffer);
  clearJsonBuffer();
  newJsonAvailable = true;
}

int RF_Data_Processor::getLastPackId(void) {
  int res = lastPackId;
  lastPackId++;
  if(lastPackId > 99) lastPackId = 1;
  return res;
}
