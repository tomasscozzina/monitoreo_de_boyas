#include "lorawan_wrapper.h"
#include "main.h"
#include <RadioLib.h>

/* ── Identificadores de pines ── */
#define PIN_ID_CS 		1
#define PIN_ID_RST 		2
#define PIN_ID_G0 		3

/* ── Defines y macros para el manejo de memoria ── */
#define NONCE_FLASH_PAGE   	63U
#define NONCE_FLASH_BANK   	FLASH_BANK_1
#define NONCE_FLASH_ADDR   	0x0801F800UL
#define ALIGN8(x)  			(((x) + 7U) & ~7U)
#define OFF_MAGIC   		0U
#define OFF_NONCES  		8U
#define FLASH_MAGIC  		0xAA55AA55UL
#define BUF_SIZE  			(OFF_NONCES + ALIGN8(RADIOLIB_LORAWAN_NONCES_BUF_SIZE))

extern SPI_HandleTypeDef hspi1;

static void (*_g0_cb)(void) = nullptr;

class STM32Hal : public RadioLibHal {

public:
    STM32Hal() : RadioLibHal(GPIO_PIN_RESET, GPIO_PIN_SET,
                             GPIO_PIN_RESET, GPIO_PIN_SET,
							 GPIO_MODE_IT_RISING, GPIO_MODE_IT_FALLING) {}

    // Métodos virtuales y abstractos

    void pinMode(uint32_t pin, uint32_t mode) override {}
    void spiBegin() override {}
    void spiEnd() override {}

    // Métodos virtuales

    long pulseIn(uint32_t pin, uint32_t state, unsigned long timeout) override {
    	return 0;
    }

    void digitalWrite(uint32_t pin, uint32_t value) override {
        GPIO_PinState state = (value == GPIO_PIN_SET) ? GPIO_PIN_SET : GPIO_PIN_RESET;

        if (pin == PIN_ID_CS) {
            HAL_GPIO_WritePin(RFM95W_CS_GPIO_Port, RFM95W_CS_Pin, state);
        }
        else if (pin == PIN_ID_RST) {
            HAL_GPIO_WritePin(RFM95W_RST_GPIO_Port, RFM95W_RST_Pin, state);
        }
    }

    uint32_t digitalRead(uint32_t pin) override {
        if (pin == PIN_ID_G0) {
            return HAL_GPIO_ReadPin(RFM95W_G0_GPIO_Port, RFM95W_G0_Pin);
        }
        return 0;
    }

    void attachInterrupt(uint32_t interruptNum, void (*cb)(void), uint32_t mode) override {
        (void)interruptNum;
        (void)mode;
        _g0_cb = cb;
    }

    void detachInterrupt(uint32_t interruptNum) override {
        (void)interruptNum;
        _g0_cb = nullptr;
    }

    void spiBeginTransaction() override {
        HAL_GPIO_WritePin(RFM95W_CS_GPIO_Port, RFM95W_CS_Pin, GPIO_PIN_RESET);
    }

    void spiEndTransaction() override {
        HAL_GPIO_WritePin(RFM95W_CS_GPIO_Port, RFM95W_CS_Pin, GPIO_PIN_SET);
    }

    void spiTransfer(uint8_t *out, size_t len, uint8_t *in) override {
        if (len == 0 || out == NULL || in == NULL) return;

        // La HAL accede directamente a la memoria de RadioLib
        HAL_SPI_TransmitReceive(&hspi1, out, in, (uint16_t)len, HAL_MAX_DELAY);
    }

    void delay(unsigned long ms) override {
    	HAL_Delay(ms);
    }

    void delayMicroseconds(unsigned long us) override {

    	uint32_t count = us * (SystemCoreClock / 1000000U) / 4;
        for (volatile uint32_t i = 0; i < count; i++) {
        	__asm__("nop"); 	// TOMI: Esto se agrega para que al optimizar la compilacion, el compilador no borre este bucle vacío
        }
    }

    unsigned long millis() override {
    	return HAL_GetTick();
    }

    unsigned long micros() override {
    	return HAL_GetTick() * 1000U;
    }
};

static STM32Hal hal;
static Module module(&hal, PIN_ID_CS, PIN_ID_G0, PIN_ID_RST, RADIOLIB_NC);
static SX1276 radio(&module);
static LoRaWANNode node(&radio, &AU915, 2);

