#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

// Undefine this to use big banging to implement the parallel protocol.
// Otherwise PIO with DMA will be used. Bitbanging is slower but allows
// to drive the display if all the PIO state machines or the PIO FIFO
// are used for other goals.
#define st77_parallel_bb

#ifndef st77_parallel_bb
#include "hardware/dma.h"
#include "hardware/pio.h"
#endif

void st77xx_fill(uint16_t c);

#ifdef st77_use_spi
// Bus setup: SPI version. Very straightforward.
void st77xx_init_spi(void) {
    // SPI initializatin.
    gpio_init(st77_dc);
    gpio_set_dir(st77_dc,GPIO_OUT);
    if (st77_rst != -1) {
        gpio_init(st77_rst);
        gpio_set_dir(st77_rst,GPIO_OUT);
    }
    if (st77_cs != -1) {
        gpio_init(st77_cs);
        gpio_set_dir(st77_cs,GPIO_OUT);
    }
    spi_init(spi_channel,spi_rate);
    spi_set_format(spi_channel, 8, spi_polarity, spi_phase, SPI_MSB_FIRST);
    gpio_set_function(st77_sck,GPIO_FUNC_SPI);
    gpio_set_function(st77_mosi,GPIO_FUNC_SPI);
}
#endif

#ifdef st77_use_parallel
#ifdef st77_parallel_bb
void st77xx_init_parallel(void) {
    gpio_init(st77_dc);
    gpio_set_dir(st77_dc,GPIO_OUT);

    if (st77_rst != -1) {
        gpio_init(st77_rst);
        gpio_set_dir(st77_rst,GPIO_OUT);
    }
    if (st77_cs != -1) {
        gpio_init(st77_cs);
        gpio_set_dir(st77_cs,GPIO_OUT);
    }

    // The read clock pin is never used to actually read from the
    // display. If available, it's configured only to put it into a
    // known state.
    if (st77_rd != -1) {
        gpio_init(st77_rd);
        gpio_set_dir(st77_rd,GPIO_OUT);
        gpio_put(st77_rd,1);
    }

    // Configure write clock and data lines.
    gpio_init(st77_wr);
    gpio_set_dir(st77_wr,GPIO_OUT);
    gpio_put(st77_wr,1);
    for (int j = 0; j < 8; j++) {
        gpio_init(st77_d0+j);
        gpio_set_dir(st77_d0+j,GPIO_OUT);
    }
}

#else // Parallel with PIO

static unsigned int st77_sm, st77_dma;

// Bus setup: Parallel 8 lines using PIO and DMA. A bit more convoluted.
void st77xx_init_parallel(void) {
    unsigned int offset;

    /* ========================= Non PIO pins setup  ======================== */
    
    // Data/Command pin.
    gpio_init(st77_dc);
    gpio_set_dir(st77_dc,GPIO_OUT);

    // The read clock pin is never used to actually read from the
    // display. If available, it's configured only to put it into a
    // known state (high = no reading).
    if (st77_rd != -1) {
        gpio_set_function(st77_rd,GPIO_FUNC_SIO);
        gpio_set_dir(st77_rd,GPIO_OUT);
        gpio_put(st77_rd,1);
    }

    // Client selection and reset are optional pins as well, but
    // if defined we need to use them.
    if (st77_rst != -1) {
        gpio_init(st77_rst);
        gpio_set_dir(st77_rst,GPIO_OUT);
    }
    if (st77_cs != -1) {
        gpio_init(st77_cs);
        gpio_set_dir(st77_cs,GPIO_OUT);
    }

    /* ============================= PIO setup ============================== */

    static const uint16_t pio_program_data[] = {
        0x6008, //  0: out    pins, 8         side 0
        0x90e0, //  1: pull   ifempty block   side 1
    };

    static const struct pio_program pp = {
        .instructions = pio_program_data,
        .length = 2,
        .origin = -1,
    };

    // Get a state machine and load the program.
    st77_sm = pio_claim_unused_sm(pio_channel,true);
    offset = pio_add_program(pio_channel,&pp);

    // Setup all the pins used by the state machine, as PIO pins.
    pio_gpio_init(pio_channel, st77_wr);
    for (int j = 0; j < 8; j++) pio_gpio_init(pio_channel, st77_d0+j);

    // Associate data and WR clock pins to the state machine.
    pio_sm_set_consecutive_pindirs(pio_channel, st77_sm, st77_d0, 8, true);
    pio_sm_set_consecutive_pindirs(pio_channel, st77_sm, st77_wr, 1, true);

    // Configure the state machine:
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset+1); // Program is 2 instructions.
    sm_config_set_sideset(&c, 1, false, false); // 1 sideset pin.
    sm_config_set_out_pins(&c,st77_d0,8);       // 8 output pins.
    sm_config_set_sideset_pins(&c,st77_wr);     // Specify sideset pin.

    // Set a 8 words FIFO using the 4 RX and 4 TX FIFOs.
    sm_config_set_fifo_join(&c,PIO_FIFO_JOIN_TX);
    // Use shift_right = false, autopool = true with threshold of 8 bits.
    sm_config_set_out_shift(&c,false,true,8);

    // Set the PIO clock divider according to the current system clock
    uint32_t wanted_mhz = 32000000;
    uint32_t cpu_mhz = clock_get_hz(clk_sys);
    // We add wanted_mhz-1 to be sure to get a clock lower or equal,
    // but lever higher than the wanted one.
    uint32_t div = (cpu_mhz+(wanted_mhz-1)) / wanted_mhz;
    printf("ST77 parallel ST clock divider set to %d\n", div);
    sm_config_set_clkdiv(&c, div);

    // Start the state machine.
    pio_sm_init(pio_channel,st77_sm,offset,&c);
    pio_sm_set_enabled(pio_channel,st77_sm,true);

    // Configure the DMA channel.
    st77_dma = dma_claim_unused_channel(true);
    dma_channel_config dmc = dma_channel_get_default_config(st77_dma);
    channel_config_set_transfer_data_size(&dmc,DMA_SIZE_8);
    channel_config_set_bswap(&dmc,false);
    channel_config_set_dreq(&dmc,pio_get_dreq(pio_channel,st77_sm,true));
    dma_channel_configure(st77_dma,&dmc,&pio_channel->txf[st77_sm],NULL,0,false);
}
#endif
#endif

