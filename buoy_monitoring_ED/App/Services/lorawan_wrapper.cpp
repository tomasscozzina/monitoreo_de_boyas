#include "lorawan_wrapper.h"
#include "main.h"
#include "RadioLib.h"
#include "spi.h"

/* Identificadores de pines */
#define PIN_ID_CS 					1
#define PIN_ID_RST 					2
#define PIN_ID_G0 					3

/* Defines y macros para el manejo de memoria */
#define NONCE_FLASH_PAGE   			63U
#define NONCE_FLASH_BANK   			FLASH_BANK_1
#define NONCE_FLASH_ADDR   			0x0801F800UL
#define ALIGN8(x)  					(((x) + 7U) & ~7U)
#define OFF_MAGIC   				0U
#define OFF_NONCES  				8U
#define FLASH_MAGIC  				0xAA55AA55UL
#define BUF_SIZE  					(OFF_NONCES + ALIGN8(RADIOLIB_LORAWAN_NONCES_BUF_SIZE))

/* Reintentos de comunicación por SPI */
#define LORAWAN_COMMS_REINTENTOS	5

/* Reintentos JOIN LoRaWAN */
#define LORAWAN_JOIN_REINTENTOS		10

/* Reintentos UPLINK LoRaWAN */
#define LORAWAN_UPLINK_REINTENTOS	5

/* Tamaño del payload esperado en el Downlink */
#define DOWNLINK_PAYLOAD_LEN		2

/* Puntero a la función de callback */
static void (*_g0_cb)(void) = nullptr;

class STM32Hal : public RadioLibHal {

public:
    STM32Hal() : RadioLibHal(GPIO_PIN_RESET, GPIO_PIN_SET,
                             GPIO_PIN_RESET, GPIO_PIN_SET,
							 GPIO_MODE_IT_RISING, GPIO_MODE_IT_FALLING) {}

    /* Métodos virtuales y abstractos */
    void pinMode(uint32_t pin, uint32_t mode) override {}
    void spiBegin() override {}
    void spiEnd() override {}

    /* Métodos virtuales */
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

	/* Prototipos de funciones privadas */
	void flash_restore(LoRaWANNode *node);
	void flash_save(LoRaWANNode *node);
	void lorawan_radioBegin(void);

	/* API pública */
	LoRaWAN_Status_t lorawan_init(lorawan_credentials_t* credentials) {
		LoRaWAN_Status_t ret;
		lorawan_radioBegin();
	    node.beginOTAA(credentials->joinEUI, credentials->deviceEUI, nullptr, credentials->appKey);
	    flash_restore(&node);

	    int16_t state;
	    for (uint8_t i = 0; i < LORAWAN_JOIN_REINTENTOS; i++) {
	        state = node.activateOTAA();
	    	if(state == RADIOLIB_LORAWAN_NEW_SESSION) {
	        	break;
	        }
	    	DPRINT("OTAA FALLÓ. REINTENTANDO \r\n");
	        HAL_Delay(5000);
	    }
	    /* Independientemente de si el JOIN es exitoso o no, se debe guardar en memoria los nonces actualizados */
	    flash_save(&node);

	    ret = (state == RADIOLIB_LORAWAN_NEW_SESSION) ? LORA_JOIN_OK : LORA_ERR_COMMS;
	    return ret;
	}

