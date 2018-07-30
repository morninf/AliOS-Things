/*
 * The Clear BSD License
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted (subject to the limitations in the disclaimer below) provided
 * that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY THIS LICENSE.
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
 */

/*
 * How to setup clock using clock driver functions:
 *
 * 1. Call CLOCK_InitXXX() to configure corresponding SCG clock source.
 *    Note: The clock could not be set when it is being used as system clock.
 *    In default out of reset, the CPU is clocked from FIRC(IRC48M),
 *    so before setting FIRC, change to use another avaliable clock source.
 *
 * 2. Call CLOCK_SetXtal0Freq() to set XTAL0 frequency based on board settings.
 *
 * 3. Call CLOCK_SetXxxModeSysClkConfig() to set SCG mode for Xxx run mode.
 *    Wait until the system clock source is changed to target source.
 *
 * 4. If power mode change is needed, call SMC_SetPowerModeProtection() to allow
 *    corresponding power mode and SMC_SetPowerModeXxx() to change to Xxx mode.
 *    Supported run mode and clock restrictions could be found in Reference Manual.
 */

/* TEXT BELOW IS USED AS SETTING FOR THE CLOCKS TOOL *****************************
!!ClocksProfile
product: Clocks v1.0
processor: MKL28Z512xxx7
package_id: MKL28Z512VLL7
mcu_data: ksdk2_0
processor_version: 1.0.0
board: FRDM-KL28Z
 * BE CAREFUL MODIFYING THIS COMMENT - IT IS YAML SETTINGS FOR THE CLOCKS TOOL **/

#include "fsl_smc.h"
#include "clock_config.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define SCG_CLKOUTCNFG_SIRC 2U /*!< SCG CLKOUT clock select: Slow IRC */
#define SCG_SOSC_DISABLE 0U    /*!< System OSC disabled */
#define SCG_SPLL_DISABLE 0U    /*!< System PLL disabled */
#define SCG_SYS_OSC_CAP_0P 0U  /*!< Oscillator 0pF capacitor load */

/*******************************************************************************
 * Variables
 ******************************************************************************/
/* System clock frequency. */
extern uint32_t SystemCoreClock;

/*******************************************************************************
 * Code
 ******************************************************************************/
/*FUNCTION**********************************************************************
 *
 * Function Name : CLOCK_CONFIG_SetScgOutSel
 * Description   : Set the SCG clock out select (CLKOUTSEL).
 * Param setting : The selected clock source.
 *
 *END**************************************************************************/
static void CLOCK_CONFIG_SetScgOutSel(uint8_t setting)
{
    SCG->CLKOUTCNFG = SCG_CLKOUTCNFG_CLKOUTSEL(setting);
}

/*FUNCTION**********************************************************************
 *
 * Function Name : CLOCK_CONFIG_FircSafeConfig
 * Description   : This function is used to safely configure FIRC clock.
 *                 In default out of reset, the CPU is clocked from FIRC(IRC48M).
 *                 Before setting FIRC, change to use SIRC as system clock,
 *                 then configure FIRC. After FIRC is set, change back to use FIRC
 *                 in case SIRC need to be configured.
 * Param fircConfig  : FIRC configuration.
 *
 *END**************************************************************************/
