#include <SD.h>

#define LOGFILE "datalog.txt"

File myfile;

void setupLog () 
{

  if (!SD.begin(4)) 
  {
    Serial.println("initialization of the SD card failed!");
    return;
  }
  
  Serial.println("initialization of the SDcard is done.");
  
}




void tolog(String message) 
{
   myfile = SD.open(LOGFILE, FILE_WRITE);
   myfile.println(message);
   myfile.close();
   Serial.println(message);
}



//overloads
void tolog()
{
  tolog("");
}

void tolog(int message)
{
   tolog(String(message));
}
