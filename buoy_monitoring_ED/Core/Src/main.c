#include "main.h"
#include "gps.h"

int main(void)
{
    system_init();

    while (1)
    {
        if (get_sw1PressEv())
        {
            DPRINT("INICIANDO\r\n");
            GPS_Status ret = GPS_WakeUp();

            switch(ret){
            	case GPS_OK:
            		DPRINT("GPS_OK\r\n");
            	break;
            	case GPS_ERR_TIMEOUT:
            		DPRINT("ERROR: GPS_ERR_TIMEOUT\r\n");
            	continue;
            	case GPS_ERR_CHECKSUM:
            	    DPRINT("ERROR: GPS_ERR_CHECKSUM\r\n");
            	continue;
            	case GPS_ERR_BADHEADER:
            	    DPRINT("ERROR: GPS_ERR_BADHEADER\r\n");
				continue;
            	case GPS_ERR_NO_FIX:
					DPRINT("ERROR: GPS_ERR_NO_FIX\r\n");
				continue;
            	case GPS_ERR_ANTENNA:
        			DPRINT("ERROR: GPS_ERR_ANTENNA\r\n");
        		continue;
            }

            ret = GPS_HasValidFix();
            while(ret != GPS_OK) {
                switch(ret){
                	case GPS_OK:
                        DPRINT("GPS_OK\r\n");
                    break;
                	case GPS_ERR_TIMEOUT:
                		DPRINT("ERROR: GPS_ERR_TIMEOUT\r\n");
                	break;
                	case GPS_ERR_CHECKSUM:
                	    DPRINT("ERROR: GPS_ERR_CHECKSUM\r\n");
                	break;
                	case GPS_ERR_BADHEADER:
                	    DPRINT("ERROR: GPS_ERR_BADHEADER\r\n");
                	break;
                	case GPS_ERR_NO_FIX:
                		DPRINT("ERROR: GPS_ERR_NO_FIX\r\n");
                	break;
                	case GPS_ERR_ANTENNA:
            			DPRINT("ERROR: GPS_ERR_ANTENNA\r\n");
            		break;
                }
                HAL_Delay(1000);
                ret = GPS_HasValidFix();
            }
            DPRINT("FIX_OK\r\n");

            int32_t lat, lon;
            GPS_UTCTime utc;

            GPS_GetLatitude(&lat);
            GPS_GetLongitude(&lon);
            GPS_GetUTCTime(&utc);

            DPRINT("LATITUD  : %ld (x1e-7 deg)\r\n", lat);
            DPRINT("LONGITUD : %ld (x1e-7 deg)\r\n", lon);
            DPRINT("FECHA    : %04d-%02d-%02d\r\n", utc.year, utc.month, utc.day);
            DPRINT("HORA UTC : %02d:%02d:%02d\r\n", utc.hour, utc.min, utc.sec);

            GPS_Sleep();
            DPRINT("MEDICIONES FINALIZADAS\r\n");
            DPRINT("GPS EN MODO BACKUP\r\n\n");
        }
    }
}