static void CLOCK_CONFIG_FircSafeConfig(const scg_firc_config_t *fircConfig)
{
    scg_sys_clk_config_t curConfig;
    const scg_sirc_config_t scgSircConfig = {.enableMode = kSCG_SircEnable,
                                             .div1 = kSCG_AsyncClkDisable,
                                             .div3 = kSCG_AsyncClkDivBy2,
                                             .range = kSCG_SircRangeHigh};
    scg_sys_clk_config_t sysClkSafeConfigSource =
    {
        .divSlow = kSCG_SysClkDivBy4, /* Slow clock divider */
#if FSL_CLOCK_DRIVER_VERSION < MAKE_VERSION(2, 1, 1)
        .reserved1 = 0,
        .reserved2 = 0,
        .reserved3 = 0,
#endif
        .divCore = kSCG_SysClkDivBy1, /* Core clock divider */
#if FSL_CLOCK_DRIVER_VERSION < MAKE_VERSION(2, 1, 1)
        .reserved4 = 0,
#endif
        .src = kSCG_SysClkSrcSirc, /* System clock source */
#if FSL_CLOCK_DRIVER_VERSION < MAKE_VERSION(2, 1, 1)
        .reserved5 = 0,
#endif
    };
    /* Init Sirc. */
    CLOCK_InitSirc(&scgSircConfig);
    /* Change to use SIRC as system clock source to prepare to change FIRCCFG register. */
    CLOCK_SetRunModeSysClkConfig(&sysClkSafeConfigSource);
    /* Wait for clock source switch finished. */
    do
    {
        CLOCK_GetCurSysClkConfig(&curConfig);
    } while (curConfig.src != sysClkSafeConfigSource.src);

    /* Init Firc. */
    CLOCK_InitFirc(fircConfig);
    /* Change back to use FIRC as system clock source in order to configure SIRC if needed. */
    sysClkSafeConfigSource.src = kSCG_SysClkSrcFirc;
    CLOCK_SetRunModeSysClkConfig(&sysClkSafeConfigSource);
    /* Wait for clock source switch finished. */
    do
    {
        CLOCK_GetCurSysClkConfig(&curConfig);
    } while (curConfig.src != sysClkSafeConfigSource.src);
}

/*******************************************************************************
 ********************** Configuration BOARD_BootClockRUN ***********************
 ******************************************************************************/
/* TEXT BELOW IS USED AS SETTING FOR THE CLOCKS TOOL *****************************
!!Configuration
name: BOARD_BootClockRUN
outputs:
- {id: Core_clock.outFreq, value: 48 MHz}
- {id: FIRCDIV1_CLK.outFreq, value: 48 MHz}
- {id: FIRCDIV3_CLK.outFreq, value: 48 MHz}
- {id: LPO_clock.outFreq, value: 1 kHz}
- {id: OSC32KCLK.outFreq, value: 32.768 kHz}
- {id: SIRCDIV3_CLK.outFreq, value: 4 MHz}
- {id: SIRC_CLK.outFreq, value: 8 MHz}
- {id: SOSCDIV3_CLK.outFreq, value: 32.768 kHz}
- {id: SOSCER_CLK.outFreq, value: 32.768 kHz}
- {id: SOSC_CLK.outFreq, value: 32.768 kHz}
- {id: Slow_clock.outFreq, value: 24 MHz}
- {id: System_clock.outFreq, value: 48 MHz}
settings:
- {id: SCG.FIRCDIV1.scale, value: '1', locked: true}
- {id: SCG.FIRCDIV3.scale, value: '1', locked: true}
- {id: SCG.SIRCDIV3.scale, value: '2', locked: true}
- {id: SCG.SOSCDIV3.scale, value: '1', locked: true}
- {id: SCG_SOSCCFG_OSC_MODE_CFG, value: ModeOscLowPower}
- {id: SCG_SOSCCSR_SOSCEN_CFG, value: Enabled}
- {id: SCG_SOSCCSR_SOSCERCLKEN_CFG, value: Enabled}
sources:
- {id: SCG.SOSC.outFreq, value: 32.768 kHz, enabled: true}
 * BE CAREFUL MODIFYING THIS COMMENT - IT IS YAML SETTINGS FOR THE CLOCKS TOOL **/

/*******************************************************************************
 * Variables for BOARD_BootClockRUN configuration
 ******************************************************************************/
