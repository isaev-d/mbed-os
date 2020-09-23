/* mbed Microcontroller Library
 * Copyright (c) 2015, STMicroelectronics
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of STMicroelectronics nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "mbed_assert.h"
#include "analogin_api.h"

#if DEVICE_ANALOGIN

#include "mbed_wait_api.h"
#include "cmsis.h"
#include "pinmap.h"
#include "mbed_error.h"
#include "PeripheralPins.h"


void analogin_pll_configuration(void)
{
#if defined(DUAL_CORE)
    while (LL_HSEM_1StepLock(HSEM, CFG_HW_RCC_SEMID)) {
    }
#endif /* DUAL_CORE */

    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_ADC;
#if ( ((CLOCK_SOURCE) & USE_PLL_HSE_EXTC) & HSE_VALUE==8000000 )
    RCC_OscInitStruct.PLL2.PLL2M = 4;   // 2 MHz
    RCC_OscInitStruct.PLL2.PLLN = 240; // 480 MHz
#elif ( ((CLOCK_SOURCE) & USE_PLL_HSE_XTAL) & HSE_VALUE==25000000 )
    RCC_OscInitStruct.PLL2.PLL2M = 5;   // 5 MHz
    RCC_OscInitStruct.PLL2.PLL2N = 96; // 480 MHz
#elif ((CLOCK_SOURCE) & USE_PLL_HSI)
    RCC_OscInitStruct.PLL2.PLL2M = 32;   // 2 MHz
    RCC_OscInitStruct.PLL2.PLLN = 240; // 480 MHz
#else
    #error ADC clock setup failed, check CLOCK_SOURCE and HSE_VALUE
#endif
    PeriphClkInitStruct.PLL2.PLL2P = 2;
    PeriphClkInitStruct.PLL2.PLL2Q = 2;
    PeriphClkInitStruct.PLL2.PLL2R = 2;
    PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_1;
    PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
    PeriphClkInitStruct.PLL2.PLL2FRACN = 0;
    PeriphClkInitStruct.AdcClockSelection = RCC_ADCCLKSOURCE_PLL2;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
        error("analogin_init HAL_RCCEx_PeriphCLKConfig");
    }

#if defined(DUAL_CORE)
    LL_HSEM_ReleaseLock(HSEM, CFG_HW_RCC_SEMID, HSEM_CR_COREID_CURRENT);
#endif /* DUAL_CORE */

}

void analogin_init(analogin_t *obj, PinName pin)
{
    uint32_t function = (uint32_t)NC;

    // ADC Internal Channels "pins"  (Temperature, Vref, Vbat, ...)
    //   are described in PinNames.h and PeripheralPins.c
    //   Pin value must be between 0xF0 and 0xFF
    if ((pin < 0xF0) || (pin >= ALT0)) {
        // Normal channels
        // Get the peripheral name from the pin and assign it to the object
        obj->handle.Instance = (ADC_TypeDef *)pinmap_peripheral(pin, PinMap_ADC);
        // Get the functions (adc channel) from the pin and assign it to the object
        function = pinmap_function(pin, PinMap_ADC);
        // Configure GPIO
        pinmap_pinout(pin, PinMap_ADC);
    } else {
        // Internal channels
        obj->handle.Instance = (ADC_TypeDef *)pinmap_peripheral(pin, PinMap_ADC_Internal);
        function = pinmap_function(pin, PinMap_ADC_Internal);
        // No GPIO configuration for internal channels
    }
    MBED_ASSERT(obj->handle.Instance != (ADC_TypeDef *)NC);
    MBED_ASSERT(function != (uint32_t)NC);

#if defined(ALTC)
    if (pin == PA_0C) {
        HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PA0, SYSCFG_SWITCH_PA0_OPEN);
    }
    if (pin == PA_1C) {
        HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PA1, SYSCFG_SWITCH_PA1_OPEN);
    }
    if (pin == PC_2C) {
        HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PC2, SYSCFG_SWITCH_PC2_OPEN);
    }
    if (pin == PC_3C) {
        HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PC3, SYSCFG_SWITCH_PC3_OPEN);
    }
#endif /* ALTC */

    obj->channel = STM_PIN_CHANNEL(function);
    obj->differential = STM_PIN_INVERTED(function);

    // Save pin number for the read function
    obj->pin = pin;

    // Configure ADC object structures
    obj->handle.State = HAL_ADC_STATE_RESET;
    obj->handle.Init.ClockPrescaler           = ADC_CLOCK_ASYNC_DIV4;
    obj->handle.Init.Resolution               = ADC_RESOLUTION_16B;
    obj->handle.Init.ScanConvMode             = ADC_SCAN_DISABLE;
    obj->handle.Init.EOCSelection             = ADC_EOC_SINGLE_CONV;
    obj->handle.Init.LowPowerAutoWait         = DISABLE;
    obj->handle.Init.ContinuousConvMode       = DISABLE;
    obj->handle.Init.NbrOfConversion          = 1;
    obj->handle.Init.DiscontinuousConvMode    = DISABLE;
    obj->handle.Init.NbrOfDiscConversion      = 0;
    obj->handle.Init.ExternalTrigConv         = ADC_SOFTWARE_START;
    obj->handle.Init.ExternalTrigConvEdge     = ADC_EXTERNALTRIGCONVEDGE_NONE;
    obj->handle.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
    obj->handle.Init.Overrun                  = ADC_OVR_DATA_OVERWRITTEN;
    obj->handle.Init.LeftBitShift             = ADC_LEFTBITSHIFT_NONE;
    obj->handle.Init.OversamplingMode         = DISABLE;

    analogin_pll_configuration();

