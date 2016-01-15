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

#ifndef __NRF_HW_LAYOUT_H
#define __NRF_HW_LAYOUT_H

#define BUTTON_PULL    NRF_GPIO_PIN_PULLUP

//Board LED's and buttons
#define RGB_LED_PIN_RED          		19                                       /**< Pin one for the RGB LED. */
#define RGB_LED_PIN_GREEN          		18                                       /**< Pin one for the RGB LED. */
#define RGB_LED_PIN_BLUE          		17                                       /**< Pin one for the RGB LED. */
#define BOARD_BUTTON          			7                                        /**< Main LED on the board. */

//Flash SPI address
#define SPIM1_SCK_PIN       10u     /**< SPI clock GPIO pin number. */
#define SPIM1_MOSI_PIN      9u     /**< SPI Master Out Slave In GPIO pin number. */
#define SPIM1_MISO_PIN      14u     /**< SPI Master In Slave Out GPIO pin number. */
#define SPIM1_SS_PIN        15u     /**< SPI Slave Select GPIO pin number. */

//SPI Pins
#define SPIM0_SCK_PIN   3u
#define SPIM0_MOSI_PIN  1u
#define SPIM0_MISO_PIN  2u
#define SPIM0_SS_PIN    4u

//Bootloader section
#define BOOTLOADER_FLASH_PAGES 0x00

#ifndef USER_FIRMWARE_IMAGE_LOCATION
#define USER_FIRMWARE_IMAGE_LOCATION 0x00037000
#endif

#define APP_START_MASK              ((uint32_t)0x2FF10000)

//External FLash addresses
#define FLASH_FW_STATUS 0x0100
#define FLASH_FW_LENGTH1 0x0101
#define FLASH_FW_LENGTH2 0x0102
#define FLASH_FW_LENGTH3 0x0103

#define FLASH_DEVICE_INT 0x1000

#define FACTORY_RESET_FW_ADDRESS 0x0004000
#define FLASH_FW_ADDRESS 0x021000

#define FACTORY_RESET_FW_SIZE (int32_t)(FLASH_FW_ADDRESS - FACTORY_RESET_FW_ADDRESS)

#define FLASH_LENGTH 0x040000

#endif