const scg_sys_clk_config_t g_sysClkConfig_BOARD_BootClockRUN = {
    .divSlow = kSCG_SysClkDivBy2, /* Slow Clock Divider: divided by 2 */
#if FSL_CLOCK_DRIVER_VERSION < MAKE_VERSION(2, 1, 1)
    .reserved1 = 0,
    .reserved2 = 0,
    .reserved3 = 0,
#endif
    .divCore = kSCG_SysClkDivBy1, /* Core Clock Divider: divided by 1 */
#if FSL_CLOCK_DRIVER_VERSION < MAKE_VERSION(2, 1, 1)
    .reserved4 = 0,
#endif
    .src = kSCG_SysClkSrcFirc, /* Fast IRC is selected as System Clock Source */
#if FSL_CLOCK_DRIVER_VERSION < MAKE_VERSION(2, 1, 1)
    .reserved5 = 0,
#endif
};
const scg_sosc_config_t g_scgSysOscConfig_BOARD_BootClockRUN = {
    .freq = 32768U,                                           /* System Oscillator frequency: 32768Hz */
    .enableMode = kSCG_SysOscEnable | kSCG_SysOscEnableErClk, /* Enable System OSC clock, Enable OSCERCLK */
    .monitorMode = kSCG_SysOscMonitorDisable,                 /* Monitor disabled */
    .div1 = kSCG_AsyncClkDisable,                             /* System OSC Clock Divider 1: Clock output is disabled */
    .div3 = kSCG_AsyncClkDivBy1,                              /* System OSC Clock Divider 3: divided by 1 */
    .capLoad = SCG_SYS_OSC_CAP_0P,                            /* Oscillator capacity load: 0pF */
    .workMode = kSCG_SysOscModeOscLowPower,                   /* Oscillator low power */
};
const scg_sirc_config_t g_scgSircConfig_BOARD_BootClockRUN = {
    .enableMode = kSCG_SircEnable | kSCG_SircEnableInLowPower, /* Enable SIRC clock, Enable SIRC in low power mode */
    .div1 = kSCG_AsyncClkDisable,                              /* Slow IRC Clock Divider 1: Clock output is disabled */
    .div3 = kSCG_AsyncClkDivBy2,                               /* Slow IRC Clock Divider 3: divided by 2 */
    .range = kSCG_SircRangeHigh,                               /* Slow IRC high range clock (8 MHz) */
};
const scg_firc_config_t g_scgFircConfig_BOARD_BootClockRUN = {
    .enableMode = kSCG_FircEnable, /* Enable FIRC clock */
    .div1 = kSCG_AsyncClkDivBy1,   /* Fast IRC Clock Divider 1: divided by 1 */
    .div3 = kSCG_AsyncClkDivBy1,   /* Fast IRC Clock Divider 3: divided by 1 */
    .range = kSCG_FircRange48M,    /* Fast IRC is trimmed to 48MHz */
    .trimConfig = NULL,            /* Fast IRC Trim disabled */
};
const scg_spll_config_t g_scgSysPllConfig_BOARD_BootClockRUN = {
    .enableMode = SCG_SPLL_DISABLE,           /* System PLL disabled */
    .monitorMode = kSCG_SysPllMonitorDisable, /* Monitor disabled */
    .div1 = kSCG_AsyncClkDisable,             /* System PLL Clock Divider 1: Clock output is disabled */
    .div3 = kSCG_AsyncClkDisable,             /* System PLL Clock Divider 3: Clock output is disabled */
    .src = kSCG_SysPllSrcSysOsc,              /* System PLL clock source is System OSC */
    .prediv = 0,                              /* Divided by 1 */
    .mult = 0,                                /* Multiply Factor is 16 */
};
/*******************************************************************************
 * Code for BOARD_BootClockRUN configuration
 ******************************************************************************/
void BOARD_BootClockRUN(void)
{
    scg_sys_clk_config_t curConfig;

    /* Init SOSC according to board configuration. */
    CLOCK_InitSysOsc(&g_scgSysOscConfig_BOARD_BootClockRUN);
    /* Set the XTAL0 frequency based on board settings. */
    CLOCK_SetXtal0Freq(g_scgSysOscConfig_BOARD_BootClockRUN.freq);
    /* Init FIRC. */
    CLOCK_CONFIG_FircSafeConfig(&g_scgFircConfig_BOARD_BootClockRUN);
    /* Init SIRC. */
    CLOCK_InitSirc(&g_scgSircConfig_BOARD_BootClockRUN);
    /* Set SCG to FIRC mode. */
    CLOCK_SetRunModeSysClkConfig(&g_sysClkConfig_BOARD_BootClockRUN);
    /* Wait for clock source switch finished. */
    do
    {
        CLOCK_GetCurSysClkConfig(&curConfig);
    } while (curConfig.src != g_sysClkConfig_BOARD_BootClockRUN.src);
    /* Set SystemCoreClock variable. */
    SystemCoreClock = BOARD_BOOTCLOCKRUN_CORE_CLOCK;
}

/*******************************************************************************
 ********************* Configuration BOARD_BootClockHSRUN **********************
 ******************************************************************************/
