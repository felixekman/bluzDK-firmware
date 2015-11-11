/**
 Copyright (c) 2015 MidAir Technology, LLC.  All rights reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation, either
 version 3 of the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public
 License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BLUETOOTH_LE_HAL_H
#define __BLUETOOTH_LE_HAL_H

#include <stdbool.h>

/* Includes ------------------------------------------------------------------*/

typedef enum {
    BLUETOOTH_LE_OFF,
    BLUETOOTH_LE_ADVERTISING,
    BLUETOOTH_LE_SLEEPING,
    BLUETOOTH_LE_CONNECTED,
} BLUETOOTH_LE_STATE;

#ifdef __cplusplus
extern "C" {
#endif
    BLUETOOTH_LE_STATE HAL_BLE_GET_STATE(void);
    void HAL_BLE_Start_Advertising(void);
    void HAL_BLE_Stop_Advertising(void);

#ifdef __cplusplus
}
#endif

#endif  /* __BLUETOOTH_LE_HAL_H */
