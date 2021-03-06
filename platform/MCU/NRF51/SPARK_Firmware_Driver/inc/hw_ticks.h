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


#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The number of ticks per microsecond of the system counter.
 * SYSTEM_TICK_COUNTER
 */
#define SYSTEM_US_TICKS		100     // cycles per microsecond

/**
 * Should return a value from a system counter.
 */
#define SYSTEM_TICK_COUNTER     0

/**
 * Increment the millisecond tick counter.
 */
void System1MsTick(void);

/**
 * Increment the microsecond tick counter.
 */
void System1UsTick(void);

/**
 * Fetch the current milliseconds count.
 * @return the number of milliseconds since the device was powered on or woken up from
 * sleep. Automatically wraps around when above UINT_MAX;
 */
system_tick_t GetSystem1MsTick();

/**
 * Fetch the current microseconds count.
 * @return the number of microseconds since the device was powered on or woken up from
 * sleep. Automatically wraps around when above UINT_MAX;
 */
system_tick_t GetSystem1UsTick();


#ifdef __cplusplus
}
#endif
