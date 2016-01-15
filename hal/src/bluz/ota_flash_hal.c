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

#include "flash.h"
#include "ota_flash_hal.h"
#undef STATIC_ASSERT
#include "hw_config.h"

#if MODULAR_FIRMWARE
const module_bounds_t module_bootloader = { 0x4000, 0x3C000, 0x40000, MODULE_FUNCTION_BOOTLOADER, 0, MODULE_STORE_MAIN };
const module_bounds_t module_system_part1 = { 0x1F000, 0x18000, 0x37000, MODULE_FUNCTION_SYSTEM_PART, 1, MODULE_STORE_MAIN };
const module_bounds_t module_user = { 0x5000, 0x37000, 0x3C000, MODULE_FUNCTION_USER_PART, 2, MODULE_STORE_MAIN};
const module_bounds_t module_factory = { 0x1F000, 0x1021000, 0x1040000, MODULE_FUNCTION_USER_PART, 1, MODULE_STORE_FACTORY};
const module_bounds_t* module_bounds[] = { &module_bootloader, &module_system_part1, &module_user, &module_factory };
const module_bounds_t module_ota = { 0x1D000, 0x1004000, 0x1021000, MODULE_FUNCTION_NONE, 0, MODULE_STORE_SCRATCHPAD};

#else
const module_bounds_t module_bootloader = { 0x4000, 0x3C000, 0x40000, MODULE_FUNCTION_BOOTLOADER, 0, MODULE_STORE_MAIN};
const module_bounds_t module_user = { 0x24000, 0x18000, 0x3C000, MODULE_FUNCTION_MONO_FIRMWARE, 0, MODULE_STORE_MAIN};
const module_bounds_t module_factory = { 0x1F000, 0x1021000, 0x1040000, MODULE_FUNCTION_MONO_FIRMWARE, 0, MODULE_STORE_FACTORY};
const module_bounds_t* module_bounds[] = { &module_bootloader, &module_user, &module_factory };

const module_bounds_t module_ota = { 0x1D000, 0x1004000, 0x1021000, MODULE_FUNCTION_NONE, 0, MODULE_STORE_SCRATCHPAD};
#endif

const unsigned module_bounds_length = 4;
void HAL_OTA_Add_System_Info(hal_system_info_t* info, bool create, void* reserved);

/**
 * Finds the location where a given module is stored. The module is identified
 * by it's funciton and index.
 * @param module_function   The function of the module to find.
 * @param module_index      The function index of the module to find.
 * @return the module_bounds corresponding to the module, NULL when not found.
 */
const module_bounds_t* find_module_bounds(uint8_t module_function, uint8_t module_index)
{
    for (unsigned i=0; i<module_bounds_length; i++) {
        if (module_bounds[i]->module_function==module_function && module_bounds[i]->module_index==module_index)
            return module_bounds[i];
    }
    return NULL;
}

/**
 * Determines if a given address is in range.
 * @param test      The address to test
 * @param start     The start range (inclusive)
 * @param end       The end range (inclusive)
 * @return {@code true} if the address is within range.
 */
inline bool in_range(uint32_t test, uint32_t start, uint32_t end)
{
    return test>=start && test<=end;
}


/**
 * Find the module_info at a given address. No validation is done so the data
 * pointed to should not be trusted.
 * @param bounds
 * @return
 */
const module_info_t* locate_module(const module_bounds_t* bounds) {
    return FLASH_ModuleInfo(FLASH_INTERNAL, bounds->start_address);
}

bool validate_module_dependencies(const module_bounds_t* bounds, bool userOptional)
{
    bool valid = false;
    const module_info_t* module = locate_module(bounds);
    if (module)
    {
        if (module->dependency.module_function == MODULE_FUNCTION_NONE || (userOptional && module_function(module)==MODULE_FUNCTION_USER_PART)) {
            valid = true;
        }
        else {
            // deliberately not transitive, so we only check the first dependency
            // so only user->system_part_2 is checked
            const module_bounds_t* dependency_bounds = find_module_bounds(module->dependency.module_function, module->dependency.module_index);
            const module_info_t* dependency = locate_module(dependency_bounds);
            valid = dependency && (dependency->module_version>=module->dependency.module_version);
        }
    }
    return valid;
}


