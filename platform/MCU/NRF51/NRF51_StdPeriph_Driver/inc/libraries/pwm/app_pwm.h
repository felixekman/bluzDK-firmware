/*
 * Copyright (c) Nordic Semiconductor ASA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 *   3. Neither the name of Nordic Semiconductor ASA nor the names of other
 *   contributors to this software may be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/** @file
 *
 * @defgroup app_pwm Pulse-width modulation (PWM)
 * @{
 * @ingroup app_common
 *
 * @brief Module for generating a pulse-width modulated output signal.
 *
 * @details This module provides a PWM implementation using timers, GPIOTE, and PPI.
 *
 * Each PWM instance utilizes 1 timer, 2 PPI channels, and 1 PPI channel group
 * plus 2 PPI and 1 GPIOTE channels per PWM channel. The maximum number of PWM
 * channels per instance is 2.
 */

#ifndef APP_PWM_H__
#define APP_PWM_H__

#include <stdint.h>
#include "sdk_errors.h"
#include "nrf_drv_timer.h"
#include "nrf_drv_common.h"
#include "nrf_drv_ppi.h"


#define APP_PWM_NOPIN                 0xFFFFFFFF

/** @brief Number of channels for one timer instance (fixed to 2 due to timer properties).*/
#define APP_PWM_CHANNELS_PER_INSTANCE 2

/**@brief Macro for creating a PWM instance. */
#define APP_PWM_INSTANCE(name, num)                                           \
    const nrf_drv_timer_t m_pwm_##name##_timer = NRF_DRV_TIMER_INSTANCE(num); \
    app_pwm_cb_t m_pwm_##name##_cb;                                           \
    /*lint -e{545}*/                                                          \
    const app_pwm_t name = {                                                  \
        .p_cb    = &m_pwm_##name##_cb,                                        \
        .p_timer = &m_pwm_##name##_timer,                                     \
    }


/**@brief PWM instance default configuration (1 channel). */
#define APP_PWM_DEFAULT_CONFIG_1CH(period_in_us, pin)                                  \
    {                                                                                  \
        .pins            = {pin, APP_PWM_NOPIN},                                       \
        .pin_polarity    = {APP_PWM_POLARITY_ACTIVE_LOW, APP_PWM_POLARITY_ACTIVE_LOW}, \
        .num_of_channels = 1,                                                          \
        .period_us       = period_in_us                                                \
    }

/**@brief PWM instance default configuration (2 channels). */
#define APP_PWM_DEFAULT_CONFIG_2CH(period_in_us, pin0, pin1)                           \
    {                                                                                  \
        .pins            = {pin0, pin1},                                               \
        .pin_polarity    = {APP_PWM_POLARITY_ACTIVE_LOW, APP_PWM_POLARITY_ACTIVE_LOW}, \
        .num_of_channels = 2,                                                          \
        .period_us       = period_in_us                                                \
    }

typedef uint16_t app_pwm_duty_t;

/**
 * @brief PWM callback that is executed when a PWM duty change has been completed.
 *
 * @param[in] pwm_id  PWM instance ID.
 */
typedef void (* app_pwm_callback_t)(uint32_t);

/**
 * @brief Channel polarity.
 */
typedef enum
{
    APP_PWM_POLARITY_ACTIVE_LOW  = 0,
    APP_PWM_POLARITY_ACTIVE_HIGH = 1
} app_pwm_polarity_t;

/**@brief PWM configuration structure used for initialization. */
typedef struct
{
    uint32_t           pins[APP_PWM_CHANNELS_PER_INSTANCE];         //!< Pins configured as PWM output.
    app_pwm_polarity_t pin_polarity[APP_PWM_CHANNELS_PER_INSTANCE]; //!< Polarity of active state on pin.
    uint32_t           num_of_channels;                             //!< Number of channels that can be used.
    uint32_t           period_us;                                   //!< PWM signal output period to configure (in microseconds).
} app_pwm_config_t;


/**
 * @defgroup app_pwm_internal Auxiliary internal types declarations
 * @{
 * @internal
 *
 * @brief Module for internal usage inside the library only
 *
 * There are some definitions that must be included in the header file because 
 * of the way the library is set up. In this way, the are accessible to the user.
 * However, any functions and variables defined here may change at any time 
 * without a warning, so you should not access them directly.
 */

    /**
     * @brief PWM channel instance
     *
     * This structure holds all data needed by a single PWM channel.
     */
    typedef struct
    {
        uint32_t           gpio_pin;        //!< Pin that is used by this PWM channel.
        uint32_t           pulsewidth;      //!< The copy of duty currently set (in ticks).
        nrf_ppi_channel_t  ppi_channels[2]; //!< PPI channels used by the PWM channel to clear and set the output.
        app_pwm_polarity_t polarity;        //!< The active state of the pin.
        uint8_t            initialized;     //!< The internal information if the selected channel was initialized.
    } app_pwm_channel_cb_t;

    /**
     * @brief Variable part of PWM instance
     *
     * This structure holds instance data that may change.
     */
    typedef struct
    {
        app_pwm_channel_cb_t    channels_cb[APP_PWM_CHANNELS_PER_INSTANCE]; //!< Channels data
        uint32_t                period;                                     //!< Selected period in ticks
        app_pwm_callback_t      p_ready_callback;                           //!< Callback function called on PWM readiness
        nrf_ppi_channel_t       ppi_channels[2];                            //!< PPI channels used temporary while changing duty
        nrf_ppi_channel_group_t ppi_group;                                  //!< PPI group used to synchronize changes on channels
        nrf_drv_state_t         state;                                      //!< Current driver status
    } app_pwm_cb_t;