/* TEXT BELOW IS USED AS SETTING FOR THE CLOCKS TOOL *****************************
!!Configuration
name: BOARD_BootClockHSRUN
outputs:
- {id: CLKOUT.outFreq, value: 8 MHz}
- {id: Core_clock.outFreq, value: 96 MHz, locked: true, accuracy: '0.001'}
- {id: FIRCDIV1_CLK.outFreq, value: 48 MHz}
- {id: FIRCDIV3_CLK.outFreq, value: 48 MHz}
- {id: LPO_clock.outFreq, value: 1 kHz}
- {id: OSC32KCLK.outFreq, value: 32.768 kHz}
- {id: PLLDIV1_CLK.outFreq, value: 96 MHz}
- {id: PLLDIV3_CLK.outFreq, value: 96 MHz}
- {id: SIRCDIV1_CLK.outFreq, value: 8 MHz}
- {id: SIRCDIV3_CLK.outFreq, value: 8 MHz}
- {id: SIRC_CLK.outFreq, value: 8 MHz}
- {id: SOSCDIV1_CLK.outFreq, value: 32.768 kHz}
- {id: SOSCDIV3_CLK.outFreq, value: 32.768 kHz}
- {id: SOSCER_CLK.outFreq, value: 32.768 kHz}
- {id: SOSC_CLK.outFreq, value: 32.768 kHz}
- {id: Slow_clock.outFreq, value: 24 MHz, locked: true, accuracy: '0.001'}
- {id: System_clock.outFreq, value: 96 MHz}
settings:
- {id: SCGMode, value: SPLL}
- {id: powerMode, value: HSRUN}
- {id: CLKOUTConfig, value: 'yes'}
- {id: SCG.DIVSLOW.scale, value: '4'}
- {id: SCG.FIRCDIV1.scale, value: '1', locked: true}
- {id: SCG.FIRCDIV3.scale, value: '1', locked: true}
- {id: SCG.PREDIV.scale, value: '4'}
- {id: SCG.SCSSEL.sel, value: SCG.SPLL_DIV2_CLK}
- {id: SCG.SIRCDIV1.scale, value: '1', locked: true}
- {id: SCG.SIRCDIV3.scale, value: '1', locked: true}
- {id: SCG.SOSCDIV1.scale, value: '1', locked: true}
- {id: SCG.SOSCDIV3.scale, value: '1', locked: true}
- {id: SCG.SPLLDIV1.scale, value: '1', locked: true}
- {id: SCG.SPLLDIV3.scale, value: '1', locked: true}
- {id: SCG.SPLLSRCSEL.sel, value: SCG.FIRC}
- {id: 'SCG::RCCR[DIVCORE].bitField', value: Divide-by-9}
- {id: 'SCG::RCCR[DIVSLOW].bitField', value: Divide-by-4}
- {id: 'SCG::RCCR[SCS].bitField', value: System PLL (SPLL_CLK)}
- {id: SCG_SOSCCFG_OSC_MODE_CFG, value: ModeOscLowPower}
- {id: SCG_SOSCCSR_SOSCEN_CFG, value: Enabled}
- {id: SCG_SOSCCSR_SOSCERCLKEN_CFG, value: Enabled}
- {id: SCG_SPLLCSR_SPLLEN_CFG, value: Enabled}
sources:
- {id: SCG.SOSC.outFreq, value: 32.768 kHz, enabled: true}
 * BE CAREFUL MODIFYING THIS COMMENT - IT IS YAML SETTINGS FOR THE CLOCKS TOOL **/

/*******************************************************************************
 * Variables for BOARD_BootClockHSRUN configuration
 ******************************************************************************/
/* System clock source and divider for run mode, it is used to prepare for switch to HSRUN mode，to make sure HSRUN
 * switch frequency range
 * is not bigger than x2.*/
const scg_sys_clk_config_t g_sysClkConfig_BOARD_BootClockRunToHSRUN = {
    .divSlow = kSCG_SysClkDivBy4, /* Slow Clock Divider: divided by 4 */
#if FSL_CLOCK_DRIVER_VERSION < MAKE_VERSION(2, 1, 1)
    .reserved1 = 0,
    .reserved2 = 0,
    .reserved3 = 0,
#endif
    .divCore = kSCG_SysClkDivBy2, /* Core Clock Divider: divided by 1 */
#if FSL_CLOCK_DRIVER_VERSION < MAKE_VERSION(2, 1, 1)
    .reserved4 = 0,
#endif
    .src = kSCG_SysClkSrcSysPll, /* System PLL is selected as System Clock Source */
#if FSL_CLOCK_DRIVER_VERSION < MAKE_VERSION(2, 1, 1)
    .reserved5 = 0,
#endif
};