/**
 * Fetches and validates the module info found at a given location.
 * @param target        Receives the module into
 * @param bounds        The location where to retrieve the module from.
 * @param userDepsOptional
 * @return {@code true} if the module info can be read via the info, crc, suffix pointers.
 */
bool fetch_module(hal_module_t* target, const module_bounds_t* bounds, bool userDepsOptional, uint16_t check_flags)
{
    memset(target, 0, sizeof(*target));
    
    target->bounds = *bounds;
    if (NULL!=(target->info = locate_module(bounds)))
    {
        target->validity_checked = MODULE_VALIDATION_RANGE | MODULE_VALIDATION_DEPENDENCIES | MODULE_VALIDATION_PLATFORM | check_flags;
        target->validity_result = 0;
        const uint8_t* module_end = (const uint8_t*)target->info->module_end_address;
        const module_bounds_t* expected_bounds = find_module_bounds(module_function(target->info), module_index(target->info));
        if (expected_bounds && in_range((uint32_t)module_end, expected_bounds->start_address, expected_bounds->end_address)) {
            target->validity_result |= MODULE_VALIDATION_RANGE;
            target->validity_result |= (PLATFORM_ID==module_platform_id(target->info)) ? MODULE_VALIDATION_PLATFORM : 0;
            // the suffix ends at module_end, and the crc starts after module end
            target->crc = (module_info_crc_t*)module_end;
            target->suffix = (module_info_suffix_t*)(module_end-sizeof(module_info_suffix_t));
            if (validate_module_dependencies(bounds, userDepsOptional))
                target->validity_result |= MODULE_VALIDATION_DEPENDENCIES;
            if ((target->validity_checked & MODULE_VALIDATION_INTEGRITY) && FLASH_VerifyCRC32(FLASH_INTERNAL, bounds->start_address, module_length(target->info)))
                target->validity_result |= MODULE_VALIDATION_INTEGRITY;
        }
        else
            target->info = NULL;
    }
    return target->info!=NULL;
}


void HAL_System_Info(hal_system_info_t* info, bool construct, void* reserved)
{
    if (construct) {
        info->platform_id = PLATFORM_ID;
        // bootloader, system 1, system 2, optional user code and factory restore
        uint8_t count = module_bounds_length;
        info->modules = malloc(module_bounds_length * sizeof(hal_module_t));
        if (info->modules) {
            info->module_count = count;
            for (unsigned i=0; i<count; i++) {
                fetch_module(info->modules+i, module_bounds[i], false, MODULE_VALIDATION_INTEGRITY);
            }
        }
    }
    else
    {
        free(info->modules);
        info->modules = NULL;
    }
    HAL_OTA_Add_System_Info(info, construct, reserved);
    
}

void HAL_OTA_Add_System_Info(hal_system_info_t* info, bool create, void* reserved)
{
    // presently no additional key/value pairs to send back
    info->key_values = NULL;
    info->key_value_count = 0;
}

uint32_t HAL_OTA_FlashAddress()
{
    return FLASH_FW_ADDRESS;
}

uint32_t HAL_OTA_FlashLength()
{
    return (int32_t)(FLASH_LENGTH - FLASH_FW_ADDRESS);
}

uint16_t HAL_OTA_ChunkSize()
{
    return 512;
}

uint16_t HAL_OTA_SessionTimeout()
{
    return 0;
}

bool HAL_FLASH_Begin(uint32_t address, uint32_t length, void* reserved)
{
    FLASH_Begin(address, length);
    return true;
}

int HAL_FLASH_Update(const uint8_t *pBuffer, uint32_t address, uint32_t length, void* reserved)
{
    return FLASH_Update(pBuffer, address, length);
}

hal_update_complete_t HAL_FLASH_End(void* reserved)
{
    FLASH_End();
    return HAL_UPDATE_APPLIED_PENDING_RESTART;
}

void HAL_FLASH_Read_ServerAddress(ServerAddress* server_addr)
{
}


bool HAL_OTA_Flashed_GetStatus(void)
{
    return false;
}

void HAL_OTA_Flashed_ResetStatus(void)
{
}

