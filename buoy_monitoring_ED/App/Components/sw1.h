/******************************************************************************
 * @file    sw1.h
 * @author  Tomás Agustín Scozzina
 * @date    23 jun 2026
 * @brief   [Breve descripción]
 ******************************************************************************/

#ifndef COMPONENTS_SW1_H_
#define COMPONENTS_SW1_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

void SW1_notifyPress(void);
bool SW1_getPressEv(void);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_SW1_H_ */