#ifdef st77_use_parallel
#ifdef st77_parallel_bb
/* Bit banging implemenation. This is not slower then the DMA version
 * so if the idea is to use the DMA in blocking mode, this could be
 * a better bet: keeps the state machine available for other uses.
 *
 * We need to take WR low and then high for at least 15
 * nanoseconds. At the default clock speed of the RP2040
 * this is more or less two clock cycles (two NOP instructions)
 * but if we got faster we will need to wait more.
 *
 * __asm volatile ("nop\n"); // Wait 1 clock cycle (~8 ns) normally.
 * __asm volatile ("nop\n"); // 2 clock cycles ~16ns.
 *
 *
 * NOP at 130Mhz: ~8ns (2 needed)
 *     at 250Mhz: 4ns  (4 needed)
 *     at 330Mhz: 3ns  (5 needed)
 *     at 400Mhz: 2.5ns (6 needed)
 *     at 500Mhz: 2ns  (8 needed)
 *
 * We also have the gpio_put() instruction (should take
 * two clock cycle). Even if I'm not sure in which cycle
 * the pin is finally set. In general to remove two NOPs
 * from the above table seems fine. */
void parallel_write_blocking(void *data, uint32_t datalen) {
    uint8_t *d = data;
    for (int j = 0; j < datalen; j++) {
        uint8_t byte = d[j];
        // WR clock low
        gpio_put(st77_wr,0);
        __asm volatile ("nop\n"); __asm volatile ("nop\n");
        __asm volatile ("nop\n");

        // Set byte to D0-D7.
        // Vanilla code would be:
        //
        // for (int i = 0; i < 8; i++)
        //     gpio_put(st77_d0+i,(byte>>i)&1);
        //
        // Bit it's much faster to set them all in oen pass:
        gpio_put_masked(0xff<<st77_d0,byte<<st77_d0);
        __asm volatile ("nop\n"); __asm volatile ("nop\n");
        __asm volatile ("nop\n");

        // WR clock high
        gpio_put(st77_wr,1);
        __asm volatile ("nop\n"); __asm volatile ("nop\n");
        __asm volatile ("nop\n");
    }
}
#else
// Send bytes to the display using the parallel 8 lines interface.
// PIO version with DMA.
//
// Note that the setup of writes is not very fast, while writing
// bulk data is very efficient (matches the maximum clock speed
// that the display can handle). So writing a lot of data in one
// call is the way to go: if you have enough RAM, take a framebuffer
// of the display in memory and use st77xx_update().
void parallel_write_blocking(void *data, uint32_t datalen) {
    // Wait for the DMA channel to be available.
    while (dma_channel_is_busy(st77_dma));
    // Set DMA source/length.
    dma_channel_set_trans_count(st77_dma,datalen,false);
    dma_channel_set_read_addr(st77_dma,data,true);
    // Wait end of DMA transfer.
    dma_channel_wait_for_finish_blocking(st77_dma);
    // Wait for the state machine to consume the FIFO.
    while(!pio_sm_is_tx_fifo_empty(pio_channel,st77_sm));

    // Avoid that successive calls to this function will make
    // the WR clock change state faster than the display
    // can handle: six NOPs are a compromise between not waiting
    // too much and max overclock speed that can be handled.
    __asm volatile ("nop\n"); __asm volatile ("nop\n");
    __asm volatile ("nop\n"); __asm volatile ("nop\n");
    __asm volatile ("nop\n"); __asm volatile ("nop\n");
}
#endif
#endif

/* Send command and/or data. */
void st77xx_write(uint8_t cmd, void *data, uint32_t datalen) {
    if (st77_cs != -1) gpio_put(st77_cs,0);
    if (cmd != 0) {
        gpio_put(st77_dc,0);
#ifdef st77_use_spi
        spi_write_blocking(spi_channel,&cmd,1);
#else
        parallel_write_blocking(&cmd,1);
#endif
    }
    if (data != NULL) {
        gpio_put(st77_dc,1);
#ifdef st77_use_spi
        spi_write_blocking(spi_channel,data,datalen);
#else
        parallel_write_blocking(data,datalen);
#endif
    }
    if (st77_cs != -1) gpio_put(st77_cs,1);
}