#if defined(ADC1)
    if ((ADCName)obj->handle.Instance == ADC_1) {
        __HAL_RCC_ADC12_CLK_ENABLE();
    }
#endif
#if defined(ADC2)
    if ((ADCName)obj->handle.Instance == ADC_2) {
        __HAL_RCC_ADC12_CLK_ENABLE();
    }
#endif
#if defined(ADC3)
    if ((ADCName)obj->handle.Instance == ADC_3) {
        __HAL_RCC_ADC3_CLK_ENABLE();
    }
#endif

    if (HAL_ADC_Init(&obj->handle) != HAL_OK) {
        error("analogin_init HAL_ADC_Init");
    }

    if ((ADCName)obj->handle.Instance == ADC_1) {
        ADC_MultiModeTypeDef multimode = {0};
        multimode.Mode = ADC_MODE_INDEPENDENT;
        if (HAL_ADCEx_MultiModeConfigChannel(&obj->handle, &multimode) != HAL_OK) {
            error("HAL_ADCEx_MultiModeConfigChannel HAL_ADC_Init");
        }
    }

    // Calibration
    if (obj->differential) {
        HAL_ADCEx_Calibration_Start(&obj->handle, ADC_CALIB_OFFSET, ADC_DIFFERENTIAL_ENDED);
    } else {
        HAL_ADCEx_Calibration_Start(&obj->handle, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED);
    }
}

uint16_t adc_read(analogin_t *obj)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    /* Reconfigure PLL as it could be lost during deepsleep */
    analogin_pll_configuration();

    // Configure ADC channel
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_64CYCLES_5;
    sConfig.Offset       = 0;
    if (obj->differential) {
        sConfig.SingleDiff = ADC_DIFFERENTIAL_ENDED;
    } else {
        sConfig.SingleDiff = ADC_SINGLE_ENDED;
    }
    sConfig.OffsetNumber           = ADC_OFFSET_NONE;
    sConfig.OffsetRightShift       = DISABLE;
    sConfig.OffsetSignedSaturation = DISABLE;

    switch (obj->channel) {
        case 0:
            sConfig.Channel = ADC_CHANNEL_0;
            break;
        case 1:
            sConfig.Channel = ADC_CHANNEL_1;
            break;
        case 2:
            sConfig.Channel = ADC_CHANNEL_2;
            break;
        case 3:
            sConfig.Channel = ADC_CHANNEL_3;
            break;
        case 4:
            sConfig.Channel = ADC_CHANNEL_4;
            break;
        case 5:
            sConfig.Channel = ADC_CHANNEL_5;
            break;
        case 6:
            sConfig.Channel = ADC_CHANNEL_6;
            break;
        case 7:
            sConfig.Channel = ADC_CHANNEL_7;
            break;
        case 8:
            sConfig.Channel = ADC_CHANNEL_8;
            break;
        case 9:
            sConfig.Channel = ADC_CHANNEL_9;
            break;
        case 10:
            sConfig.Channel = ADC_CHANNEL_10;
            break;
        case 11:
            sConfig.Channel = ADC_CHANNEL_11;
            break;
        case 12:
            sConfig.Channel = ADC_CHANNEL_12;
            break;
        case 13:
            sConfig.Channel = ADC_CHANNEL_13;
            break;
        case 14:
            sConfig.Channel = ADC_CHANNEL_14;
            break;
        case 15:
            sConfig.Channel = ADC_CHANNEL_15;
            break;
        case 16:
            sConfig.Channel = ADC_CHANNEL_16;
            break;
        case 17:
            sConfig.Channel = ADC_CHANNEL_17;

            if ((ADCName)obj->handle.Instance == ADC_3) {
                sConfig.Channel = ADC_CHANNEL_VBAT;
                sConfig.SamplingTime = ADC_SAMPLETIME_810CYCLES_5;
            }
            break;
        case 18:
            sConfig.Channel = ADC_CHANNEL_18;

            if ((ADCName)obj->handle.Instance == ADC_3) {
                sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
                sConfig.SamplingTime = ADC_SAMPLETIME_810CYCLES_5;
            }
            break;
        case 19:
            sConfig.Channel = ADC_CHANNEL_19;

            if ((ADCName)obj->handle.Instance == ADC_3) {
                sConfig.Channel = ADC_CHANNEL_VREFINT;
                sConfig.SamplingTime = ADC_SAMPLETIME_810CYCLES_5;
            }
            break;
        default:
            return 0;
    }

    if (HAL_ADC_ConfigChannel(&obj->handle, &sConfig) != HAL_OK) {
        error("HAL_ADC_ConfigChannel issue");
    }

    if (HAL_ADC_Start(&obj->handle) != HAL_OK) {
        error("HAL_ADC_Start issue");
    }

    // Wait end of conversion and get value
    uint16_t adcValue = 0;
    if (HAL_ADC_PollForConversion(&obj->handle, 10) == HAL_OK) {
        adcValue = (uint16_t)HAL_ADC_GetValue(&obj->handle);
    }
    LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE((&obj->handle)->Instance), LL_ADC_PATH_INTERNAL_NONE);
    return adcValue;
}

const PinMap *analogin_pinmap()
{
    return PinMap_ADC;
}

#endif