/** @} */


/**@brief PWM instance structure. */
typedef struct
{
    app_pwm_cb_t *p_cb;                    //!< Pointer to control block internals.
    nrf_drv_timer_t const * const p_timer; //!< Timer used by this PWM instance.
} app_pwm_t;

/**
 * @brief Function for initializing a PWM instance.
 *
 * @param[in] p_instance        PWM instance.
 * @param[in] p_config          Initial configuration.
 * @param[in] p_ready_callback  Pointer to ready callback function (or NULL to disable).
 *
 * @retval    NRF_SUCCESS If initialization was successful.
 * @retval    NRF_ERROR_NO_MEM If there were not enough free resources.
 * @retval    NRF_ERROR_INVALID_PARAM If an invalid configuration structure was passed.
 * @retval    NRF_ERROR_INVALID_STATE If the timer/PWM is already in use or if initialization failed.
 */
ret_code_t app_pwm_init(app_pwm_t const * const p_instance, app_pwm_config_t const * const p_config,
                        app_pwm_callback_t p_ready_callback);


/**
 * @brief Function for uninitializing a PWM instance and releasing the allocated resources.
 *
 * @param[in] p_instance  PWM instance.
 *
 * @retval    NRF_SUCCESS If uninitialization was successful.
 * @retval    NRF_ERROR_INVALID_STATE If the given instance was not initialized.
 */
ret_code_t app_pwm_uninit(app_pwm_t const * const p_instance);

/**
 * @brief Function for enabling a PWM instance after initialization.
 *
 * @param[in] p_instance  PWM instance.
 *
 * @retval    NRF_SUCCESS If the operation was successful.
 * @retval    NRF_ERROR_INVALID_STATE If the given instance was not initialized.
 */
void app_pwm_enable(app_pwm_t const * const p_instance);

/**
 * @brief Function for stopping a PWM instance after initialization.
 *
 * @param[in] p_instance  PWM instance.
 *
 * @retval    NRF_SUCCESS If the operation was successful.
 * @retval    NRF_ERROR_INVALID_STATE If the given instance was not initialized.
 */
void app_pwm_disable(app_pwm_t const * const p_instance);

/**
 * @brief Function for setting the PWM channel duty cycle in percents.
 *
 * A duty cycle change requires one full PWM clock period to finish.
 * If another change is attempted for any channel of given instance before
 * the current change is complete, the new attempt will result in the error
 * NRF_ERROR_BUSY.
 *
 * @param[in] p_instance  PWM instance.
 * @param[in] channel     Channel number.
 * @param[in] duty        Duty cycle (0 - 100).
 *
 * @retval    NRF_SUCCESS If the operation was successful.
 * @retval    NRF_ERROR_BUSY If the PWM is not ready yet.
 * @retval    NRF_ERROR_INVALID_STATE If the given instance was not initialized.
 *
 */
ret_code_t app_pwm_channel_duty_set(app_pwm_t const * const p_instance,
                                  uint8_t channel, app_pwm_duty_t duty);

/**
 * @brief Function for retrieving the PWM channel duty cycle in percents.
 *
 * @param[in] p_instance  PWM instance.
 * @param[in] channel     Channel number.
 *
 * @return    Duty cycle value.
 */
app_pwm_duty_t app_pwm_channel_duty_get(app_pwm_t const * const p_instance, uint8_t channel);


/**
 * @name Functions accessing values in ticks
 *
 * Auxiliary functions that allow to get values in actual timer ticks.
 * @{
 */

    /**
     * @brief Function for setting PWM channel duty cycle in clock ticks.
     *
     * @note      Duty cycle changes require one full PWM clock period to finish.
     *            Until that, the next change attempt (for any channel of given instance)
     *            will result in an NRF_ERROR_BUSY error.
     *
     * @param[in] p_instance  PWM instance.
     * @param[in] channel     Channel number.
     * @param[in] ticks       Number of PWM clock ticks.
     *
     * @retval    NRF_SUCCESS If the operation was successful.
     * @retval    NRF_ERROR_BUSY If PWM is not ready yet.
     * @retval    NRF_ERROR_INVALID_STATE If the given instance was not initialized.
     */
    ret_code_t app_pwm_channel_duty_ticks_set(app_pwm_t const * const p_instance,
                                              uint8_t           channel,
                                              uint16_t          ticks);


    /**
     * @brief Function for retrieving the PWM channel duty cycle in ticks.
     *
     * This function retrieves real, currently set duty in ticks.
     * For one full PWM cycle the value might be different than the value set by the last
     * @ref app_pwm_channel_duty_ticks_set function call.
     *
     * @param[in] p_instance  PWM instance.
     * @param[in] channel     Channel number.
     *
     * @return    Number of ticks set for selected channel.
     *
     */
    uint16_t app_pwm_channel_duty_ticks_get(app_pwm_t const * const p_instance, uint8_t channel);

    /**
     * @brief Function for returning the number of ticks in a whole cycle.
     *
     * @param[in] p_instance  PWM instance.
     *
     * @return    Number of ticks that corresponds to 100% of the duty cycle.
     */
    uint16_t app_pwm_cycle_ticks_get(app_pwm_t const * const p_instance);
/** @} */


#endif

/** @} */