void HAL_FLASH_Read_ServerPublicKey(uint8_t *keyBuffer)
{
    //TO DO: Well, clearly this needs to change....
    
    //particle staging server
//    unsigned char pubkey[EXTERNAL_FLASH_SERVER_PUBLIC_KEY_LENGTH] = {0x30, 0x82, 0x01, 0x22, 0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0F, 0x00, 0x30, 0x82, 0x01, 0x0A, 0x02, 0x82, 0x01, 0x01, 0x00, 0xA6, 0x99, 0xCD, 0x40, 0x24, 0xEB, 0x90, 0xDE, 0x0F, 0xC1, 0x52, 0x91, 0x27, 0xD1, 0x63, 0x75, 0x05, 0x78, 0x5C, 0x26, 0x36, 0xC5, 0x4A, 0x7E, 0x33, 0x42, 0x89, 0x95, 0xFC, 0x75, 0x9C, 0xDB, 0xDE, 0xBA, 0xD3, 0xDD, 0x49, 0x81, 0xE6, 0x8E, 0x3E, 0xE5, 0x0F, 0x0B, 0xC4, 0x2B, 0x63, 0x8F, 0x5A, 0xB3, 0x3D, 0x8A, 0x2D, 0x9F, 0xF0, 0x1D, 0x1E, 0xE4, 0x62, 0xC7, 0x89, 0x2A, 0x49, 0xDA, 0x83, 0x56, 0x90, 0xE2, 0x0D, 0x8B, 0x64, 0x63, 0x95, 0xAC, 0x63, 0x97, 0x59, 0x4E, 0x9C, 0xA3, 0xEB, 0xF7, 0x43, 0x4A, 0x9F, 0xC0, 0x33, 0xF9, 0x56, 0xD9, 0xD5, 0xDF, 0x5A, 0x6C, 0x7D, 0x6F, 0x8F, 0xB3, 0x7C, 0x89, 0x79, 0xBC, 0x00, 0xF1, 0x51, 0x18, 0x8D, 0x6B, 0x80, 0x22, 0xF7, 0x40, 0x8D, 0xA9, 0x20, 0x54, 0xC7, 0xA2, 0x4C, 0x52, 0x8D, 0x39, 0x94, 0x9F, 0xA8, 0x74, 0x9F, 0x8F, 0x4D, 0xD9, 0xAD, 0x88, 0xC8, 0xF5, 0x53, 0x18, 0x4A, 0x7E, 0x0E, 0x58, 0xD7, 0x47, 0xB0, 0xA9, 0x33, 0x9F, 0x7C, 0xEC, 0xBD, 0x7D, 0xF2, 0xAA, 0x6B, 0x61, 0x17, 0xFF, 0x0C, 0xFD, 0xFC, 0x67, 0xDB, 0x24, 0x79, 0xF5, 0x48, 0x8C, 0x9C, 0x9A, 0x95, 0x41, 0x08, 0x1D, 0x84, 0x06, 0x4B, 0xF1, 0xE6, 0x21, 0xB2, 0xF8, 0x3F, 0xD7, 0xF1, 0x0C, 0xAF, 0xC1, 0xC8, 0x4A, 0x4E, 0x49, 0x93, 0xA4, 0x99, 0xF2, 0xAC, 0x1E, 0xF1, 0x08, 0x41, 0xAC, 0xEA, 0x44, 0x64, 0x56, 0xA5, 0x4E, 0xF5, 0x1D, 0x99, 0x8C, 0x76, 0xEA, 0x79, 0x30, 0x65, 0x75, 0x1C, 0xA6, 0x1D, 0x26, 0xF7, 0xC5, 0x73, 0x3C, 0x47, 0x47, 0x4B, 0xEB, 0x68, 0x26, 0xE3, 0xB9, 0xCD, 0x97, 0xF0, 0x9C, 0x2B, 0xE3, 0xFB, 0x90, 0xD1, 0xD4, 0xB8, 0x6A, 0x85, 0x78, 0x1C, 0xCD, 0x2E, 0xD2, 0xD0, 0xC3, 0x78, 0x9E, 0x14, 0xF5, 0x02, 0x03, 0x01, 0x00, 0x01};
    
    //public server key
    unsigned char pubkey[EXTERNAL_FLASH_SERVER_PUBLIC_KEY_LENGTH] = {0x30, 0x82, 0x01, 0x22, 0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0F, 0x00, 0x30, 0x82, 0x01, 0x0A, 0x02, 0x82, 0x01, 0x01, 0x00, 0xBE, 0xCC, 0xBE, 0x43, 0xDB, 0x8E, 0xEA, 0x15, 0x27, 0xA6, 0xBB, 0x52, 0x6D, 0xE1, 0x51, 0x2B, 0xA0, 0xAB, 0xCC, 0xA1, 0x64, 0x77, 0x48, 0xAD, 0x7C, 0x66, 0xFC, 0x80, 0x7F, 0xF6, 0x99, 0xA5, 0x25, 0xF2, 0xF2, 0xDA, 0xE0, 0x43, 0xCF, 0x3A, 0x26, 0xA4, 0x9B, 0xA1, 0x87, 0x03, 0x0E, 0x9A, 0x8D, 0x23, 0x9A, 0xBC, 0xEA, 0x99, 0xEA, 0x68, 0xD3, 0x5A, 0x14, 0xB1, 0x26, 0x0F, 0xBD, 0xAA, 0x6D, 0x6F, 0x0C, 0xAC, 0xC4, 0x77, 0x2C, 0xD1, 0xC5, 0xC8, 0xB1, 0xD1, 0x7B, 0x68, 0xE0, 0x25, 0x73, 0x7B, 0x52, 0x89, 0x68, 0x20, 0xBD, 0x06, 0xC6, 0xF0, 0xE6, 0x00, 0x30, 0xC0, 0xE0, 0xCF, 0xF6, 0x1B, 0x3A, 0x45, 0xE9, 0xC4, 0x5B, 0x55, 0x17, 0x06, 0xA3, 0xD3, 0x4A, 0xC6, 0xD5, 0xB8, 0xD2, 0x17, 0x02, 0xB5, 0x27, 0x7D, 0x8D, 0xE4, 0xD4, 0x7D, 0xD3, 0xED, 0xC0, 0x1D, 0x8A, 0x7C, 0x25, 0x1E, 0x21, 0x4A, 0x51, 0xAE, 0x57, 0x06, 0xDD, 0x60, 0xBC, 0xA1, 0x34, 0x90, 0xAA, 0xCC, 0x09, 0x9E, 0x3B, 0x3A, 0x41, 0x4C, 0x3C, 0x9D, 0xF3, 0xFD, 0xFD, 0xB7, 0x27, 0xC1, 0x59, 0x81, 0x98, 0x54, 0x60, 0x4A, 0x62, 0x7A, 0xA4, 0x9A, 0xBF, 0xDF, 0x92, 0x1B, 0x3E, 0xFC, 0xA7, 0xE4, 0xA4, 0xB3, 0x3A, 0x9A, 0x5F, 0x57, 0x93, 0x8E, 0xEB, 0x19, 0x64, 0x95, 0x22, 0x4A, 0x2C, 0xD5, 0x60, 0xF5, 0xF9, 0xD0, 0x03, 0x50, 0x83, 0x69, 0xC0, 0x6B, 0x53, 0xF0, 0xF0, 0xDA, 0xF8, 0x13, 0x82, 0x1F, 0xCC, 0xBB, 0x5F, 0xE2, 0xC1, 0xDF, 0x3A, 0xE9, 0x7F, 0x5D, 0xE2, 0x7D, 0xB9, 0x50, 0x80, 0x3C, 0x58, 0x33, 0xEF, 0x8C, 0xF3, 0x80, 0x3F, 0x11, 0x01, 0xD2, 0x68, 0x86, 0x5F, 0x3C, 0x5E, 0xE6, 0xC1, 0x8E, 0x32, 0x2B, 0x28, 0xCB, 0xB5, 0xCC, 0x1B, 0xA8, 0x50, 0x5E, 0xA7, 0x0D, 0x02, 0x03, 0x01, 0x00, 0x01};
    
    //local server key
//    unsigned char pubkey[EXTERNAL_FLASH_SERVER_PUBLIC_KEY_LENGTH] = {0x30, 0x82, 0x01, 0x22, 0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0F, 0x00, 0x30, 0x82, 0x01, 0x0A, 0x02, 0x82, 0x01, 0x01, 0x00, 0xC1, 0xC7, 0xC8, 0x50, 0xA1, 0xE5, 0x53, 0x5D, 0x2C, 0xB5, 0xFA, 0x3A, 0x82, 0xA4, 0x33, 0x06, 0x14, 0x1E, 0x5B, 0x0B, 0x83, 0xAD, 0xF2, 0xF4, 0x41, 0x5D, 0xBD, 0x60, 0x61, 0x60, 0x06, 0xA5, 0x93, 0x94, 0x7B, 0xEB, 0x85, 0x8C, 0x82, 0x1D, 0x25, 0xA7, 0x47, 0x0D, 0x41, 0x98, 0x67, 0xB7, 0x46, 0x4C, 0x62, 0x3D, 0x6F, 0x03, 0x3C, 0x0A, 0xE1, 0x73, 0x2F, 0x1E, 0x64, 0x21, 0x3B, 0xE0, 0x66, 0x2B, 0x95, 0x0B, 0x61, 0xD7, 0xC2, 0x1F, 0x5B, 0x4A, 0xDF, 0x79, 0xEC, 0xDD, 0x5C, 0x91, 0xF2, 0xF7, 0xC0, 0x4E, 0xA5, 0x6B, 0x05, 0x62, 0x73, 0xBE, 0x47, 0x58, 0x47, 0xDB, 0xCA, 0x6A, 0x7C, 0xF3, 0x56, 0x61, 0xFD, 0x37, 0xE2, 0x74, 0xF6, 0xCA, 0x65, 0x68, 0x9E, 0xAD, 0xB0, 0x87, 0x92, 0xF6, 0xE4, 0x56, 0x8D, 0xF0, 0xD8, 0x9B, 0x1F, 0x0D, 0x68, 0xA2, 0xB5, 0x81, 0xA8, 0x24, 0x00, 0x54, 0x4F, 0xFD, 0x3C, 0xB9, 0x69, 0x45, 0x73, 0x3A, 0xF4, 0x6C, 0x42, 0x90, 0xD9, 0x71, 0xEF, 0x0C, 0x32, 0xC5, 0x9B, 0x5E, 0x76, 0x67, 0x6F, 0xE2, 0x70, 0xA8, 0x34, 0xE7, 0xCF, 0xD0, 0xEE, 0x07, 0x99, 0xA2, 0x11, 0x65, 0xFE, 0x89, 0xEC, 0xA9, 0x17, 0xD1, 0x63, 0x4E, 0x1C, 0xFA, 0x6B, 0xBD, 0x37, 0x5C, 0xAE, 0x07, 0x65, 0x76, 0x26, 0xDD, 0xAF, 0xF7, 0xE0, 0x81, 0x20, 0x76, 0xAB, 0xC0, 0x5F, 0x8D, 0xA3, 0x15, 0xD8, 0xB2, 0x06, 0x05, 0xF3, 0x7A, 0x73, 0x6A, 0x31, 0xE7, 0xF5, 0x9D, 0x9E, 0xB1, 0x89, 0x81, 0xF8, 0x25, 0x54, 0xF1, 0xD8, 0x2B, 0x08, 0x42, 0x9F, 0xB4, 0xF9, 0x82, 0xD5, 0xFB, 0x0E, 0xE3, 0x54, 0x8B, 0xD4, 0x01, 0xA9, 0x6C, 0x58, 0x8C, 0x1E, 0xEF, 0xDE, 0xD5, 0xF1, 0xD6, 0x41, 0x6B, 0x9C, 0xFD, 0x4E, 0xF1, 0x6E, 0x69, 0x5F, 0x11, 0xEE, 0x9B, 0x02, 0x03, 0x01, 0x00, 0x01};
    
    memcpy(keyBuffer, pubkey, EXTERNAL_FLASH_SERVER_PUBLIC_KEY_LENGTH);
}