extern "C" {

	static uint64_t bytes_to_u64(const uint8_t *b) {
		return ((uint64_t)b[0] << 56) | ((uint64_t)b[1] << 48) |
			   ((uint64_t)b[2] << 40) | ((uint64_t)b[3] << 32) |
			   ((uint64_t)b[4] << 24) | ((uint64_t)b[5] << 16) |
			   ((uint64_t)b[6] << 8)  | (uint64_t)b[7];
	}

	static bool flash_restore(LoRaWANNode *node) {
		const uint8_t *base = (const uint8_t *)NONCE_FLASH_ADDR;

		uint32_t magic;
		memcpy(&magic, base + OFF_MAGIC, sizeof(magic));
		if (magic != FLASH_MAGIC) {
			return false;
		}
		node->setBufferNonces(base + OFF_NONCES);
		return true;
	}

	static HAL_StatusTypeDef flash_save(LoRaWANNode *node) {
		static uint8_t buf[BUF_SIZE];
		memset(buf, 0xFF, sizeof(buf));

		uint32_t magic = FLASH_MAGIC;
		memcpy(buf + OFF_MAGIC,  &magic, sizeof(magic));
		memcpy(buf + OFF_NONCES, node->getBufferNonces(), RADIOLIB_LORAWAN_NONCES_BUF_SIZE);

		HAL_FLASH_Unlock();
		FLASH_EraseInitTypeDef erase = {
			.TypeErase = FLASH_TYPEERASE_PAGES,
			.Banks     = NONCE_FLASH_BANK,
			.Page      = NONCE_FLASH_PAGE,
			.NbPages   = 1
		};
		uint32_t pageError = 0;
		HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase, &pageError);
		if (status != HAL_OK) {
			HAL_FLASH_Lock();
			return status;
		}

		for (size_t i = 0; i < sizeof(buf); i += 8){
			uint64_t word;
			memcpy(&word, buf + i, 8);
			status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, NONCE_FLASH_ADDR + i, word);
			if (status != HAL_OK) {
				HAL_FLASH_Lock();
				return status;
			}
		}
		HAL_FLASH_Lock();
		return HAL_OK;
	}

    void lorawan_g0_irq(void) {
        if (_g0_cb)
        	_g0_cb();	// Función de Callback
    }

	/*
	 * %%%%%%%%%%%% INICIO DE API PÚBLICA %%%%%%%%%%%%%
	*/

	void lorawan_init(void) {
    	DPRINT("INICIANDO RADIO\r\n");

        int16_t state = radio.begin();
    	while (state != RADIOLIB_ERR_NONE){
    		DPRINT("LA INICIALIZACIÓN FALLÓ. CÓDIGO DE ERROR DE RADIOLIB: %d\n\r", state);
    		DPRINT("REINTENTANDO EN 10 SEGUNDOS\r\n");
    		HAL_Delay(10000);

    		state = radio.begin();
    	}
    	DPRINT("INICIALIZACIÓN EXISTOSA\r\n");
    }

    void lorawan_configure(const lorawan_credentials_t *cred) {
    	DPRINT("CONFIGURANDO CREDENCIALES\r\n");

        uint64_t joinEUI = bytes_to_u64(cred->joinEUI);
        uint64_t devEUI = bytes_to_u64(cred->devEUI);

        int16_t state = node.beginOTAA(joinEUI, devEUI, nullptr, const_cast<uint8_t *>(cred->appKey));
    	while (state != RADIOLIB_ERR_NONE){
    		DPRINT("LA CONFIGURACIÓN FALLÓ. CÓDIGO DE ERROR DE RADIOLIB: %d\n\r", state);
    		DPRINT("REINTENTANDO EN 10 SEGUNDOS\r\n");
    		HAL_Delay(10000);

    		state = node.beginOTAA(joinEUI, devEUI, nullptr, const_cast<uint8_t *>(cred->appKey));
    	}
        DPRINT("CONFIGURACIÓN EXITOSA\r\n");
    }

    void lorawan_session_restore(void) {
    	DPRINT("VERIFICANDO EXISTENCIA DE BUFFER NONCE DE SESIONES ANTERIORES \r\n");

        bool restored = flash_restore(&node);
        if(restored){
        	DPRINT("SE HA RESTAURADO EL BUFFER NONCE DE LA SESIÓN ANTERIOR \r\n");
        }
        else{
        	DPRINT("NO SE HA RESTAURADO EL BUFFER NONCE DE LA SESIÓN ANTERIOR, POR INEXISTENCIA O DATO CORROMPIDO \r\n");
        }
    }

    void lorawan_join(void) {
    	DPRINT("ACTIVANDO END DEVICE VÍA OTAA \r\n");

    	node.setADR(false);		// TOMI: Prueba apagando el ADR para test de alcance

    	int16_t state = node.activateOTAA();
        while((state != RADIOLIB_LORAWAN_NEW_SESSION) && (state != RADIOLIB_LORAWAN_SESSION_RESTORED)){
    		DPRINT("LA ACTIVACIÓN FALLÓ. CÓDIGO DE ERROR DE RADIOLIB: %d\n\r", state);
    		DPRINT("REINTENTANDO EN 10 SEGUNDOS\r\n");
    		HAL_Delay(10000);
    		state = node.activateOTAA();
        }
        node.setDatarate(0);	// TOMI: Prueba fijando DR0 (SF12) para prueba de alcance
        DPRINT("ACTIVACIÓN EXITOSA \r\n");
    }

    void lorawan_session_save(void) {
    	DPRINT("ALMACENANDO EN NVM EL BUFFER NONCE DE LA SESIÓN ACTUAL \r\n");

    	HAL_StatusTypeDef result = flash_save(&node);
        if (result == HAL_OK){
            DPRINT("ALMACENAMIENTO EXITOSO \r\n");
        }
        else{
        	DPRINT("NO SE HA ALMACENADO EL BUFFER NONCE DE LA SESIÓN ACTUAL. CÓDIGO DE ERROR DE HAL: %d \r\n", result);
        }
    }

    void lorawan_send(lorawan_uplink_t *uplink, lorawan_downlink_t *downlink) {

        *downlink = {};
        size_t downlink_len = 0;
        LoRaWANEvent_t evUp = {};
        LoRaWANEvent_t evDown = {};

        int16_t state = 0;
        state = node.sendReceive((const uint8_t *) uplink->payload, uplink->len, uplink->port, downlink->data, &downlink_len, uplink->confirmed, &evUp, &evDown);

        DPRINT("UPLINK ENVIADO    - PAYLOAD: %.*s; COUNTER: %lu; FREC: %lu; DR: %d; PORT: %d; CONFIRMED:  %s; POWER: %d \r\n",
        		uplink->len,
				(char*)uplink->payload,
				evUp.fCnt - 1,	// se le resta 1 porque sendRecieve suma 1 al contador fCnt luego de la transmisión
				(unsigned long)(evUp.freq * 1000000UL),
				evUp.datarate,
				evUp.fPort,
				(evUp.confirmed ? "Y" : "N"),
				evUp.power);

        if (state == RADIOLIB_ERR_NONE || state == 1 || state == 2) {
			if (downlink_len > 0) {
				downlink->len = (uint8_t)downlink_len;
				downlink->port = evDown.fPort;
				downlink->window = (state > 0) ? state : 0;
				downlink->available = true;

		        DPRINT("DOWNLINK RECIBIDO - PAYLOAD: %.*s; COUNTER: %lu; FREC: %lu; DR: %d; PORT: %d; CONFIRMING: %s; RSSI:  %d; WINDOW: %d \r\n",
		        		downlink->len,
						(char*)downlink->data,
						evDown.fCnt,
						(unsigned long)(evDown.freq * 1000000UL),
						evDown.datarate,
						downlink->port,
						(evDown.confirming ? "Y" : "N"),
						evDown.power,
						downlink->window);
			}
			else {
				DPRINT("NO SE RECIBIÓ DOWLINK \r\n");
			}
		}
        else {
			DPRINT("LA TRANSMISIÓN FALLÓ. CÓDIGO DE ERROR DE RADIOLIB: %d\n\r", state);
		}
    }
	/*
	 * %%%%%%%%%%%% FIN DE API PÚBLICA %%%%%%%%%%%%%
	*/
}

