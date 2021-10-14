#include <Arduino.h>

void GetNtpTime(String server, long gmtOffset, int daylightOffset, tm* timeinfo, bool* result)
{
  //configuration
  configTime(gmtOffset, daylightOffset, server.c_str());

  int intentos = 0;
  *result = false;

  while (intentos < 3 && !*result)
  {
    intentos++;

    if (getLocalTime(timeinfo))
    {
      *result = true;
    }
    else
    {
      //wait 5 seconds before the next iteration
      sleep(5);
    }
  }
}