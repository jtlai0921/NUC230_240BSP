/**************************************************************************//**
 * @file     ap_main.c
 * @version  V1.00
 * $Revision: 2 $
 * $Date: 15/08/18 11:54a $
 * @brief    FMC VECMAP sample program (loader) for Nano100 series MCU
 *
 * @note
 * Copyright (C) 2014 Nuvoton Technology Corp. All rights reserved.
*****************************************************************************/
#include <stdio.h>
#include "NUC230_240.h"
#include "NuEdu-Basic01.h"
#include "map.h"


static int  load_image_to_flash(uint32_t image_base, uint32_t image_limit, uint32_t flash_addr, uint32_t max_size);
int IsDebugFifoEmpty(void);
volatile uint32_t const VersionNumber __attribute__((at(0x1000 + USER_AP0_ENTRY))) = 0x00001;

void TMR0_IRQHandler(void)
{
    static uint32_t sec = 1;
    printf("%d sec\n", sec++);
    LED_On(sec);

    // clear timer interrupt flag
    TIMER_ClearIntFlag(TIMER0);

}
#ifdef __ARMCC_VERSION
__asm __INLINE __set_SP(uint32_t _sp)
{
    MSR MSP, r0
    BX lr
}
#endif
__INLINE void BranchTo(uint32_t u32Address)
{
    FUNC_PTR        *func;
    FMC_SetVectorPageAddr(u32Address);
    func = (FUNC_PTR *)(*(uint32_t *)(u32Address + 4));
    printf("branch to address 0x%x\n", (int)func);
    printf("\n\nChange VECMAP and branch to user application...\n");
    while(!IsDebugFifoEmpty());
    __set_SP(*(uint32_t *)u32Address);
    func();
}

#define PLL_CLOCK           48000000
void SYS_Init(void)
{
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init System Clock                                                                                       */
    /*---------------------------------------------------------------------------------------------------------*/
    /* Unlock protected registers */
    SYS_UnlockReg();

    /* Enable Internal RC 22.1184MHz clock */
    CLK_EnableXtalRC(CLK_PWRCON_OSC22M_EN_Msk);

    /* Waiting for Internal RC clock ready */
    CLK_WaitClockReady(CLK_CLKSTATUS_OSC22M_STB_Msk);

    /* Switch HCLK clock source to Internal RC and HCLK source divide 1 */
    CLK_SetHCLK(CLK_CLKSEL0_HCLK_S_HIRC, CLK_CLKDIV_HCLK(1));

    /* Enable external XTAL 12MHz clock */
    CLK_EnableXtalRC(CLK_PWRCON_XTL12M_EN_Msk);

    /* Waiting for external XTAL clock ready */
    CLK_WaitClockReady(CLK_CLKSTATUS_XTL12M_STB_Msk);

    /* Set core clock as PLL_CLOCK from PLL */
    CLK_SetCoreClock(PLL_CLOCK);

    /* Update System Core Clock */
    /* User can use SystemCoreClockUpdate() to calculate SystemCoreClock and CycylesPerUs automatically. */
    SystemCoreClockUpdate();

    /* Lock protected registers */
    SYS_LockReg();
}

void Timer0_Init(void)
{
    // Give a dummy target frequency here. Will over write capture resolution with macro
    TIMER_Open(TIMER0, TIMER_PERIODIC_MODE, 1);

    // Enable timer interrupt
    TIMER_EnableInt(TIMER0);
    NVIC_EnableIRQ(TMR0_IRQn);


    // Start Timer 0
    TIMER_Start(TIMER0);
}

