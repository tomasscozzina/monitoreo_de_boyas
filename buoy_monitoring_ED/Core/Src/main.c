/**
 ******************************************************************************
 * @file    main.c
 * @brief   Main de prueba para el ADXL345.
 *
 * Prueba las dos funciones del acelerómetro:
 *   - Al presionar SW1: lee la inclinación e imprime los resultados.
 *   - Si INT1 dispara (impacto): imprime un aviso de emergencia.
 ******************************************************************************
 */

#include "main.h"
#include "adxl345.h"
#include <math.h>

int main(void) {
    system_init();
    DPRINT("Sistema iniciado\r\n");

    /* Inicializar el acelerómetro */
    ADXL345_Status ret = ADXL345_Init();
    if (ret != ADXL345_OK) {
        DPRINT("ERROR: ADXL345_Init() = %d\r\n", ret);
        while (1) {}
    }

    DPRINT("Presiona SW1 para leer inclinacion\r\n");

    while (1) {

        // Prueba de inclinación, disparada por SW1
        if (get_sw1PressEv()) {
            DPRINT("Leyendo inclinacion...\r\n");

            ADXL345_TiltData tilt;
            ret = ADXL345_ReadTilt(&tilt);

            if (ret == ADXL345_OK) {
                // TOMI: DPRINT no soporta %f. Se imprimen partes entera y decimal por separado.
                int total_i = (int)tilt.tilt_deg;
                int total_d = (int)(fabsf(tilt.tilt_deg - total_i) * 100);
                int xz_i    = (int)tilt.tilt_xz_deg;
                int xz_d    = (int)(fabsf(tilt.tilt_xz_deg - xz_i) * 100);
                int yz_i    = (int)tilt.tilt_yz_deg;
                int yz_d    = (int)(fabsf(tilt.tilt_yz_deg - yz_i) * 100);

                DPRINT("Inclinacion total: %d.%02d grados\r\n", total_i, total_d);
                DPRINT("Plano XZ (Y=0):    %d.%02d grados\r\n", xz_i, xz_d);
                DPRINT("Plano YZ (X=0):    %d.%02d grados\r\n", yz_i, yz_d);
            }
            // Codigo para el TEST
//			if (ret == ADXL345_OK) {
//				float xz = fabsf(tilt.tilt_xz_deg);
//				int xz_i = (int)xz;
//				int xz_d = (int)((xz - xz_i) * 100);
//				DPRINT("Plano XZ (Y=0): %d.%02d grados\r\n", xz_i, xz_d);
//			}
            else {
                DPRINT("ERROR: ADXL345_ReadTilt() = %d\r\n", ret);
            }
        }

        // Prueba de detección de impacto, disparada por INT1
        if (ADXL345_GetAndClearImpactFlag()) {

        	DPRINT("*** IMPACTO DETECTADO ***\r\n");
        	HAL_Delay(500);		// Espero 500 ms como antirebote

        	// TOMI: Siempre llamar a esta función si se detecta interrupción por Activity, ya que limpia la bandera
            ret = ADXL345_HandleImpact();
            if (ret != ADXL345_OK) {
                DPRINT("ERROR: ADXL345_GetAndClearImpactFlag() = %d\r\n", ret);
            }
        }
    }
}
