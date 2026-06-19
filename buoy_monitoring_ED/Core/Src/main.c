// Código para el TEST de precisión

#include "main.h"
#include "ina219.h"

int main(void) {
    system_init();
    DPRINT("Sistema iniciado\r\n");

    /* Inicializar los 3 módulos INA219 */
    INA219_Status ret = INA219_InitAll();
    if (ret != INA219_OK) {
        DPRINT("ERROR: INA219_InitAll() = %d\r\n", ret);
        while (1) {}
    }
    DPRINT("INA219: 3 modulos inicializados OK\r\n");
    DPRINT("Presiona SW1 para obtener una medición\r\n");

    int32_t voltage_mV = 0;
    int32_t current_mA = 0;
    INA219_Channel ch = INA219_CH1;

    while (1) {

        if (get_sw1PressEv())
        {
        	ret = INA219_GetVoltage(ch, &voltage_mV);
			if (ret != INA219_OK) {
				DPRINT("ERROR: INA219_GetVoltage(CH%d) = %d\r\n", (int)ch, ret);
				HAL_Delay(500);
				continue;
			}

			ret = INA219_GetCurrent(ch, &current_mA);
			if (ret != INA219_OK) {
				DPRINT("ERROR: INA219_GetCurrent(CH%d) = %d\r\n", (int)ch, ret);
				HAL_Delay(500);
				continue;
			}

			DPRINT("CH%d | %ld mV | %ld mA\r\n", (int)ch, voltage_mV, current_mA);
//
//    		ret = INA219_DetectAndMeasure(ch, &current_mA,3100);
//    		if (ret != INA219_OK) {
//				DPRINT("ERROR: INA219_DetectAndMeasure(CH%d) = %d\r\n", (int)ch, ret);
//				continue;
//			}
//    		DPRINT("CH%d | %ld mA\r\n", (int)ch, current_mA);
        }
    }
}