	LoRaWAN_Status_t lorawan_sendReceive(lorawan_uplink_t *uplink, lorawan_downlink_t *downlink) {
		size_t downlink_len = 0;
		LoRaWANEvent_t evUp = {0};
		LoRaWANEvent_t evDown = {0};

		int16_t state = 0;
		for(uint8_t i = 0; i < LORAWAN_UPLINK_REINTENTOS; i++) {
			state = node.sendReceive((const uint8_t *) uplink->payload, (size_t) uplink->len, uplink->port, downlink->data, &downlink_len, uplink->confirmed, &evUp, &evDown);
			/* Si la transmisión del Uplink falla, se reintenta LORAWAN_UPLINK_REINTENTOS veces */
			if(state >= 0) {
				break;
			}
			HAL_Delay(5000);
		}

		LoRaWAN_Status_t ret;
		if((state == 1) || (state == 2)) {
			/* Se envió Uplink y se recibió Dowlink en Rx1 o Rx2, según 'state' */
			if(downlink_len == DOWNLINK_PAYLOAD_LEN) {
				/* El Dowlink recibido contiene payload del tamaño esperado */
				downlink->available = true;
			}
			else {
				/* El Dowlink recibido NO contiene payload o tiene un tamaño distinto al esperado */
				memset(downlink, 0, sizeof(lorawan_downlink_t));
			}
			/* Contenga o no payload, se evalua la flag confirming */
			downlink->confirming = evDown.confirming;
			ret = LORA_DOWN_AVAILABLE;
		}
		else {
			/* Se envió Uplink y NO se recibió Dowlink, o se produjo un error en la comunicación */
			memset(downlink, 0, sizeof(lorawan_downlink_t));
			(state == RADIOLIB_ERR_NONE) ? ret = LORA_UP_OK : ret = LORA_ERR_COMMS;
		}
		return ret;
	}

	void lorawan_sleep(void) {
		radio.sleep();
	}

	void lorawan_setADR(bool enable) {
		node.setADR(enable);
	}

	void lorawan_setDataRate(uint8_t dr) {
		node.setDatarate(dr);
	}

	int8_t lorawan_getSNR(void) {
		return ((int8_t)radio.getSNR());
	}

	int8_t lorawan_getRSSI(void) {
		return ((int8_t)radio.getRSSI());
	}

	void RFM95W_notifyG0(void) {
		if (_g0_cb)
			_g0_cb();
	}

	/* Definiciones de funciones privadas */
	void lorawan_radioBegin(void) {
		for (uint8_t i = 0; i < LORAWAN_COMMS_REINTENTOS; i++) {
			/* Inicializo el módulo con parámetros pertenecientes a la sub-banda 2 de AU915 */
			if(radio.begin(915.2, 125.0, 9, 7, RADIOLIB_SX127X_SYNC_WORD_LORAWAN) == RADIOLIB_ERR_NONE) {
				return;
			}
			HAL_Delay(100);
			DPRINT("PRIMER REINTENTO BEGIN N° %d \n\r", i);
		}
		// Si luego de 5 intentos, la inicialización sigue fallando, se reinicia la interfaz SPI
		HAL_SPI_DeInit(&hspi1);
		HAL_SPI_Init(&hspi1);

		// Se repiten los 5 intentos
		for (uint8_t i = 0; i < LORAWAN_COMMS_REINTENTOS; i++) {
			/* Inicializo el módulo con parámetros pertenecientes a la sub-banda 2 de AU915 */
			if(radio.begin(915.2, 125.0, 9, 7, RADIOLIB_SX127X_SYNC_WORD_LORAWAN) == RADIOLIB_ERR_NONE) {
				return;
			}
			HAL_Delay(100);
			DPRINT("SEGUNDO REINTENTO BEGIN N° %d \n\r", i);
		}
		// Luego de los 10 intentos fallidos (habiendo reiniciado la interfaz de por medio) se llama al Error_Handler() para reiniciar el sistema
		Error_Handler();
	}

	void flash_restore(LoRaWANNode *node) {
		const uint8_t *base = (const uint8_t *)NONCE_FLASH_ADDR;

		uint32_t magic;
		memcpy(&magic, base + OFF_MAGIC, sizeof(magic));
		if (magic != FLASH_MAGIC) {
			return;
		}
		/* Para borrar los nonces de la flash, ejecutar "monitor flash mass_erase" en la Debugger Console, durante un DEBUG */
		node->setBufferNonces(base + OFF_NONCES);
		return;
	}

	void flash_save(LoRaWANNode *node) {
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
		if (HAL_FLASHEx_Erase(&erase, &pageError) != HAL_OK) {
			HAL_FLASH_Lock();
			return;
		}

		for (size_t i = 0; i < sizeof(buf); i += 8){
			uint64_t word;
			memcpy(&word, buf + i, 8);
			if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, NONCE_FLASH_ADDR + i, word) != HAL_OK) {
				HAL_FLASH_Lock();
				return;
			}
		}
		HAL_FLASH_Lock();
		return;
	}
} // Fin del extern C
