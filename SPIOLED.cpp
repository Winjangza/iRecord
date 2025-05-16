#include "SPIOLED.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <QThread>
#include <QDebug>
#include "font16.h"
// Hardware functions. --------------------------------------------------------

// ----------------------------------------------------------------------------
/*
    Writes a command.
*/
// ----------------------------------------------------------------------------
SPIOLED::SPIOLED(char *dc,   char *reset, char *spichannel)
{
    ssd1322_init(dc ,reset,spichannel);
}
// Hardware functions. --------------------------------------------------------

// ----------------------------------------------------------------------------
/*
    Writes a command.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_write_command( uint8_t command )
{
    uint8_t spi[1];
    ssd1322->gpio_dc->setval_gpio(SSD1322_INPUT_COMMAND);
    spi[0] = command;
    send_byte_data(spi, 1);
}

// ----------------------------------------------------------------------------
/*
    Writes a data byte.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_write_data( uint8_t data )
{
    uint8_t spi[1];
    ssd1322->gpio_dc->setval_gpio(SSD1322_INPUT_DATA );
    spi[0] = data;
    send_byte_data(spi, 1);
}
void SPIOLED::ssd1322_write_data( uint8_t *data , uint16_t len)
{
    ssd1322->gpio_dc->setval_gpio(SSD1322_INPUT_DATA );
    send_byte_data(data, len);
}
// ----------------------------------------------------------------------------
/*
    Writes a data stream.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_write_stream( uint8_t *buf, uint16_t count )
{
    ssd1322_write_command( SSD1322_CMD_SET_WRITE );
    ssd1322->gpio_dc->setval_gpio(SSD1322_INPUT_DATA );
    send_byte_data(buf, count);
}

// ----------------------------------------------------------------------------
/*
    Triggers a hardware reset:

    Hardware is reset when RES# pin is pulled low.
    For normal operation RES# pin should be pulled high.
    Redundant if RES# pin is wired to VCC.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_reset()
{
    ssd1322->gpio_reset->resetGpio();
    QThread::msleep(100);
    ssd1322->gpio_reset->setGpio();
    QThread::msleep(1000);
}

// ----------------------------------------------------------------------------
/*
    Enables grey scale lookup table.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_enable_greys(  )
{
    ssd1322_write_command( SSD1322_CMD_ENABLE_GREYS );
}

// ----------------------------------------------------------------------------
/*
    Sets column start & end addresses within display data RAM:

    0 <= start[6:0] < end[6:0] <= 0x77 (119).

    If horizontal address increment mode is enabled, column address pointer is
    automatically incremented after a column read/write. Column address pointer
    is reset after reaching the end column address.

    Note: columns are in groups of 4.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_cols( uint8_t start, uint8_t end )
{
    if (( end   < start ) ||
        ( start > SSD1322_COLS_MAX * 4 ) ||
        ( end   > SSD1322_COLS_MAX * 4 )) return;

    ssd1322_write_command( SSD1322_CMD_SET_COLS );
    ssd1322_write_data( start / 4 + SSD1322_COL_OFFSET );
    ssd1322_write_data( end / 4 + SSD1322_COL_OFFSET );
}

// ----------------------------------------------------------------------------
/*
    Resets column start & end addresses within display data RAM:

    Start = 0x00.
    End   = 0x77 (119).
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_cols_default(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_COLS );
    ssd1322_write_data( SSD1322_DEFAULT_COL1 );
    ssd1322_write_data( SSD1322_DEFAULT_COL2 );
}

// ----------------------------------------------------------------------------
/*
    Enables writing data continuously into display RAM.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_write_continuous(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_WRITE );
    ssd1322->gpio_dc->setval_gpio(SSD1322_INPUT_DATA );
}

// ----------------------------------------------------------------------------
/*
    Enables reading data continuously from display RAM. Not used in SPI mode.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_read_continuous(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_READ );
}

// ----------------------------------------------------------------------------
/*
    Sets column start & end addresses within display data RAM:

    0 <= Start[6:0] < End[6:0] <= 0x7f (127).

    If vertical address increment mode is enabled, column address pointer is
    automatically incremented after a column read/write. Column address pointer
    is reset after reaching the end column address.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_rows( uint8_t start, uint8_t end )
{
    if (( end   < start ) ||
        ( start > SSD1322_ROWS_MAX ) ||
        ( end   > SSD1322_ROWS_MAX )) return;

    ssd1322_write_command( SSD1322_CMD_SET_ROWS );
    ssd1322_write_data( start );
    ssd1322_write_data( end );
}

// ----------------------------------------------------------------------------
/*
    Resets row start & end addresses within display data RAM:

    Start = 0.
    End   = 0x7f (127).
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_rows_default(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_ROWS );
    ssd1322_write_data( SSD1322_DEFAULT_ROW1 );
    ssd1322_write_data( SSD1322_DEFAULT_ROW2 );
}

// ----------------------------------------------------------------------------
/*
    Sets various addressing configurations.

    a[0] = 0: Increment display RAM by column (horizontal).
    a[0] = 1: Increment display RAM by row (vertical).
    a[1] = 0: Columns incremented left to right.
    a[1] = 1: Columns incremented right to left.
    a[2] = 0: Direct mapping (reset).
    a[2] = 1: The four nibbles of the RAM data bus are re-mapped.
    a[3] = 0.
    a[4] = 0: Rows incremented from top to bottom.
    a[4] = 1: Rows incremented from bottom to top.
    a[5] = 0: Sequential pin assignment of COM pins.
    a[5] = 1: Odd/even split of COM pins.
    a[7:6] = 0x00.
    b[3:0] = 0x01.
    b[4] = 0: Disable dual COM mode.
    b[4] = 1: Enable dual COM mode. A[5] must be disabled and MUX <= 63.
    b[5] = 0.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_remap( uint8_t a, uint8_t b )
{
    ssd1322_write_command( SSD1322_CMD_SET_REMAP );
    ssd1322_write_data( a );
    ssd1322_write_data( b );
}

// ----------------------------------------------------------------------------
/*
    Sets default addressing configurations.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_remap_default(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_REMAP );
    ssd1322_write_data( SSD1322_DEFAULT_REMAP1 );
    ssd1322_write_data( SSD1322_DEFAULT_REMAP2 );
}

// ----------------------------------------------------------------------------
/*
    Sets display RAM start line.

    0 <= start <= 127 (0x7f).
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_start( uint8_t start )
{
    if ( start > SSD1322_ROWS_MAX ) return;

    ssd1322_write_command( SSD1322_CMD_SET_START );
    ssd1322_write_data( start );
}

// ----------------------------------------------------------------------------
/*
    Sets default display RAM start line.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_start_default(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_START );
    ssd1322_write_data( SSD1322_DEFAULT_START );
}

// ----------------------------------------------------------------------------
/*
    Sets display RAM offset.

    0 <= start <= 127 (0x7f).
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_offset( uint8_t offset )
{
    if ( offset > SSD1322_COLS_MAX ) return;

    ssd1322_write_command( SSD1322_CMD_SET_OFFSET );
    ssd1322_write_data( offset );
}

// ----------------------------------------------------------------------------
/*
    Sets default display RAM offset.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_offset_default(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_OFFSET );
    ssd1322_write_data( SSD1322_DEFAULT_OFFSET );
}

// ----------------------------------------------------------------------------
/*
    Sets display mode to normal (display shows contents of display RAM).
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_display_normal(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_PIX_NORM );
}

// ----------------------------------------------------------------------------
/*
    Sets all pixels on regardless of display RAM content.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_display_all_on(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_PIX_ON );
}

// ----------------------------------------------------------------------------
/*
    Sets all pixels off regardless of display RAM content.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_display_all_off(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_PIX_OFF );
}

// ----------------------------------------------------------------------------
/*
    Displays inverse of display RAM content.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_display_inverse(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_PIX_INV );
}

// ----------------------------------------------------------------------------
/*
    Enables partial display mode (subset of available display).

    0 <= start <= end <= 0x7f.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_part_display_on( uint8_t start, uint8_t end )
{
    if (( end   < start ) ||
        ( start > SSD1322_ROWS_MAX ) ||
        ( end   > SSD1322_ROWS_MAX )) return;

    ssd1322_write_command( SSD1322_CMD_SET_PART_ON );
    ssd1322_write_data( start );
    ssd1322_write_data( end );
}

// ----------------------------------------------------------------------------
/*
    Enables partial display mode (subset of available display).

    0 <= start <= end <= 0x7f.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_part_display_off(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_PART_OFF );
}

// ----------------------------------------------------------------------------
/*
    Enables internal VDD regulator.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_vdd_internal(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_VDD );
    ssd1322_write_data( SSD1322_VDD_INTERNAL );
}

// ----------------------------------------------------------------------------
/*
    Disables internal VDD regulator (requires external regulator).
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_vdd_external(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_VDD );
    ssd1322_write_data( SSD1322_VDD_EXTERNAL );
}

// ----------------------------------------------------------------------------
/*
    Turns display circuit on.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_display_on(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_DISP_ON );
}

// ----------------------------------------------------------------------------
/*
    Turns display circuit off.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_display_off(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_DISP_OFF );
}

// ----------------------------------------------------------------------------
/*
    Sets length of phase 1 & 2 of segment waveform driver.

    Phase 1: phase[3:0] Set from 5 DCLKS - 31 DCLKS in steps of 2. Higher OLED
             capacitance may require longer period to discharge.
    Phase 2: phase[7:4]Set from 3 DCLKS - 15 DCLKS in steps of 1. Higher OLED
             capacitance may require longer period to charge.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_phase( uint8_t phase )
{
    ssd1322_write_command( SSD1322_CMD_SET_PHASE );
    ssd1322_write_data( phase );
}

// ----------------------------------------------------------------------------
/*
    Sets default length of phase 1 & 2 of segment waveform driver.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_phase_default(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_PHASE );
    ssd1322_write_data( SSD1322_DEFAULT_PHASE );
}

// ----------------------------------------------------------------------------
/*
    Sets clock divisor/frequency.

    Front clock divider  A[3:0] = 0x0-0xa, reset = 0x1.
    Oscillator frequency A[7:4] = 0x0-0xf, reset = 0xc.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_clock( uint8_t clock )
{
    ssd1322_write_command( SSD1322_CMD_SET_CLOCK );
    ssd1322_write_data( clock );
}

// ----------------------------------------------------------------------------
/*
    Sets default clock divisor/frequency.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_clock_default(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_CLOCK );
    ssd1322_write_data( SSD1322_DEFAULT_CLOCK );
}

// ----------------------------------------------------------------------------
/*
    Sets display enhancement A.

    a[1:0] 0x00 = external VSL, 0x02 = internal VSL.
    a[7:2] = 0xa0.
    b[2:0] = 0x05.
    b[7:3] 0xf8 = enhanced, 0xb0 = normal.

    Typical values: a = 0xa0, b = 0xfd.

*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_enhance_a( uint8_t a, uint8_t b )
{
    ssd1322_write_command( SSD1322_CMD_SET_ENHANCE_A );
    ssd1322_write_data( a );
    ssd1322_write_data( b );
}

// ----------------------------------------------------------------------------
/*
    Sets default display enhancement A.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_enhance_a_default(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_ENHANCE_A );
    ssd1322_write_data( SSD1322_DEFAULT_ENHANCE_A1 );
    ssd1322_write_data( SSD1322_DEFAULT_ENHANCE_A2 );
}

// ----------------------------------------------------------------------------
/*
    Sets state of GPIO pins (GPIO0 & GPIO1).

    gpio[1:0] GPIO0
    gpio[3:2] GPIO1
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_gpios( uint8_t gpio )
{
    ssd1322_write_command( SSD1322_CMD_SET_GPIOS );
    ssd1322_write_data( gpio );
}

// ----------------------------------------------------------------------------
/*
    Sets default state of GPIO pins (GPIO0 & GPIO1).
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_gpios_default(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_GPIOS );
    ssd1322_write_data( SSD1322_DEFAULT_GPIOS );
}

// ----------------------------------------------------------------------------
/*
    Sets second pre-charge period.

    period[3:0] 0 to 15DCLK.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_period( uint8_t period )
{
    ssd1322_write_command( SSD1322_CMD_SET_PERIOD );
    ssd1322_write_data( period );
}

// ----------------------------------------------------------------------------
/*
    Sets default second pre-charge period.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_period_default(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_PERIOD );
    ssd1322_write_data( SSD1322_DEFAULT_PERIOD );
}

// ----------------------------------------------------------------------------
/*
    Sets user-defined greyscale table GS1 - GS15 (Not GS0).

    gs[7:0] 0 <= GS1 <= GS2 .. <= GS15.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_greys( uint8_t gs[SSD1322_GREYSCALES] )
{
    ssd1322_write_command( SSD1322_CMD_SET_GREYS );
    ssd1322_write_stream( gs, SSD1322_GREYSCALES );
    ssd1322_set_enable_greys(  );
}

// ----------------------------------------------------------------------------
/*
    Sets greyscale lookup table to default linear values.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_greys_default(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_GREYS_DEF );
}

// ----------------------------------------------------------------------------
/*
    Sets pre-charge voltage level for segment pins with reference to VCC.

    voltage[4:0] 0.2xVCC (0x00) to 0.6xVCC (0x3e).
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_pre_volt( uint8_t volts )
{
    ssd1322_write_command( SSD1322_CMD_SET_PRE_VOLT );
    ssd1322_write_data( volts );
}

// ----------------------------------------------------------------------------
/*
    Sets default pre-charge voltage level for segment pins.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_pre_volt_default(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_PRE_VOLT );
    ssd1322_write_data( SSD1322_DEFAULT_PRE_VOLT );
}

// ----------------------------------------------------------------------------
/*
    Sets the high voltage level of common pins, VCOMH with reference to VCC.

    voltage[3:0] 0.72xVCC (0x00) to 0.86xVCC (0x07).
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_com_volt( uint8_t volts )
{
    ssd1322_write_command( SSD1322_CMD_SET_COM_VOLT );
    ssd1322_write_data( volts );
}

// ----------------------------------------------------------------------------
/*
    Sets the default high voltage level of common pins, VCOMH.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_com_volt_default(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_COM_VOLT );
    ssd1322_write_data( SSD1322_DEFAULT_COM_VOLT );
}

// ----------------------------------------------------------------------------
/*
    Sets the display contrast (segment output current ISEG).

    contrast[7:0]. Contrast varies linearly from 0x00 to 0xff.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_contrast( uint8_t contrast )
{
    ssd1322_write_command( SSD1322_CMD_SET_CONTRAST );
    ssd1322_write_data( contrast );
}

// ----------------------------------------------------------------------------
/*
    Sets the default display contrast (segment output current ISEG).
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_contrast_default(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_CONTRAST );
    ssd1322_write_data( SSD1322_DEFAULT_CONTRAST );
}

// ----------------------------------------------------------------------------
/*
    Sets the display brightness (segment output current scaling factor).

    factor[3:0] 1/16 (0x00) to 16/16 (0x0f).
    The smaller the value, the dimmer the display.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_brightness( uint8_t factor )
{
    ssd1322_write_command( SSD1322_CMD_SET_BRIGHTNESS );
    ssd1322_write_data( factor );
}

// ----------------------------------------------------------------------------
/*
    Sets the default display brightness.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_brightness_default(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_BRIGHTNESS );
    ssd1322_write_data( SSD1322_DEFAULT_BRIGHTNESS );
}

// ----------------------------------------------------------------------------
/*
    Sets multiplex ratio (number of common pins enabled)

    ratio[6:0] 16 (0x00) to 128 (0x7f).
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_mux( uint8_t ratio )
{
    ssd1322_write_command( SSD1322_CMD_SET_MUX );
    ssd1322_write_data( ratio );
}

// ----------------------------------------------------------------------------
/*
    Sets default multiplex ratio (number of common pins enabled)
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_mux_default(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_MUX );
    ssd1322_write_data( SSD1322_DEFAULT_MUX );
}

// ----------------------------------------------------------------------------
/*
    Sets display enhancement B.

    a[3:0] = 0x02.
    a[5:4] 0x00 = reserved, 0x02 = normal (reset).
    a[7:6] = 0x80.
    b[7:0] = 0x20.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_enhance_b( uint8_t a, uint8_t b )
{
    ssd1322_write_command( SSD1322_CMD_SET_ENHANCE_B );
    ssd1322_write_data( a );
    ssd1322_write_data( b );
}

// ----------------------------------------------------------------------------
/*
    Sets default display enhancement B.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_enhance_b_default(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_ENHANCE_B );
    ssd1322_write_data( SSD1322_DEFAULT_ENHANCE_B1 );
    ssd1322_write_data( SSD1322_DEFAULT_ENHANCE_B2 );
}

// ----------------------------------------------------------------------------
/*
    Sets command lock - prevents all command except unlock.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_command_lock(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_LOCK );
    ssd1322_write_data( SSD1322_COMMAND_LOCK );
}

// ----------------------------------------------------------------------------
/*
    Unsets command lock.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_command_unlock(  )
{
    ssd1322_write_command( SSD1322_CMD_SET_LOCK );
    ssd1322_write_data( SSD1322_COMMAND_UNLOCK );
}

// ----------------------------------------------------------------------------
/*
    Clears display RAM.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_clear_display(  )
{
    ssd1322_set_cols_default(  );
    qDebug() << "ssd1322_set_cols_default";
    ssd1322_set_rows_default(  );
    qDebug() << "ssd1322_set_rows_default";
    ssd1322_set_write_continuous(  );
    qDebug() << "ssd1322_set_write_continuous";
    uint8_t col, row;
    uint8_t clearPixel[128*16] = {0x0, };
    for ( row = SSD1322_ROWS_MIN; row <= SSD1322_ROWS_MAX+1; row+=16 )
    {
        ssd1322_write_data(clearPixel,128*16);
    }
}

// ----------------------------------------------------------------------------
/*
    Sets default display operation settings.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_defaults(  )
{
    ssd1322_set_command_unlock(  );
    ssd1322_set_clock_default(  );
    ssd1322_set_mux_default(  );
    ssd1322_set_offset_default(  );
    ssd1322_set_start_default(  );
    ssd1322_set_remap_default(  );
    ssd1322_set_gpios_default(  );
    ssd1322_set_vdd_internal(  );
    ssd1322_set_enhance_a_default(  );
    ssd1322_set_contrast_default(  );
    ssd1322_set_brightness_default(  );
    ssd1322_set_greys_default(  );
    ssd1322_set_phase_default(  );
    ssd1322_set_enhance_b_default(  );
    ssd1322_set_pre_volt_default(  );
    ssd1322_set_period_default(  );
    ssd1322_set_com_volt_default(  );
    ssd1322_set_display_normal(  );
    ssd1322_set_part_display_off(  );
    QThread::msleep(1000);
}

// ----------------------------------------------------------------------------
/*
    Sets typical display operation settings.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_set_typical(  )
{
    ssd1322_set_command_unlock(  );
    ssd1322_set_display_off(  );
    ssd1322_set_cols( 0x1c, 0x5b );
    ssd1322_set_rows( 0x00, 0x3f );
    ssd1322_set_clock( 0x91 );
    ssd1322_set_mux( 0x3f );
    ssd1322_set_offset( 0x00 );
    ssd1322_set_start( 0x00 );
    ssd1322_set_remap( 0x14, 0x11 );
    ssd1322_set_gpios( 0x00 );
    ssd1322_set_vdd_internal(  );
    ssd1322_set_enhance_a( 0xa0, 0xfd );
    ssd1322_set_contrast( 0x9f );
    ssd1322_set_brightness( 0x04 );
    ssd1322_set_greys_default(  );
    ssd1322_set_phase( 0xe2 );
    ssd1322_set_enhance_b( 0x00, 0x20 );
    ssd1322_set_pre_volt( 0x1f );
    ssd1322_set_period( 0x08 );
    ssd1322_set_com_volt( 0x07 );
    ssd1322_set_display_normal(  );
    ssd1322_set_part_display_off(  );
    ssd1322_set_display_on(  );
    QThread::msleep(1000);
}

// ----------------------------------------------------------------------------
/*
    Initialises display.
*/
// ----------------------------------------------------------------------------
void SPIOLED::ssd1322_init( char *dc,   char  *reset, char *spichannel)
{
    static bool init = false;       // 1st Call to init.

    ssd1322 = new ssd1322_t;
    ssd1322->gpio_dc = new GPIOClass(dc);
    ssd1322->gpio_reset = new GPIOClass(reset);
    spi_init(spichannel);
    init = true;

    QThread::msleep(100);
    ssd1322->gpio_dc->setdir_gpio("out");
    ssd1322->gpio_reset->setdir_gpio("out");
    ssd1322->gpio_dc->setGpio();
    ssd1322->gpio_reset->setGpio();

    ssd1322_reset(  );
    qDebug() << "ssd1322_reset";
    ssd1322_set_typical(  );
    qDebug() << "ssd1322_set_typical";
    ssd1322_clear_display(  );
    qDebug() << "ssd1322_clear_display";

}