const scg_sys_clk_config_t g_sysClkConfig_BOARD_BootClockHSRUN = {
    .divSlow = kSCG_SysClkDivBy4, /* Slow Clock Divider: divided by 4 */
#if FSL_CLOCK_DRIVER_VERSION < MAKE_VERSION(2, 1, 1)
    .reserved1 = 0,
    .reserved2 = 0,
    .reserved3 = 0,
#endif
    .divCore = kSCG_SysClkDivBy1, /* Core Clock Divider: divided by 1 */
#if FSL_CLOCK_DRIVER_VERSION < MAKE_VERSION(2, 1, 1)
    .reserved4 = 0,
#endif
    .src = kSCG_SysClkSrcSysPll, /* System PLL is selected as System Clock Source */
#if FSL_CLOCK_DRIVER_VERSION < MAKE_VERSION(2, 1, 1)
    .reserved5 = 0,
#endif
};
const scg_sosc_config_t g_scgSysOscConfig_BOARD_BootClockHSRUN = {
    .freq = 32768U,                                           /* System Oscillator frequency: 32768Hz */
    .enableMode = kSCG_SysOscEnable | kSCG_SysOscEnableErClk, /* Enable System OSC clock, Enable OSCERCLK */
    .monitorMode = kSCG_SysOscMonitorDisable,                 /* Monitor disabled */
    .div1 = kSCG_AsyncClkDivBy1,                              /* System OSC Clock Divider 1: divided by 1 */
    .div3 = kSCG_AsyncClkDivBy1,                              /* System OSC Clock Divider 3: divided by 1 */
    .capLoad = SCG_SYS_OSC_CAP_0P,                            /* Oscillator capacity load: 0pF */
    .workMode = kSCG_SysOscModeOscLowPower,                   /* Oscillator low power */
};
const scg_sirc_config_t g_scgSircConfig_BOARD_BootClockHSRUN = {
    .enableMode = kSCG_SircEnable | kSCG_SircEnableInLowPower, /* Enable SIRC clock, Enable SIRC in low power mode */
    .div1 = kSCG_AsyncClkDivBy1,                               /* Slow IRC Clock Divider 1: divided by 1 */
    .div3 = kSCG_AsyncClkDivBy1,                               /* Slow IRC Clock Divider 3: divided by 1 */
    .range = kSCG_SircRangeHigh,                               /* Slow IRC high range clock (8 MHz) */
};
const scg_firc_config_t g_scgFircConfig_BOARD_BootClockHSRUN = {
    .enableMode = kSCG_FircEnable, /* Enable FIRC clock */
    .div1 = kSCG_AsyncClkDivBy1,   /* Fast IRC Clock Divider 1: divided by 1 */
    .div3 = kSCG_AsyncClkDivBy1,   /* Fast IRC Clock Divider 3: divided by 1 */
    .range = kSCG_FircRange48M,    /* Fast IRC is trimmed to 48MHz */
    .trimConfig = NULL,            /* Fast IRC Trim disabled */
};
const scg_spll_config_t g_scgSysPllConfig_BOARD_BootClockHSRUN = {
    .enableMode = kSCG_SysPllEnable,          /* Enable SPLL clock */
    .monitorMode = kSCG_SysPllMonitorDisable, /* Monitor disabled */
    .div1 = kSCG_AsyncClkDivBy1,              /* System PLL Clock Divider 1: divided by 1 */
    .div3 = kSCG_AsyncClkDivBy1,              /* System PLL Clock Divider 3: divided by 1 */
    .src = kSCG_SysPllSrcFirc,                /* System PLL clock source is Fast IRC */
    .prediv = 3,                              /* Divided by 4 */
    .mult = 0,                                /* Multiply Factor is 16 */
};
/*******************************************************************************
 * Code for BOARD_BootClockHSRUN configuration
 ******************************************************************************/
