#include <common.h>
#include <command.h>
#include <asm/mipsregs.h>
#include <asm/addrspace.h>
#include <config.h>
#include <version.h>
#include "ar7240_soc.h"

#ifdef CONFIG_ATH_NAND_BR
#include <nand.h>
#endif

#define SETBITVAL(val, pos, bit) do {ulong bitval = (bit) ? 0x1 : 0x0; (val) = ((val) & ~(0x1 << (pos))) | ( (bitval) << (pos));} while(0)


extern int wasp_ddr_initial_config(uint32_t refresh);
extern int ar7240_ddr_find_size(void);

#ifdef COMPRESSED_UBOOT
#	define prmsg(...)
#	define args		char *s
#	define board_str(a)	do {			\
	char ver[] = "0";				\
	strcpy(s, "U-Boot " a "Wasp 1.");		\
	ver[0] += ar7240_reg_rd(AR7240_REV_ID) & 0xf;	\
	strcat(s, ver);					\
} while (0)
#else
#	define prmsg		printf
#	define args		void
#	define board_str(a)	printf(a)
#endif

void
wasp_usb_initial_config(void)
{
#define unset(a)	(~(a))

	if ((ar7240_reg_rd(WASP_BOOTSTRAP_REG) & WASP_REF_CLK_25) == 0) {
		ar7240_reg_wr_nf(AR934X_SWITCH_CLOCK_SPARE,
			ar7240_reg_rd(AR934X_SWITCH_CLOCK_SPARE) |
			SWITCH_CLOCK_SPARE_USB_REFCLK_FREQ_SEL_SET(2));
	} else {
		ar7240_reg_wr_nf(AR934X_SWITCH_CLOCK_SPARE,
			ar7240_reg_rd(AR934X_SWITCH_CLOCK_SPARE) |
			SWITCH_CLOCK_SPARE_USB_REFCLK_FREQ_SEL_SET(5));
	}

	udelay(1000);
	ar7240_reg_wr(AR7240_RESET,
		ar7240_reg_rd(AR7240_RESET) |
		RST_RESET_USB_PHY_SUSPEND_OVERRIDE_SET(1));
	udelay(1000);
	ar7240_reg_wr(AR7240_RESET,
		ar7240_reg_rd(AR7240_RESET) &
		unset(RST_RESET_USB_PHY_RESET_SET(1)));
	udelay(1000);
	ar7240_reg_wr(AR7240_RESET,
		ar7240_reg_rd(AR7240_RESET) &
		unset(RST_RESET_USB_PHY_ARESET_SET(1)));
	udelay(1000);
	ar7240_reg_wr(AR7240_RESET,
		ar7240_reg_rd(AR7240_RESET) &
		unset(RST_RESET_USB_HOST_RESET_SET(1)));
	udelay(1000);
	if ((ar7240_reg_rd(AR7240_REV_ID) & 0xf) == 0) {
		/* Only for WASP 1.0 */
		ar7240_reg_wr(0xb8116c84 ,
			ar7240_reg_rd(0xb8116c84) & unset(1<<20));
	}
}

void wasp_gpio_config(void)
{
	/* disable the CLK_OBS on GPIO_4 and set GPIO4 as input */
	ar7240_reg_rmw_clear(GPIO_OE_ADDRESS, (1 << 4));
	ar7240_reg_rmw_clear(GPIO_OUT_FUNCTION1_ADDRESS, GPIO_OUT_FUNCTION1_ENABLE_GPIO_4_MASK);
	ar7240_reg_rmw_set(GPIO_OUT_FUNCTION1_ADDRESS, GPIO_OUT_FUNCTION1_ENABLE_GPIO_4_SET(0x80));
	ar7240_reg_rmw_set(GPIO_OE_ADDRESS, (1 << 4));
}

int
wasp_mem_config(void)
{
	extern void ath_ddr_tap_cal(void);
	unsigned int type, reg32;

	type = wasp_ddr_initial_config(CFG_DDR_REFRESH_VAL);

	ath_ddr_tap_cal();

	prmsg("Tap value selected = 0x%x [0x%x - 0x%x]\n",
		ar7240_reg_rd(AR7240_DDR_TAP_CONTROL0),
		ar7240_reg_rd(0xbd007f10), ar7240_reg_rd(0xbd007f14));

	/* Take WMAC out of reset */
	reg32 = ar7240_reg_rd(AR7240_RESET);
	reg32 = reg32 &  ~AR7240_RESET_WMAC;
	ar7240_reg_wr_nf(AR7240_RESET, reg32);

#if !defined(CONFIG_ATH_NAND_BR)
	/* Switching regulator settings */
	ar7240_reg_wr_nf(0x18116c40, 0x633c8176); /* AR_PHY_PMU1 */
	ar7240_reg_wr_nf(0x18116c44, 0x10380000); /* AR_PHY_PMU2 */

	wasp_usb_initial_config();

#endif /* !defined(CONFIG_ATH_NAND_BR) */

	wasp_gpio_config();

	reg32 = ar7240_ddr_find_size();

	return reg32;
}

long int initdram(int board_type)
{
	return (wasp_mem_config());
}

int	checkboard(args)
{
#if CONFIG_AP123
	board_str("AP123\n");
#elif CONFIG_MI124
	board_str("MI124\n");
#else
	board_str("DB120\n");
#endif
	return 0;
}


//add by greping at 2015.6.11
void all_led_on(void){
	unsigned int gpio;

	gpio = ar7240_reg_rd(AR934X_GPIO_OUT);


	SETBITVAL(gpio, GPIO_SYS_LED_BIT,      GPIO_SYS_LED_ON);

	ar7240_reg_wr(AR934X_GPIO_OUT, gpio);
}

void all_led_off(void){
	unsigned int gpio;

	gpio = ar7240_reg_rd(AR934X_GPIO_OUT);


	SETBITVAL(gpio, GPIO_SYS_LED_BIT,      !GPIO_SYS_LED_ON);

	ar7240_reg_wr(AR934X_GPIO_OUT, gpio);
}
int reset_button_status(void){
	unsigned int gpio;

	//set GPIO_RST_BUTTON_BIT as input
	ar7240_reg_rmw_set(AR934X_GPIO_BASE, (1 << GPIO_RST_BUTTON_BIT));

	gpio = ar7240_reg_rd(AR934X_GPIO_IN);

	if(gpio & (1 << GPIO_RST_BUTTON_BIT)){
#if defined(GPIO_RST_BUTTON_IS_ACTIVE_LOW)
		return(0);
#else
		return(1);
#endif
	} else {
#if defined(GPIO_RST_BUTTON_IS_ACTIVE_LOW)
		return(1);
#else
		return(0);
#endif
	}
}