void SPIOLED::spi_init(char* spidev)
{
    fd_spi = open(spidev, O_RDWR);
    if (fd_spi < 0)
        qDebug() << "can't open device";
    ret_spi = ioctl(fd_spi, SPI_IOC_WR_MODE, &mode_spi);
    if (ret_spi == -1)
        qDebug() << ("can't set spi mode");
    ret_spi = ioctl(fd_spi, SPI_IOC_RD_MODE, &mode_spi);
    if (ret_spi == -1)
        qDebug() << ("can't get spi mode");
    ret_spi = ioctl(fd_spi, SPI_IOC_WR_BITS_PER_WORD, &bits);
    if (ret_spi == -1)
        qDebug() << ("can't set bits per word");
    ret_spi = ioctl(fd_spi, SPI_IOC_RD_BITS_PER_WORD, &bits);
    if (ret_spi == -1)
        qDebug() << ("can't get bits per word");
    ret_spi = ioctl(fd_spi, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if (ret_spi == -1)
        qDebug() << ("can't set max speed hz");
    ret_spi = ioctl(fd_spi, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
    if (ret_spi == -1)
        qDebug() << ("can't get max speed hz");

    printf("spi mode: 0x%x\n", mode_spi);
    printf("bits per word: %d\n", bits);
    printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);

}

void SPIOLED::clear_rx(){
    for (int i = 0; i < 10; i++){
        rx[i]  = 0;
    }
}
int SPIOLED::send_byte_data(uint8_t *txByteData, uint16_t len)
{
    uint8_t *rxByteData;
    struct spi_ioc_transfer tr = {
        tr.tx_buf = (unsigned long)txByteData,
        tr.rx_buf = (unsigned long)rxByteData,
        tr.len = len,
        tr.delay_usecs = delay,
        tr.speed_hz = speed,
        tr.bits_per_word = bits,
    };
    if (mode_spi & SPI_TX_QUAD)
        tr.tx_nbits = 4;
    else if (mode_spi & SPI_TX_DUAL)
        tr.tx_nbits = 2;
    if (mode_spi & SPI_RX_QUAD)
        tr.rx_nbits = 4;
    else if (mode_spi & SPI_RX_DUAL)
        tr.rx_nbits = 2;
    if (!(mode_spi & SPI_LOOP)) {
        if (mode_spi & (SPI_TX_QUAD | SPI_TX_DUAL))
            tr.rx_buf = 0;
        else if (mode_spi & (SPI_RX_QUAD | SPI_RX_DUAL))
            tr.tx_buf = 0;
    }
    ret_spi = ioctl(fd_spi, SPI_IOC_MESSAGE(1), &tr);
    if (ret_spi < 0)
        qDebug() << "can't send_byte_data" << ret_spi;
    return ret_spi;
}

uint8_t SPIOLED::fontConv(bool value)
{
    if (value) return 0x0F;
    return 0;
}
void SPIOLED::test_checkerboard()
{
    uint8_t line[16][128];
    uint8_t lines[16*128];
    uint8_t chr_f16_4D[32] =         // 2 unsigned chars per row
    {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC1, 0x80, 0xC1, 0x80, 0xA2, 0x80,    // row 1 - 6
            0xA2, 0x80, 0x94, 0x80, 0x94, 0x80, 0x88, 0x80, 0x88, 0x80, 0x80, 0x80,    // row 7 - 12
            0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00                             // row 13 - 16
    };
    for ( uint8_t row = 0; row < 16; row++ )
    {
        for ( uint8_t col = 0; col < 128; col++ ){
            line[row][col] = 0;
        }
    }

    for ( uint8_t row = 0; row < 16; row++ )
    {

        for (int col = 0;col < 16; col++) {
            line[row][(col*8)+0]   = ((fontConv((chr_f16_4D[2*row] & 0x80) >> 7)) << 4) + fontConv((chr_f16_4D[2*row] & 0x40) >> 6);
            line[row][(col*8)+1] = ((fontConv((chr_f16_4D[2*row] & 0x20) >> 5)) << 4) + fontConv((chr_f16_4D[2*row] & 0x10) >> 4);
            line[row][(col*8)+2] = ((fontConv((chr_f16_4D[2*row] & 0x08) >> 3)) << 4) + fontConv((chr_f16_4D[2*row] & 0x04) >> 2);
            line[row][(col*8)+3] = ((fontConv((chr_f16_4D[2*row] & 0x02) >> 1)) << 4) + fontConv((chr_f16_4D[2*row] & 0x01) >> 0);

            line[row][(col*8)+4] = ((fontConv((chr_f16_4D[(2*row)+1] & 0x80) >> 7)) << 4) + fontConv((chr_f16_4D[(2*row)+1] & 0x40) >> 6);
            line[row][(col*8)+5] = ((fontConv((chr_f16_4D[(2*row)+1] & 0x20) >> 5)) << 4) + fontConv((chr_f16_4D[(2*row)+1] & 0x10) >> 4);
            line[row][(col*8)+6] = ((fontConv((chr_f16_4D[(2*row)+1] & 0x08) >> 3)) << 4) + fontConv((chr_f16_4D[(2*row)+1] & 0x04) >> 2);
            line[row][(col*8)+7] = ((fontConv((chr_f16_4D[(2*row)+1] & 0x02) >> 1)) << 4) + fontConv((chr_f16_4D[(2*row)+1] & 0x01) >> 0);
        }
    }
    for ( uint8_t row = 0; row < 16; row++ )
    {
        for ( uint8_t col = 0; col < 128; col++ ){
            lines[(row*128)+col] = line[row][col];
        }
    }


    ssd1322_set_cols(0,255 );
    ssd1322_set_rows(0,63 );
    ssd1322_set_write_continuous();
    ssd1322_write_data(lines,16*128);
}

void SPIOLED::ascii2bitmap(uint8_t lineNum, uint8_t startCol, uint8_t *ascii, bool doubleCharPerRow)
{
    uint8_t line[16][128];

    for ( uint8_t row = 0; row < 16; row++ )
    {
        for ( uint8_t col = startCol; col < 128; col++ ){
            line[row][col] = 0;
        }
    }

    if (doubleCharPerRow)
    {
        uint8_t col = startCol;
        for ( uint8_t row = 0; row < 16; row++ )
        {
            line[row][col+0] = ((fontConv((ascii[2*row] & 0x80) >> 7)) << 4) + fontConv((ascii[2*row] & 0x40) >> 6);
            line[row][col+1] = ((fontConv((ascii[2*row] & 0x20) >> 5)) << 4) + fontConv((ascii[2*row] & 0x10) >> 4);
            line[row][col+2] = ((fontConv((ascii[2*row] & 0x08) >> 3)) << 4) + fontConv((ascii[2*row] & 0x04) >> 2);
            line[row][col+3] = ((fontConv((ascii[2*row] & 0x02) >> 1)) << 4) + fontConv((ascii[2*row] & 0x01) >> 0);

            line[row][col+4] = ((fontConv((ascii[(2*row)+1] & 0x80) >> 7)) << 4) + fontConv((ascii[(2*row)+1] & 0x40) >> 6);
            line[row][col+5] = ((fontConv((ascii[(2*row)+1] & 0x20) >> 5)) << 4) + fontConv((ascii[(2*row)+1] & 0x10) >> 4);
            line[row][col+6] = ((fontConv((ascii[(2*row)+1] & 0x08) >> 3)) << 4) + fontConv((ascii[(2*row)+1] & 0x04) >> 2);
            line[row][col+7] = ((fontConv((ascii[(2*row)+1] & 0x02) >> 1)) << 4) + fontConv((ascii[(2*row)+1] & 0x01) >> 0);
        }
    }
    else
    {
        uint8_t col = startCol;
        for ( uint8_t row = 0; row < 16; row++ )
        {
            line[row][col+0] = ((fontConv((ascii[row] & 0x80) >> 7)) << 4) + fontConv((ascii[row] & 0x40) >> 6);
            line[row][col+1] = ((fontConv((ascii[row] & 0x20) >> 5)) << 4) + fontConv((ascii[row] & 0x10) >> 4);
            line[row][col+2] = ((fontConv((ascii[row] & 0x08) >> 3)) << 4) + fontConv((ascii[row] & 0x04) >> 2);
            line[row][col+3] = ((fontConv((ascii[row] & 0x02) >> 1)) << 4) + fontConv((ascii[row] & 0x01) >> 0);
        }
    }
    for ( uint8_t row = 0; row < 16; row++ )
    {
        for ( uint8_t col = startCol; col < 128; col++ ){
            switch (lineNum) {
            case 1:
                line1[row][col] += line[row][col];
                break;
            case 2:
                line2[row][col] += line[row][col];
                break;
            case 3:
                line3[row][col] += line[row][col];
                break;
            case 4:
                line4[row][col] += line[row][col];
                break;
            case 5:
                line5[row][col] += line[row][col];
                break;
            }

        }
    }
}

void SPIOLED::printLine(QString text, uint8_t lineNum, uint8_t textAlign)
{
    uint8_t lines[16*128];
    QByteArray byteData(text.toLocal8Bit());
    for ( uint8_t row = 0; row < 16; row++ )
    {
        for ( uint8_t col = 0; col < 128; col++ ){
            switch (lineNum)
            {
            case 1:
                line1[row][col] = 0;
                break;
            case 2:
                line2[row][col] = 0;
                break;
            case 3:
                line3[row][col] = 0;
                break;
            case 4:
                line4[row][col] = 0;
                break;
            case 5:
                line5[row][col] = 0;
                break;
            }

        }
    }
    uint8_t colStartPlot = 0;
    uint8_t colStopPlot = 255;
    uint8_t startCol = 0;
    uint8_t lastPixel = 0;
    uint8_t pixelLength = 0;
    int i = 0;
    Q_FOREACH(uint8_t ascii, byteData)
    {
        uint8_t *fontData = (chrtbl_f16[ascii-32]);
        bool doubleCharPerRow = false;
        if ((ascii == 'W') || (ascii == 'M'))doubleCharPerRow = true;
        if (startCol < 128)
        ascii2bitmap(lineNum,startCol,fontData,doubleCharPerRow);
        startCol += widtbl_f16[ascii-32] - 3;
        QString chardata = QString::number(ascii,16);
//        qDebug() << byteData.at(i) << ascii << widtbl_f16[ascii-32]; i++;
    }
//    for ( uint8_t row = 0; row < 16; row++ )
//    {
//        for ( uint8_t col = 0; col < 128; col++ ){
//            if (lines[(row*128)+col] != 0) pixelLength = col;

//            switch (lineNum)
//            {
//            case 1:
//                lines[(row*128)+col] = line1[row][col];
//                break;
//            case 2:
//                lines[(row*128)+col] = line2[row][col];
//                break;
//            case 3:
//                lines[(row*128)+col] = line3[row][col];
//                break;
//            case 4:
//                lines[(row*128)+col] = line4[row][col];
//                break;
//            case 5:
//                lines[(row*128)+col] = line5[row][col];
//                break;
//            }

//        }
//    }
    if (textAlign == ALIGNCENTER)
    {
        colStartPlot = (128-startCol)/2;
        if (colStartPlot < 0) colStartPlot = 0;
    }
    else if (textAlign == ALIGNRIGHT) {
        colStartPlot = 128-startCol;
    }
    else {
        colStartPlot = 0;
    }
    for ( uint8_t row = 0; row < 16; row++ )
    {
        for ( uint8_t col = 0; col < 128; col++ ){
            if (lines[(row*128)+col] != 0) pixelLength = col;

            switch (lineNum)
            {
            case 1:
                if (col<colStartPlot){
                    lines[(row*128)+col] = 0;
                }
                else {
                    lines[(row*128)+col] = line1[row][col-colStartPlot];
                }
                break;
            case 2:
                if (col<colStartPlot){
                    lines[(row*128)+col] = 0;
                }
                else {
                    lines[(row*128)+col] = line2[row][col-colStartPlot];
                }
                break;
            case 3:
                if (col<colStartPlot){
                    lines[(row*128)+col] = 0;
                }
                else {
                    lines[(row*128)+col] = line3[row][col-colStartPlot];
                }
                break;
            case 4:
                if (col<colStartPlot){
                    lines[(row*128)+col] = 0;
                }
                else {
                    lines[(row*128)+col] = line4[row][col-colStartPlot];
                }
                break;
            }

        }
    }
    ssd1322_set_cols(0,255);
    ssd1322_set_rows(((lineNum-1)*16),63 );
    ssd1322_set_write_continuous();
    ssd1322_write_data(lines,16*128);
}

