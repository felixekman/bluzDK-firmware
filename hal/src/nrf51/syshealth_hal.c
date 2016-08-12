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

#include "syshealth_hal.h"
#include "hw_config.h"

eSystemHealth sys_health_cache;

void HAL_Set_Sys_Health(eSystemHealth health) {
    if (health>sys_health_cache)
    {
        sys_health_cache = health;
    }
}

eSystemHealth HAL_Get_Sys_Health() {
    return sys_health_cache;
}


