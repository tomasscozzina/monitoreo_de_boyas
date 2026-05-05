#include "main.h"
#include "string.h"

#define numTx_DR0 10
#define numTx_DR2 11

int main(void){

	system_init();
	lorawan_setup();

	char texto_payload[20];
	lorawan_setADR(false);	// TOMI: Apago el ADR para la prueba de alcance

	while (1){

		if(get_sw1PressEv()){

			for(int i = 0; i < 6; i++){
				HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET);
				HAL_Delay(100);
				HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);
				HAL_Delay(100);
			}

			DPRINT("INICIANDO TRANSMICIÓN CON DR0 (SF12) \r\n\n");
			lorawan_setDataRate(0);		// Seteo DR0 (SF12) para las primeras 20 Tx
			for(int i = 0; i < numTx_DR0; i++){
				lorawan_downlink_t downlink;
				lorawan_uplink_t uplink;

				int len = snprintf(texto_payload, sizeof(texto_payload), "%d,%d,%d", i,
						lorawan_getRSSI(),
						lorawan_getSNR());

				uplink.payload = texto_payload;
				uplink.len = (uint8_t)len;
				uplink.port = PERIODIC_TRANSMISSION_PORT;
				uplink.confirmed = false;

				lorawan_send(&uplink, &downlink);

				HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET);
				HAL_Delay(500);
				HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);

				HAL_Delay(4500);	// aproximadamente, 5 segundos en total entre Tx
			}

			DPRINT("INICIANDO TRANSMICIÓN CON DR2 (SF10) \r\n\n");
			lorawan_setDataRate(2);		// Seteo DR2 (SF10) para las segundas 20 Tx
			for(int i = 0; i < numTx_DR2; i++){
				lorawan_downlink_t downlink;
				lorawan_uplink_t uplink;

				int len = snprintf(texto_payload, sizeof(texto_payload), "%d,%d,%d", i,
										lorawan_getRSSI(),
										lorawan_getSNR());

				uplink.payload = texto_payload;
				uplink.len = (uint8_t)len;
				uplink.port = PERIODIC_TRANSMISSION_PORT;
				uplink.confirmed = false;

				lorawan_send(&uplink, &downlink);

				HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET);
				HAL_Delay(500);
				HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);

				HAL_Delay(4500);	// aproximadamente, 5 segundos en total entre Tx
			}

			DPRINT("TRANSMISIONES FINALIZADAS \r\n\n");
			get_sw1PressEv();	// Esto para borrar la flag por pulsados accidentales durante las Tx
		}

	}
}