void BOARD_BootClockHSRUN(void)
{
    scg_sys_clk_config_t curConfig;
    /* In HSRUN mode, the maximum allowable change in frequency of the system/bus/core/flash is
    * restricted to x2, to follow this restriction, enter HSRUN mode should follow:
 * 1.set the run mode to a safe configurations.
    * 2.set the PLL or FLL output target frequency for HSRUN mode.
    * 3.switch RUN mode configuration.
    * 4.switch to HSRUN mode.
    * 5.switch to HSRUN mode target requency value.
    */
    /* Init SOSC according to board configuration. */
    CLOCK_InitSysOsc(&g_scgSysOscConfig_BOARD_BootClockHSRUN);
    /* Set the XTAL0 frequency based on board settings. */
    CLOCK_SetXtal0Freq(g_scgSysOscConfig_BOARD_BootClockHSRUN.freq);
    /* Init FIRC. */
    CLOCK_CONFIG_FircSafeConfig(&g_scgFircConfig_BOARD_BootClockHSRUN);

    /* Init SysPll. */
    CLOCK_InitSysPll(&g_scgSysPllConfig_BOARD_BootClockHSRUN);

    /* switch run mode core clock source and set run mode divider
    to make sure core frequency not overflow */
    CLOCK_SetRunModeSysClkConfig(&g_sysClkConfig_BOARD_BootClockRunToHSRUN);

    /* Set HSRUN power mode. */
    SMC_SetPowerModeProtection(SMC, kSMC_AllowPowerModeAll);
    SMC_SetPowerModeHsrun(SMC);
    while (SMC_GetPowerModeState(SMC) != kSMC_PowerStateHsrun)
    {
    }

    /* Set SCG to SPLL mode. */
    CLOCK_SetHsrunModeSysClkConfig(&g_sysClkConfig_BOARD_BootClockHSRUN);
    /* Wait for clock source switch finished. */
    do
    {
        CLOCK_GetCurSysClkConfig(&curConfig);
    } while (curConfig.src != g_sysClkConfig_BOARD_BootClockHSRUN.src);
    /* Init SIRC. */
    CLOCK_InitSirc(&g_scgSircConfig_BOARD_BootClockHSRUN);
    /* Set SystemCoreClock variable. */
    SystemCoreClock = BOARD_BOOTCLOCKHSRUN_CORE_CLOCK;
    /* Set SCG CLKOUT selection. */
    CLOCK_CONFIG_SetScgOutSel(SCG_CLKOUTCNFG_SIRC);
}

/*******************************************************************************
 ********************* Configuration BOARD_BootClockVLPR ***********************
 ******************************************************************************/
/* TEXT BELOW IS USED AS SETTING FOR THE CLOCKS TOOL *****************************
!!Configuration
name: BOARD_BootClockVLPR
outputs:
- {id: Core_clock.outFreq, value: 8 MHz, locked: true, accuracy: '0.001'}
- {id: LPO_clock.outFreq, value: 1 kHz}
- {id: SIRC_CLK.outFreq, value: 8 MHz}
- {id: Slow_clock.outFreq, value: 1 MHz, locked: true, accuracy: '0.001'}
- {id: System_clock.outFreq, value: 8 MHz}
settings:
- {id: SCGMode, value: SIRC}
- {id: powerMode, value: VLPR}
- {id: SCG.DIVSLOW.scale, value: '8'}
- {id: SCG.SCSSEL.sel, value: SCG.SIRC}
- {id: 'SCG::RCCR[DIVSLOW].bitField', value: Divide-by-8}
- {id: 'SCG::RCCR[SCS].bitField', value: Slow IRC (SIRC_CLK)}
- {id: SCG_FIRCCSR_FIRCLPEN_CFG, value: Enabled}
sources:
- {id: SCG.SOSC.outFreq, value: 32.768 kHz, enabled: true}
 * BE CAREFUL MODIFYING THIS COMMENT - IT IS YAML SETTINGS FOR THE CLOCKS TOOL **/

/*******************************************************************************
 * Variables for BOARD_BootClockVLPR configuration
 ******************************************************************************/
