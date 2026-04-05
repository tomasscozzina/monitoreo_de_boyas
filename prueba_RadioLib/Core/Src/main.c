#include "main.h"
#include "string.h"

int main(void){

	system_init();
	lorawan_setup();

	static uint16_t n_mensaje = 1;
	char texto_payload[20]; // Buffer para "mensaje XXX"

	while (1){
		lorawan_downlink_t downlink;
		lorawan_uplink_t uplink;

		int len = snprintf(texto_payload, sizeof(texto_payload), "Mensaje %u", n_mensaje);

		uplink.payload = texto_payload;
		uplink.len = (uint8_t)len;
		uplink.port = PERIODIC_TRANSMISSION_PORT;
		uplink.confirmed = false;

		lorawan_send(&uplink, &downlink);

		n_mensaje++;
		HAL_Delay(10000);
	}
}