/* Command without arguments. */
void st77xx_cmd(uint8_t cmd) {
    st77xx_write(cmd,NULL,0);
}

/* Command + 1 byte data argument. */
void st77xx_cmd1(uint8_t cmd, uint8_t val) {
    st77xx_write(cmd,&val,1);
}

/* Write data. */
void st77xx_data(void *data, uint32_t datalen) {
    st77xx_write(0,data,datalen);
}

/* Display initialization. */
void st77xx_init(void) {
    #ifdef st77_use_spi
    st77xx_init_spi();
    #else
    st77xx_init_parallel();
    #endif

    // Display initialization
    if (st77_rst != -1) {
        gpio_put(st77_rst,1); sleep_ms(50);
        gpio_put(st77_rst,0); sleep_ms(50);
        gpio_put(st77_rst,1); sleep_ms(150);
    }
    st77xx_cmd(0x01); // Software reset
    sleep_ms(50);
    st77xx_cmd(0x11); // Awake from sleep
    sleep_ms(50);

    // Set color mode
    uint8_t colormode = 0x50 | 0x05; // 65k colors | RGB565
    st77xx_cmd1(0x3a,colormode & 0x77);
    sleep_ms(50);

    // Set memory access mode
    uint8_t madctl = 0;
    if (st77_landscape) madctl |= 0x20;
    if (st77_mirror_x) madctl |= 0x40;
    if (st77_mirror_y) madctl |= 0x80;
    st77xx_cmd1(0x36,madctl);
    st77xx_cmd(st77_inversion ? 0x21 : 0x20); // Inversion
    sleep_ms(10);

    st77xx_cmd(0x13); // Normal mode on
    sleep_ms(10);

    // At startup the display RAM is full of random pixel colors,
    // it's not nice to see. Fill the screen with black pixels
    // before showing the content to the user.
    st77xx_fill(0x0);

    st77xx_cmd(0x29); // Display on
    sleep_ms(500);

    // Power on the backlight
    if (st77_bl != -1) {
        gpio_init(st77_bl);
        gpio_set_dir(st77_bl,GPIO_OUT);
        gpio_put(st77_bl,1);
    }
}

void st77xx_setwin(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    x1 += st77_offset_x;
    y1 += st77_offset_y;
    x2 += st77_offset_x;
    y2 += st77_offset_y;
    uint8_t se[4];
    se[0] = (x1 >> 8) & 0xff;
    se[1] = x1 & 0xff;
    se[2] = (x2 >> 8) & 0xff;
    se[3] = x2 & 0xff;
    st77xx_write(0x2a,se,4);

    se[0] = (y1 >> 8) & 0xff;
    se[1] = y1 & 0xff;
    se[2] = (y2 >> 8) & 0xff;
    se[3] = y2 & 0xff;
    st77xx_write(0x2b,se,4);
    st77xx_cmd(0x2c); // Enter receive buffer data mode.
}

uint16_t st77xx_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t rgb = (r & 0xf8) << 8 | (g & 0xfc) << 3 | b >> 3;
    return (rgb >> 8) | ((rgb & 0xff) << 8);
}

void st77xx_pixel(uint16_t x, uint16_t y, uint16_t c) {
    st77xx_setwin(x,y,x,y);
    st77xx_data(&c,2);
}

void st77xx_pixel_rgb(uint16_t x, uint16_t y, uint32_t rgb) {
    if (x >= st77_width || y >= st77_height) return;
    uint16_t rgb565 = st77xx_rgb565(rgb&0xff,(rgb>>8)&0xff,(rgb>>16)&0xff);
    st77xx_pixel(x,y,rgb565);
}

void st77xx_fill_box(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t c) {
    unsigned int buflen = 256;
    uint16_t buf[buflen];
    uint32_t left = w*h;

    // Crop to visible display area.
    uint32_t x2 = x+w-1;
    uint32_t y2 = y+h-1;
    if (x2 >= st77_width) {
        if (x >= st77_width) return;
        x2 = st77_width-1;
    }
    if (y2 >= st77_height) {
        if (y >= st77_height) return;
        y2 = st77_height-1;
    }

    // Prefill buffer.
    if (left < buflen) buflen = left;
    for (int j = 0; j < buflen; j++) buf[j] = c;

    // Transfer buffer-length data at time until we can.
    st77xx_setwin(x,y,x+w-1,y+h-1);
    while(left >= buflen) {
        st77xx_data(buf,buflen*2);
        left -= buflen;
    }

    // Handle the reminder with a single write.
    if (left > 0) st77xx_data(buf,left*2);
}

void st77xx_fill(uint16_t c) {
    st77xx_fill_box(0,0,st77_width,st77_height,c);
}

void st77xx_update(uint16_t *fb) {
    st77xx_setwin(0,0,st77_width-1,st77_height-1);
    st77xx_data(fb,st77_width*st77_height*2);
}
