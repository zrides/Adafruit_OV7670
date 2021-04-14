// INITIALLY THIS IS COPIED FROM THE SAMD51 CODE AND WON'T ACTUALLY
// COMPILE ON RP2040. A WORK IN PROGRESS.

// This is the RP2040-specific parts of OV7670 camera interfacing (device-
// agnostic parts are in ov7670.c). It configures and accesses hardware-
// specific peripherals (PWM, DMA, PIO, etc.).

#if defined(PICO_SDK_VERSION_MAJOR)
#include "ov7670.h"

// PIO code in this table is modified at runtime so that PCLK and VSYNC
// pins are configurable (rather than fixed GP##s). Data pins must be
// contiguous.
static uint16_t ov7670_pio_opcodes[] = {
  0b0010000010000000, // WAIT 1 GPIO 0 (mask in VSYNC pin before use)
  0b0010000010000000, // WAIT 1 GPIO 0 (mask in PCLK pin before use)
  0b0100000000001000, // IN PINS 8
  0b0010000000000000, // WAIT 0 GPIO 0 (mask in PCLK pin before use)
};

struct pio_program ov7670_pio_program = {
  .instructions = ov7670_pio_opcodes,
  .length = sizeof ov7670_pio_opcodes / sizeof ov7670_pio_opcodes[0],
  .origin = -1,
};

// Each supported architecture MUST provide this function with this name,
// arguments and return type. It receives a pointer to a structure with
// at least a list of pins, and usually additional device-specific data
// attached to the 'arch' element.
OV7670_status OV7670_arch_begin(OV7670_host *host) {

  // LOOK UP PWM SLICE & CHANNEL BASED ON XCLK PIN -------------------------

  uint8_t slice = pwm_gpio_to_slice_num(host->pins->xclk);
  uint8_t channel = pwm_gpio_to_channel(host->pins->xclk);

  // CONFIGURE PWM FOR XCLK OUT --------------------------------------------
  // XCLK to camera is required for it to communicate over I2C!

  // Currently assumes 125 MHz CPU clk, output is around 25 MHz
  pwm_config config = pwm_get_default_config();
  pwm_config_set_clkdiv(&config, 125000000.0 / (OV7670_XCLK_HZ * 2));
  pwm_config_set_wrap(&config, 1); // 2 clocks/cycle
  pwm_init(slice, &config, true);
  pwm_set_chan_level(slice, channel, 1); // 50% duty cycle
  gpio_set_function(host->pins->xclk, GPIO_FUNC_PWM);

  // SET UP PIO ------------------------------------------------------------

  // TO DO: verify the DATA pins are contiguous.

  // Initially by default this always uses pio0, but I suppose this could be
  // written to use whatever pio has resources.
  host->arch->pio = pio0;

  ov_7670_pio_opcodes[0] |= host->pins->vsync; // Modify GPIO # for VSYNC
  ov_7670_pio_opcodes[1] |= host->pins->pclk;  // Modify GPIO # for PCLK
  // Opcode 2 is unmodified
  ov_7670_pio_opcodes[3] |= host->pins->pclk;  // Modify GPIO # for PCLK

  // Here's where resource check & switch to pio1 might go
  uint offset = pio_add_program(host->arch->pio, &ov7670_pio_program);
  host->arch->sm = pio_claim_unused_sm(host->arch->pio, true); // 0-3

  // host->pins->data[0] is data bit 0. PIO code requires all 8 data be
  // contiguous.
  pio_sm_set_consecutive_pindirs(host->arch->pio, host->arch->sm,
                                 host->pins->data[0], 8, false);

  pio_sm_config c = pio_get_default_sm_config(offset);
  sm_config_set_wrap(&c, offset, offset + ov7670_pio_program.length - 1);

  sm_config_set_in_pins(&c, host->pins->data[0]);
  sm_config_set_in_shift(&c, false, true, 8); // 8 data bits
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

  pio_sm_init(host->pio, host->sm, offset, &c);
  pio_sm_set_enabled(host->pio, host->sm, true);

  // This can improve GPIO responsiveness but is less noise-immune.
  uint32_t mask = (0xFF << host->pins->data[0]) | (1 << host->pins->pclk);
  pio->input_sync_bypass = mask;

  host->arch->pclk_mask = 1UL << host->pins->pclk;
  host->arch->vsync_mask = 1UL << host->pins->vsync;
  host->arch->hsync_mask = 1UL << host->pins->hsync;

  return OV7670_STATUS_OK;
}

// Non-DMA capture function using previously-initialized PCC peripheral.
void OV7670_capture(uint16_t *dest, uint16_t width, uint16_t height,
                    volatile uint32_t *vsync_reg, uint32_t vsync_bit,
                    volatile uint32_t *hsync_reg, uint32_t hsync_bit) {

  while (*vsync_reg & vsync_bit)
    ; // Wait for VSYNC low (frame end)
  OV7670_disable_interrupts();
  while (!*vsync_reg & vsync_bit)
    ; // Wait for VSYNC high (frame start)

  width /= 2;                             // PCC receives 2 pixels at a time
  for (uint16_t y = 0; y < height; y++) { // For each row...
    while (*hsync_reg & hsync_bit)
      ; //  Wait for HSYNC low (row end)
    while (!*hsync_reg & hsync_bit)
      ;                               //  Wait for HSYNC high (row start)
    for (int x = 0; x < width; x++) { //   For each column pair...
      while (!PCC->ISR.bit.DRDY)
        ;                     //    Wait for PCC data ready
      *dest++ = PCC->RHR.reg; //    Store 2 pixels
    }
  }

  OV7670_enable_interrupts();
}

// DEVICE-SPECIFIC FUNCTIONS FOR NON-ARDUINO PLATFORMS ---------------------

// If a platform doesn't offer a device-agnostic function for certain
// operations (i.e. can't be trivially remapped in ov7670.h as is done for
// Arduino), those device-specific functions can be declared here, with a
// platform-specific #ifdef around them. These functions may include:
// OV7670_delay_ms(x)
// OV7670_pin_output(pin)
// OV7670_pin_write(pin, hi)
// OV7670_disable_interrupts()
// OV7670_enable_interrupts()
// Also see notes at top regarding pin MUXing in this file.

#endif // end PICO_SDK_VERSION_MAJOR