const scg_sys_clk_config_t g_sysClkConfig_BOARD_BootClockVLPR = {
    .divSlow = kSCG_SysClkDivBy8, /* Slow Clock Divider: divided by 8 */
#if FSL_CLOCK_DRIVER_VERSION < MAKE_VERSION(2, 1, 1)
    .reserved1 = 0,
    .reserved2 = 0,
    .reserved3 = 0,
#endif
    .divCore = kSCG_SysClkDivBy1, /* Core Clock Divider: divided by 1 */
#if FSL_CLOCK_DRIVER_VERSION < MAKE_VERSION(2, 1, 1)
    .reserved4 = 0,
#endif
    .src = kSCG_SysClkSrcSirc, /* Slow IRC is selected as System Clock Source */
#if FSL_CLOCK_DRIVER_VERSION < MAKE_VERSION(2, 1, 1)
    .reserved5 = 0,
#endif
};
const scg_sosc_config_t g_scgSysOscConfig_BOARD_BootClockVLPR = {
    .freq = 0U,                               /* System Oscillator frequency: 0Hz */
    .enableMode = SCG_SOSC_DISABLE,           /* System OSC disabled */
    .monitorMode = kSCG_SysOscMonitorDisable, /* Monitor disabled */
    .div1 = kSCG_AsyncClkDisable,             /* System OSC Clock Divider 1: Clock output is disabled */
    .div3 = kSCG_AsyncClkDisable,             /* System OSC Clock Divider 3: Clock output is disabled */
    .capLoad = SCG_SYS_OSC_CAP_0P,            /* Oscillator capacity load: 0pF */
    .workMode = kSCG_SysOscModeExt,           /* Use external clock */
};
const scg_sirc_config_t g_scgSircConfig_BOARD_BootClockVLPR = {
    .enableMode = kSCG_SircEnable | kSCG_SircEnableInLowPower, /* Enable SIRC clock, Enable SIRC in low power mode */
    .div1 = kSCG_AsyncClkDisable,                              /* Slow IRC Clock Divider 1: Clock output is disabled */
    .div3 = kSCG_AsyncClkDisable,                              /* Slow IRC Clock Divider 3: Clock output is disabled */
    .range = kSCG_SircRangeHigh,                               /* Slow IRC high range clock (8 MHz) */
};
const scg_firc_config_t g_scgFircConfig_BOARD_BootClockVLPR = {
    .enableMode = kSCG_FircEnable | kSCG_FircEnableInLowPower, /* Enable FIRC clock, Enable FIRC in low power mode */
    .div1 = kSCG_AsyncClkDisable,                              /* Fast IRC Clock Divider 1: Clock output is disabled */
    .div3 = kSCG_AsyncClkDisable,                              /* Fast IRC Clock Divider 3: Clock output is disabled */
    .range = kSCG_FircRange48M,                                /* Fast IRC is trimmed to 48MHz */
    .trimConfig = NULL,                                        /* Fast IRC Trim disabled */
};
const scg_spll_config_t g_scgSysPllConfig_BOARD_BootClockVLPR = {
    .enableMode = SCG_SPLL_DISABLE,           /* System PLL disabled */
    .monitorMode = kSCG_SysPllMonitorDisable, /* Monitor disabled */
    .div1 = kSCG_AsyncClkDisable,             /* System PLL Clock Divider 1: Clock output is disabled */
    .div3 = kSCG_AsyncClkDisable,             /* System PLL Clock Divider 3: Clock output is disabled */
    .src = kSCG_SysPllSrcSysOsc,              /* System PLL clock source is System OSC */
    .prediv = 0,                              /* Divided by 1 */
    .mult = 0,                                /* Multiply Factor is 16 */
};
/*******************************************************************************
 * Code for BOARD_BootClockVLPR configuration
 ******************************************************************************/
void BOARD_BootClockVLPR(void)
{
    /* Init FIRC. */
    CLOCK_CONFIG_FircSafeConfig(&g_scgFircConfig_BOARD_BootClockVLPR);
    /* Init SIRC. */
    CLOCK_InitSirc(&g_scgSircConfig_BOARD_BootClockVLPR);
    /* Allow SMC all power modes. */
    SMC_SetPowerModeProtection(SMC, kSMC_AllowPowerModeAll);
    /* Set VLPR power mode. */
    SMC_SetPowerModeVlpr(SMC);
    while (SMC_GetPowerModeState(SMC) != kSMC_PowerStateVlpr)
    {
    }
    /* Set SystemCoreClock variable. */
    SystemCoreClock = BOARD_BOOTCLOCKVLPR_CORE_CLOCK;
}