/*---------------------------------------------------------------------------------------------------------*/
/*  Main Function                                                                                          */
/*---------------------------------------------------------------------------------------------------------*/
int32_t main(void)
{
    int         cbs, ch;
    uint32_t    au32Config[2];

    /* Init System, IP clock and multi-function I/O */
    SYS_Init();
    /* Init UART to 115200-8n1 for print message */
    UART0_Init();
    Initial_LED();
    Timer0_Init();

    SYS_UnlockReg();

    /* Enable FMC ISP function */
    FMC_Open();

    printf("\n\n");
    printf("+--------------------------------------------------+\n");
    printf("|         User program running on APROM:0x%x       |\n", *(uint32_t*)0x4);
    printf("+--------------------------------------------------+\n");

    /*-------------------------------------------------------------
     *  Check Boot loader image
     *------------------------------------------------------------*/
    if(FMC_Read(FMC_LDROM_BASE) == 0xFFFFFFFF)
    {
        printf("Don't find boot loader\n");
        printf("Writing fmc_ld_boot.bin image to LDROM...\n");
        FMC_EnableLDUpdate();
        if(load_image_to_flash((uint32_t)&loaderImage1Base, (uint32_t)&loaderImage1Limit,
                               FMC_LDROM_BASE, FMC_LDROM_SIZE) != 0)
        {
            printf("Load image to LDROM failed!\n");
            return -1;
        }
        FMC_DisableLDUpdate();
        while(!IsDebugFifoEmpty());
        NVIC_SystemReset();
    }

    /*-------------------------------------------------------------
     *  Modify CBS to 00b (boot from APROM)
     *------------------------------------------------------------*/
    FMC_ReadConfig(au32Config, 2);
    cbs = (au32Config[0] >> 6) & 0x3;
    printf("Config0 = 0x%x, Config1 = 0x%x, CBS=%d\n\n", au32Config[0], au32Config[1], cbs);

    if(cbs)
    {
        printf("\n\nChange boot setting to [Boot from APROM].\n");
        FMC_EnableConfigUpdate();
        au32Config[0] &= ~0xc0;          /* set CBS to 00b */
        au32Config[0] |= 0x1;           /* disable Data Flash */
        FMC_WriteConfig(au32Config, 2);
    }
    while(1)
    {
        printf("\n\nDo you want to update AP1?(Yes/No)\n");
        while(1)
        {
            ch = getchar();
            if((ch == 'Y') || (ch == 'y'))
            {
                printf("Writing fmc_isp.bin image to APROM address 0x%x...\n", USER_AP1_ENTRY);
                FMC_EnableAPUpdate();
                if(load_image_to_flash((uint32_t)&loaderImage2Base, (uint32_t)&loaderImage2Limit,
                                       USER_AP1_ENTRY, USER_AP1_MAX_SIZE) != 0)
                {
                    printf("Load image to APROM failed!\n");
                    return -1;
                }
                FMC_DisableAPUpdate();
                break;
            }
            if((ch == 'N') || (ch == 'n')) break;
        }

        printf("\n\nDo you want to branch AP1?(Yes/No)\n");
        while(1)
        {
            ch = getchar();
            if((ch == 'Y') || (ch == 'y')) BranchTo(USER_AP1_ENTRY);
            if((ch == 'N') || (ch == 'n')) break;
        }
    }
}


static int  load_image_to_flash(uint32_t image_base, uint32_t image_limit, uint32_t flash_addr, uint32_t max_size)
{
    uint32_t   i, j, u32Data, u32ImageSize, *pu32Loader;

    u32ImageSize = image_limit - image_base;
    if(u32ImageSize == 0)
    {
        printf("  ERROR: Loader Image is 0 bytes!\n");
        return -1;
    }

    if(u32ImageSize > max_size)
    {
        printf("  ERROR: Loader Image is larger than %d KBytes!\n", max_size / 1024);
        return -1;
    }

    printf("Program image to flash address 0x%x...", flash_addr);
    pu32Loader = (uint32_t *)image_base;
    for(i = 0; i < u32ImageSize; i += FMC_FLASH_PAGE_SIZE)
    {
        if(FMC_Erase(flash_addr + i))
        {
            printf("Erase failed on 0x%x\n", flash_addr + i);
            return -1;
        }

        for(j = 0; j < FMC_FLASH_PAGE_SIZE; j += 4)
        {
            FMC_Write(flash_addr + i + j, pu32Loader[(i + j) / 4]);
        }
    }
    printf("OK.\n");

    printf("Verify ...");

    /* Verify loader */
    for(i = 0; i < u32ImageSize; i += FMC_FLASH_PAGE_SIZE)
    {
        for(j = 0; j < FMC_FLASH_PAGE_SIZE; j += 4)
        {
            u32Data = FMC_Read(flash_addr + i + j);
            if(u32Data != pu32Loader[(i + j) / 4])
            {
                printf("data mismatch on 0x%x, [0x%x], [0x%x]\n", flash_addr + i + j, u32Data, pu32Loader[(i + j) / 4]);
                return -1;
            }

            if(i + j >= u32ImageSize)
                break;
        }
    }
    printf("OK.\n");
    return 0;
}


