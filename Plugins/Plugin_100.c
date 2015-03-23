//#######################################################################################################
//##                    This Plugin is only for use with the RFLink software package                   ## 
//##                                        Plugin-100 AlectoV2                                        ##
//##                                        > 868 MHz Plugin <                                         ##
//#######################################################################################################

/*********************************************************************************************\
 * Dit protocol zorgt voor ontvangst van Alecto weerstation buitensensoren
 * 
 * Auteur             : Nodo-team (Martinus van den Broek) www.nodo-domotica.nl
 *                      Support ACH2010 en code optimalisatie door forumlid: Arendst
 * Support            : www.nodo-domotica.nl
 * Datum              : 25 Jan 2013
 * Versie             : 1.3
 *********************************************************************************************
 * Technische informatie:
 * DKW2012 Message Format: (11 Bytes, 88 bits):
 * AAAAAAAA AAAABBBB BBBB__CC CCCCCCCC DDDDDDDD EEEEEEEE FFFFFFFF GGGGGGGG GGGGGGGG HHHHHHHH IIIIIIII
 *                         Temperature Humidity Windspd_ Windgust Rain____ ________ Winddir  Checksum
 * A = start/unknown, first 8 bits are always 11111111
 * B = Rolling code
 * C = Temperature (10 bit value with -400 base)
 * D = Humidity
 * E = windspeed (* 0.3 m/s, correction for webapp = 3600/1000 * 0.3 * 100 = 108))
 * F = windgust (* 0.3 m/s, correction for webapp = 3600/1000 * 0.3 * 100 = 108))
 * G = Rain ( * 0.3 mm)
 * H = winddirection (0 = north, 4 = east, 8 = south 12 = west)
 * I = Checksum, calculation is still under investigation
 *
 * WS3000 and ACH2010 systems have no winddirection, message format is 8 bit shorter
 * Message Format: (10 Bytes, 80 bits):
 * AAAAAAAA AAAABBBB BBBB__CC CCCCCCCC DDDDDDDD EEEEEEEE FFFFFFFF GGGGGGGG GGGGGGGG HHHHHHHH
 *                         Temperature Humidity Windspd_ Windgust Rain____ ________ Checksum
 * 
 * --------------------------------------------------------------------------------------------
 * DCF Time Message Format: (NOT DECODED!, we already have time sync through webapp)
 * AAAAAAAA BBBBCCCC DDDDDDDD EFFFFFFF GGGGGGGG HHHHHHHH IIIIIIII JJJJJJJJ KKKKKKKK LLLLLLLL MMMMMMMM
 * 	    11                 Hours   Minutes  Seconds  Year     Month    Day      ?        Checksum
 * B = 11 = DCF
 * C = ?
 * D = ?
 * E = ?
 * F = Hours BCD format (7 bits only for this byte, MSB could be '1')
 * G = Minutes BCD format
 * H = Seconds BCD format
 * I = Year BCD format (only two digits!)
 * J = Month BCD format
 * K = Day BCD format
 * L = ?
 * M = Checksum
 \*********************************************************************************************/
#define PLUGIN_ID 100
#define PLUGIN_NAME "AlectoV2"

#define DKW2012_PULSECOUNT 176
#define ACH2010_MIN_PULSECOUNT 160 // reduce this value (144?) in case of bad reception
#define ACH2010_MAX_PULSECOUNT 160

uint8_t Plugin_100_ProtocolAlectoCRC8( uint8_t *addr, uint8_t len);

unsigned int Plugin_100_ProtocolAlectoRainBase=0;