int HAL_FLASH_Read_CorePrivateKey(uint8_t *keyBuffer, private_key_generation_t* generation)
{
    //TO DO: ...as does this....
    unsigned char private_key[EXTERNAL_FLASH_CORE_PRIVATE_KEY_LENGTH] = {0x30, 0x82, 0x02, 0x5E, 0x02, 0x01, 0x00, 0x02, 0x81, 0x81, 0x00, 0xE3, 0x80, 0xED, 0xE4, 0xED, 0xEC, 0x5D, 0x60, 0x00, 0x4E, 0xF1, 0x2E, 0x39, 0x3C, 0x61, 0x48, 0x08, 0xAC, 0xC8, 0x9B, 0x4C, 0x41, 0x0A, 0xB9, 0x23, 0xED, 0xBE, 0xC1, 0xE9, 0x1C, 0x13, 0x93, 0xD0, 0xBE, 0x9B, 0x94, 0x0C, 0x8A, 0xD1, 0x59, 0xE7, 0xE9, 0xFE, 0xC3, 0x3D, 0x48, 0xD0, 0x46, 0x55, 0x3D, 0x9B, 0x0A, 0x03, 0x03, 0xAD, 0x18, 0x72, 0x75, 0xBC, 0x4A, 0xAA, 0x2B, 0x94, 0x82, 0x36, 0x6F, 0x3E, 0xB0, 0x04, 0x20, 0xC2, 0xFD, 0x5D, 0xDA, 0x07, 0x00, 0x37, 0x6A, 0x41, 0x32, 0xC8, 0x47, 0xA4, 0xBF, 0x77, 0xEC, 0x69, 0x4C, 0x6C, 0xDA, 0xCF, 0x49, 0x6A, 0xCF, 0x4E, 0x07, 0x6F, 0x16, 0xA3, 0x3B, 0xCF, 0xE6, 0x42, 0x30, 0x90, 0xB5, 0xDA, 0x55, 0x5A, 0x1A, 0xFE, 0x18, 0xB5, 0x8B, 0xBF, 0xC4, 0xFE, 0x37, 0x41, 0x58, 0xA8, 0x1B, 0x12, 0x83, 0x4D, 0xF6, 0x9D, 0x2B, 0x02, 0x03, 0x01, 0x00, 0x01, 0x02, 0x81, 0x81, 0x00, 0x99, 0xB1, 0x16, 0x05, 0x9C, 0x3E, 0x1B, 0xEE, 0xA9, 0x06, 0xAB, 0xA4, 0x60, 0x82, 0x4B, 0xEE, 0x0F, 0xFE, 0x3A, 0x1F, 0xBF, 0xEA, 0x08, 0xC6, 0x7E, 0x61, 0x34, 0x87, 0x67, 0x65, 0xD2, 0x4B, 0xFF, 0xAF, 0x65, 0x07, 0x25, 0x59, 0xFA, 0x88, 0x54, 0x46, 0x1E, 0x17, 0xE3, 0xA4, 0xF7, 0x1F, 0x2C, 0xA2, 0xCB, 0xC4, 0x7D, 0xB8, 0xD4, 0x0D, 0x39, 0xF6, 0x13, 0xD8, 0x15, 0x12, 0x0F, 0xE6, 0x89, 0xA6, 0x5F, 0xE3, 0x60, 0x8D, 0xA8, 0xE4, 0x41, 0xB3, 0xB5, 0xA6, 0xCF, 0x55, 0x45, 0xB3, 0x00, 0x8B, 0x20, 0x38, 0x27, 0x88, 0xC3, 0xCB, 0x4B, 0xF6, 0xF4, 0x78, 0xDC, 0x82, 0xC6, 0x89, 0xDA, 0xF0, 0x53, 0x2F, 0x54, 0xEC, 0xAE, 0x23, 0xE7, 0x8E, 0x61, 0xB2, 0x3F, 0x29, 0x9A, 0x2F, 0x53, 0x1C, 0xB8, 0x65, 0x5E, 0x86, 0x0B, 0x99, 0xC3, 0x92, 0x46, 0x6B, 0x75, 0xF7, 0x11, 0xE1, 0x02, 0x41, 0x00, 0xF5, 0x46, 0x1C, 0x4F, 0xAB, 0x07, 0xB6, 0xF3, 0xDC, 0x5B, 0x53, 0xB1, 0x74, 0x5C, 0x71, 0x8D, 0x2E, 0xDC, 0x53, 0xFD, 0x00, 0x3B, 0x0D, 0x1B, 0x25, 0x55, 0xB7, 0x70, 0x23, 0x56, 0xB5, 0xB9, 0x45, 0xE5, 0x6C, 0x44, 0xC6, 0x7D, 0x56, 0xFF, 0xCE, 0xE0, 0x56, 0x44, 0x21, 0x7C, 0x04, 0x07, 0xCC, 0x71, 0x97, 0x4F, 0x17, 0xA2, 0xE5, 0x7E, 0xCA, 0xDF, 0xDD, 0xFF, 0x79, 0x56, 0xCC, 0x39, 0x02, 0x41, 0x00, 0xED, 0x73, 0xE0, 0x99, 0x8A, 0xF2, 0x1C, 0xF1, 0xD7, 0x7C, 0xD4, 0xAA, 0xF5, 0x73, 0x6E, 0xC2, 0x58, 0xCC, 0x00, 0x26, 0x12, 0x9D, 0x76, 0x36, 0x40, 0xE1, 0x68, 0xA9, 0x56, 0xDA, 0x8F, 0xE2, 0x00, 0xA8, 0x7D, 0x1A, 0xD0, 0xF3, 0xDE, 0xA9, 0xB2, 0xEA, 0x14, 0xE5, 0x42, 0x97, 0x20, 0xDE, 0xE3, 0x22, 0x7C, 0xBA, 0xBC, 0xEA, 0xA3, 0xC2, 0x5E, 0xC6, 0x5E, 0xDD, 0x79, 0xA4, 0xFC, 0x83, 0x02, 0x41, 0x00, 0xBB, 0xF0, 0x39, 0xF7, 0x4D, 0xBC, 0xFE, 0x92, 0x03, 0x32, 0x33, 0x82, 0x11, 0x00, 0x58, 0xBD, 0xEE, 0xBF, 0x42, 0xD7, 0xE4, 0xDA, 0x5A, 0xA3, 0x87, 0x4B, 0x13, 0xE1, 0x28, 0x22, 0xE3, 0xE2, 0x10, 0x4D, 0xC8, 0x55, 0x36, 0xA6, 0x8A, 0x08, 0x3F, 0x53, 0xA4, 0xA6, 0x55, 0xE5, 0xFA, 0x0C, 0xA3, 0xBA, 0x12, 0x4F, 0xB7, 0x73, 0xC9, 0x58, 0x0B, 0x49, 0xD8, 0x88, 0x4E, 0x48, 0x94, 0xF9, 0x02, 0x40, 0x7F, 0xB2, 0x0D, 0x5B, 0x05, 0x29, 0xE6, 0xFE, 0xF7, 0xCF, 0x9D, 0xDE, 0xC2, 0x58, 0xED, 0x7B, 0x7E, 0x9D, 0x56, 0x87, 0x23, 0x03, 0xA3, 0x0A, 0xD2, 0x21, 0x66, 0x53, 0x8A, 0xED, 0xC6, 0xEA, 0xD7, 0x47, 0xC4, 0xDF, 0xA2, 0xF7, 0x43, 0x0B, 0x27, 0xB8, 0x52, 0xBC, 0x67, 0xEF, 0x36, 0x32, 0x27, 0x1B, 0xE8, 0xCF, 0xD3, 0xC0, 0xAB, 0x88, 0x5F, 0xC7, 0x76, 0x44, 0xCC, 0xA2, 0x39, 0x59, 0x02, 0x41, 0x00, 0x9A, 0xDF, 0x7E, 0x7F, 0xDB, 0xB1, 0xDB, 0x0E, 0xC1, 0xFC, 0x51, 0xFB, 0xC9, 0x11, 0x9B, 0x2A, 0x92, 0xCD, 0xC0, 0x6A, 0xAC, 0x87, 0x81, 0xA0, 0x02, 0x3B, 0xAF, 0x22, 0x66, 0xA2, 0x47, 0x16, 0x58, 0x75, 0x23, 0x4A, 0x5E, 0x66, 0x85, 0x7D, 0x8D, 0x0E, 0x1A, 0xD5, 0xA8, 0x5B, 0xB1, 0x7C, 0x05, 0x80, 0x12, 0xE6, 0xCB, 0x7D, 0x3C, 0xEB, 0x17, 0xC4, 0x87, 0xAA, 0xA6, 0xA2, 0xBD, 0xB8};
    
    memcpy(keyBuffer, private_key, EXTERNAL_FLASH_CORE_PRIVATE_KEY_LENGTH);
    return EXTERNAL_FLASH_CORE_PRIVATE_KEY_LENGTH;
}

uint16_t HAL_Set_Claim_Code(const char* code)
{
    return -1;
}

uint16_t HAL_Get_Claim_Code(char* buffer, unsigned len)
{
    if (len)
        buffer[0] = 0;
    return 0;
}

