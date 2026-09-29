#include <rp5c01.h>

__sfr __at (0xb4) RTC_REGSEL;
__sfr __at (0xb5) RTC_DATA;

uint8_t rp5c01_read_reg(uint8_t reg)
{
    RTC_REGSEL = reg & 0xf;
    return RTC_DATA;
}

void rp5c01_write_reg(uint8_t reg, uint8_t value)
{
    RTC_REGSEL = reg & 0xf;
    RTC_DATA = value;
}

uint_fast8_t plt_rtc_secs(void)
{
    return rp5c01_rtc_secs();
}

/*
 *	Full clock access through /dev/rtc (CONFIG_RTC_FULL): setdate reads it
 *	at boot and then no longer asks for the date and time. The RP5C01 keeps
 *	one BCD digit per register (the upper bits read back as ones) and, as
 *	the MSX BIOS sets it, the year in two digits counted from 1980.
 */
#include <kdata.h>
#include <rtc.h>

static uint8_t get2(uint8_t reg)
{
    return (rp5c01_read_reg(reg) & 0x0F) + (rp5c01_read_reg(reg + 1) & 0x0F) * 10;
}

static void set2(uint8_t reg, uint8_t value)
{
    rp5c01_write_reg(reg, value % 10);
    rp5c01_write_reg(reg + 1, value / 10);
}

int plt_rtc_read(void)
{
    struct cmos_rtc cmos;
    uint8_t *p = cmos.data.bytes;
    uint16_t len = sizeof(cmos);
    uint16_t year;
    uint8_t sec;
    irqflags_t irq;

    if (udata.u_count < len)
        len = udata.u_count;
    irq = di();
    rp5c01_write_reg(MODE_SEL, TIMER_ENABLE | MODE00);
    /* Read again if a second went by meanwhile (and maybe carried) */
    do {
        sec = get2(REG_1_SEC_CTR);
        year = 1980 + get2(REG_1_YEAR_CTR);
        p[2] = get2(REG_1_MONTH_CTR) - 1;
        p[3] = get2(REG_1_DAY_CTR);
        p[4] = get2(REG_1_HOUR_CTR);
        p[5] = get2(REG_1_MIN_CTR);
    } while (sec != get2(REG_1_SEC_CTR));
    irqrestore(irq);
    p[0] = year;
    p[1] = year >> 8;
    p[6] = sec;
    cmos.type = CMOS_RTC_DEC;
    if (uput(&cmos, udata.u_base, len) == -1)
        return -1;
    return len;
}

int plt_rtc_write(void)
{
    struct cmos_rtc cmos;
    uint8_t *p = cmos.data.bytes;
    uint16_t year;
    irqflags_t irq;

    if (udata.u_count != sizeof(cmos)) {
        udata.u_error = EINVAL;
        return -1;
    }
    if (uget(udata.u_base, &cmos, sizeof(cmos)) == -1)
        return -1;
    if (cmos.type != CMOS_RTC_DEC) {
        udata.u_error = EINVAL;
        return -1;
    }
    year = p[0] | (p[1] << 8);
    irq = di();
    /* Clock stopped while the digits are set */
    rp5c01_write_reg(MODE_SEL, MODE00);
    set2(REG_1_YEAR_CTR, year - 1980);
    set2(REG_1_MONTH_CTR, p[2] + 1);
    set2(REG_1_DAY_CTR, p[3]);
    set2(REG_1_HOUR_CTR, p[4]);
    set2(REG_1_MIN_CTR, p[5]);
    set2(REG_1_SEC_CTR, p[6]);
    rp5c01_write_reg(MODE_SEL, TIMER_ENABLE | MODE00);
    irqrestore(irq);
    return sizeof(cmos);
}