boolean Plugin_100(byte function, struct NodoEventStruct *event, char *string)
{
  boolean success=false;

  switch(function)
  {
#ifdef PLUGIN_100_CORE
  case PLUGIN_RAWSIGNAL_IN:
    {
      if (!(((RawSignal.Number >= ACH2010_MIN_PULSECOUNT) && (RawSignal.Number <= ACH2010_MAX_PULSECOUNT)) || (RawSignal.Number == DKW2012_PULSECOUNT))) return false;

      byte c=0;
      byte rfbit;
      byte data[9]; 
      byte msgtype=0;
      byte rc=0;
      unsigned int rain=0;
      byte checksum=0;
      byte checksumcalc=0;
      byte basevar;
      byte maxidx = 8;
      char buffer[11]="";  
      //==================================================================================
      if(RawSignal.Number > ACH2010_MAX_PULSECOUNT) maxidx = 9;  
      // Get message back to front as the header is almost never received complete for ACH2010
      byte idx = maxidx;
      for(byte x=RawSignal.Number; x>0; x=x-2) {
          if(RawSignal.Pulses[x-1]*RawSignal.Multiply < 0x300) rfbit = 0x80; else rfbit = 0;  
          data[idx] = (data[idx] >> 1) | rfbit;
          c++;
          if (c == 8) {
            if (idx == 0) break;
            c = 0;
            idx--;
          }   
      }
      //==================================================================================
      checksum = data[maxidx];
      checksumcalc = Plugin_100_ProtocolAlectoCRC8(data, maxidx);
  
      msgtype = (data[0] >> 4) & 0xf;               // msg type must be 5 or 10
      rc = (data[0] << 4) | (data[1] >> 4);         // rolling code

      if (checksum != checksumcalc) return false;
      if ((msgtype != 10) && (msgtype != 5)) return true;  // why true?
      //==================================================================================
      int wdir = 0;
      int temp = 0;
      byte hum = 0;
      int wspeed = 0;
      int wgust = 0;
      
      temp = (((data[1] & 0x3) * 256 + data[2]) - 400);
      hum = data[3];
      wspeed = data[4] * 108;
      wspeed = wspeed / 10;
      wgust = data[5] * 108;
      wgust = wgust / 10;
      rain = (data[6] * 256) + data[7];
      if (RawSignal.Number == DKW2012_PULSECOUNT) wdir = (data[8] & 0xf);
      // ----------------------------------
      // Output
      // ----------------------------------
      sprintf(buffer, "20;%02X;", PKSequenceNumber++); // Node and packet number 
      Serial.print( buffer );
      // ----------------------------------
      if (RawSignal.Number == DKW2012_PULSECOUNT) {
         Serial.print("DKW2012;");                    // Label
      } else {
         Serial.print("Alecto V2;");                    // Label
      }
      sprintf(buffer, "ID=00%02x;", rc);   // ID    
      Serial.print( buffer );
      sprintf(buffer, "TEMP=%04x;", temp);     
      Serial.print( buffer );
      sprintf(buffer, "HUM=%02x;", hum);     
      Serial.print( buffer );
      sprintf(buffer, "WINSP=%04x;", wspeed);     
      Serial.print( buffer );
      sprintf(buffer, "WINGS=%04x;", wgust);     
      Serial.print( buffer );
      sprintf(buffer, "RAIN=%04x;", rain);     
      Serial.print( buffer );
      if (RawSignal.Number == DKW2012_PULSECOUNT) {
         sprintf(buffer, "WDIR=%04x;", wdir);     
         Serial.print( buffer );
      }
      Serial.println();     
      // ----------------------------------
      RawSignal.Repeats=true;                          // suppress repeats of the same RF packet
      RawSignal.Number=0;                              // do not process the packet any further
      success = true;                                  // processing successful
      break;
    }
#endif // PLUGIN_100_CORE
  }      
  return success;
}

#ifdef PLUGIN_100_CORE
/*********************************************************************************************\
 * Calculates CRC-8 checksum
 * reference http://lucsmall.com/2012/04/29/weather-station-hacking-part-2/
 *           http://lucsmall.com/2012/04/30/weather-station-hacking-part-3/
 *           https://github.com/lucsmall/WH2-Weather-Sensor-Library-for-Arduino/blob/master/WeatherSensorWH2.cpp
 \*********************************************************************************************/
uint8_t Plugin_100_ProtocolAlectoCRC8( uint8_t *addr, uint8_t len)
{
  uint8_t crc = 0;
  // Indicated changes are from reference CRC-8 function in OneWire library
  while (len--) {
    uint8_t inbyte = *addr++;
    for (uint8_t i = 8; i; i--) {
      uint8_t mix = (crc ^ inbyte) & 0x80; // changed from & 0x01
      crc <<= 1; // changed from right shift
      if (mix) crc ^= 0x31;// changed from 0x8C;
      inbyte <<= 1; // changed from right shift
    }
  }
  return crc;
}
#endif // PLUGIN_100_CORE