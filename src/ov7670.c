// SPDX-FileCopyrightText: 2020 P Burgess for Adafruit Industries
//
// SPDX-License-Identifier: MIT

#include "ov7670.h"

// REQUIRED EXTERN FUNCTIONS -----------------------------------------------

// Outside code MUST provide these functions with the same arguments and
// return types (see end of Adafruit_OV7670.cpp for an example). These allow
// basic debug-print and I2C operations without referencing C++ objects or
// accessing specific peripherals from this code. That information is all
// contained in the higher-level calling code, which can set up the platform
// pointer for whatever objects and/or peripherals it needs. Yes this is
// ugly and roundabout, but it makes this part of the code more reusable
// (not having to re-implement for each platform/architecture) and the
// functions in question aren't performance-oriented anyway (typically used
// just once on startup, or during I2C transfers which are slow anyway).

extern void OV7670_print(char *str);
extern int OV7670_read_register(void *platform, uint8_t reg);
extern void OV7670_write_register(void *platform, uint8_t reg, uint8_t value);

// UTILITY FUNCTIONS -------------------------------------------------------

// Write a dual-0xFF-terminated list of commands to the camera.
// First argument is a pointer to platform-specific data...e.g. on Arduno,
// this points to a C++ object so we can find our way back to the correct
// I2C peripheral (so this code doesn't have to deal with platform-specific
// I2C calls).
void OV7670_write_list(void *platform, const OV7670_command *cmd) {
  for (int i = 0; cmd[i].reg != 0xFF && cmd[i].value != 0xFF; i++) {
#if 0 // DEBUG
    char buf[50];
    sprintf(buf, "Write reg %02X = %02X\n", cmd[i].reg, cmd[i].value);
    OV7670_print(buf);
#endif
    OV7670_write_register(platform, cmd[i].reg, cmd[i].value);
    OV7670_delay_ms(1); // Required, else lockup on init
    // NOTE: delay might only be needed on SAMD51
  }
}

// Write a list of commands of specified length to the camera.
// Similar to be above function, but based on a length rather than
// a 0xFF/0xFF end-of-list (register 0xFF is a thing on OV2640 so
// that's not a good approach going forward).
void OV7670_write_list_len(void *platform, const OV7670_command *cmd, int len) {
  for (int i = 0; i < len; i++) {
    OV7670_write_register(platform, cmd[i].reg, cmd[i].value);
    OV7670_delay_ms(1); // Required, else lockup on init
    // NOTE: delay might only be needed on SAMD51
  }
}

// CAMERA STARTUP ----------------------------------------------------------

static const OV7670_command
    OV7670_rgb[] =
        {
            // Manual output format, RGB, use RGB565 and full 0-255 output range
            {OV7670_REG_COM7, OV7670_COM7_RGB},
            {OV7670_REG_RGB444, 0},
            {OV7670_REG_COM15, OV7670_COM15_RGB565 | OV7670_COM15_R00FF},
            {0xFF, 0xFF}},
    OV7670_yuv[] =
        {
            // Manual output format, YUV, use full output range
            {OV7670_REG_COM7, OV7670_COM7_YUV},
            {OV7670_REG_COM15, OV7670_COM15_R00FF},
            {0xFF, 0xFF}},
    OV7670_init[] =
        {
            // OV7670 camera initialization after reset
            {OV7670_REG_TSLB, OV7670_TSLB_YLAST},    // No auto window
            {OV7670_REG_COM10, OV7670_COM10_VS_NEG}, // -VSYNC (req by SAMD PCC)
            {OV7670_REG_SLOP, 0x20},
            {OV7670_REG_GAM_BASE, 0x1C},
            {OV7670_REG_GAM_BASE + 1, 0x28},
            {OV7670_REG_GAM_BASE + 2, 0x3C},
            {OV7670_REG_GAM_BASE + 3, 0x55},
            {OV7670_REG_GAM_BASE + 4, 0x68},
            {OV7670_REG_GAM_BASE + 5, 0x76},
            {OV7670_REG_GAM_BASE + 6, 0x80},
            {OV7670_REG_GAM_BASE + 7, 0x88},
            {OV7670_REG_GAM_BASE + 8, 0x8F},
            {OV7670_REG_GAM_BASE + 9, 0x96},
            {OV7670_REG_GAM_BASE + 10, 0xA3},
            {OV7670_REG_GAM_BASE + 11, 0xAF},
            {OV7670_REG_GAM_BASE + 12, 0xC4},
            {OV7670_REG_GAM_BASE + 13, 0xD7},
            {OV7670_REG_GAM_BASE + 14, 0xE8},
            {OV7670_REG_COM8,
             OV7670_COM8_FASTAEC | OV7670_COM8_AECSTEP | OV7670_COM8_BANDING},
            {OV7670_REG_GAIN, 0x00},
            {OV7670_COM2_SSLEEP, 0x00},
            {OV7670_REG_COM4, 0x00},
            {OV7670_REG_COM9, 0x20}, // Max AGC value
            {OV7670_REG_BD50MAX, 0x05},
            {OV7670_REG_BD60MAX, 0x07},
            {OV7670_REG_AEW, 0x75},
            {OV7670_REG_AEB, 0x63},
            {OV7670_REG_VPT, 0xA5},
            {OV7670_REG_HAECC1, 0x78},
            {OV7670_REG_HAECC2, 0x68},
            {0xA1, 0x03},              // Reserved register?
            {OV7670_REG_HAECC3, 0xDF}, // Histogram-based AEC/AGC setup
            {OV7670_REG_HAECC4, 0xDF},
            {OV7670_REG_HAECC5, 0xF0},
            {OV7670_REG_HAECC6, 0x90},
            {OV7670_REG_HAECC7, 0x94},
            {OV7670_REG_COM8, OV7670_COM8_FASTAEC | OV7670_COM8_AECSTEP |
                                  OV7670_COM8_BANDING | OV7670_COM8_AGC |
                                  OV7670_COM8_AEC},
            {OV7670_REG_COM5, 0x61},
            {OV7670_REG_COM6, 0x4B},
            {0x16, 0x02},            // Reserved register?
            {OV7670_REG_MVFP, 0x07}, // 0x07,
            {OV7670_REG_ADCCTR1, 0x02},
            {OV7670_REG_ADCCTR2, 0x91},
            {0x29, 0x07}, // Reserved register?
            {OV7670_REG_CHLF, 0x0B},
            {0x35, 0x0B}, // Reserved register?
            {OV7670_REG_ADC, 0x1D},
            {OV7670_REG_ACOM, 0x71},
            {OV7670_REG_OFON, 0x2A},
            {OV7670_REG_COM12, 0x78},
            {0x4D, 0x40}, // Reserved register?
            {0x4E, 0x20}, // Reserved register?
            {OV7670_REG_GFIX, 0x5D},
            {OV7670_REG_REG74, 0x19},
            {0x8D, 0x4F}, // Reserved register?
            {0x8E, 0x00}, // Reserved register?
            {0x8F, 0x00}, // Reserved register?
            {0x90, 0x00}, // Reserved register?
            {0x91, 0x00}, // Reserved register?
            {OV7670_REG_DM_LNL, 0x00},
            {0x96, 0x00}, // Reserved register?
            {0x9A, 0x80}, // Reserved register?
            {0xB0, 0x84}, // Reserved register?
            {OV7670_REG_ABLC1, 0x0C},
            {0xB2, 0x0E}, // Reserved register?
            {OV7670_REG_THL_ST, 0x82},
            {0xB8, 0x0A}, // Reserved register?
            {OV7670_REG_AWBC1, 0x14},
            {OV7670_REG_AWBC2, 0xF0},
            {OV7670_REG_AWBC3, 0x34},
            {OV7670_REG_AWBC4, 0x58},
            {OV7670_REG_AWBC5, 0x28},
            {OV7670_REG_AWBC6, 0x3A},
            {0x59, 0x88}, // Reserved register?
            {0x5A, 0x88}, // Reserved register?
            {0x5B, 0x44}, // Reserved register?
            {0x5C, 0x67}, // Reserved register?
            {0x5D, 0x49}, // Reserved register?
            {0x5E, 0x0E}, // Reserved register?
            {OV7670_REG_LCC3, 0x04},
            {OV7670_REG_LCC4, 0x20},
            {OV7670_REG_LCC5, 0x05},
            {OV7670_REG_LCC6, 0x04},
            {OV7670_REG_LCC7, 0x08},
            {OV7670_REG_AWBCTR3, 0x0A},
            {OV7670_REG_AWBCTR2, 0x55},
            {OV7670_REG_MTX1, 0x80},
            {OV7670_REG_MTX2, 0x80},
            {OV7670_REG_MTX3, 0x00},
            {OV7670_REG_MTX4, 0x22},
            {OV7670_REG_MTX5, 0x5E},
            {OV7670_REG_MTX6, 0x80}, // 0x40?
            {OV7670_REG_AWBCTR1, 0x11},
            {OV7670_REG_AWBCTR0, 0x9F}, // Or use 0x9E for advance AWB
            {OV7670_REG_BRIGHT, 0x00},
            {OV7670_REG_CONTRAS, 0x40},
            {OV7670_REG_CONTRAS_CENTER, 0x80}, // 0x40?
            {0xFF, 0xFF}},                     // End-of-data marker
    ov2640_vga[] =
        {
            // OV2640 camera initialization after reset
            // WIP STUFF, don't take this seriously yet
            // Also, this will be put in separate file later
            {OV2640_REG_RA_DLMT, OV2640_RA_DLMT_DSP},    // DSP bank select 0
            {0x2C, 0xFF},                                // Reserved
            {0x2E, 0xDF},                                // Reserved
            {OV2640_REG_RA_DLMT, OV2640_RA_DLMT_SENSOR}, // Sensor bank sel 1
            {0x3C, 0x32},                                // Reserved
            {OV2640_REG1_CLKRC, 0x00},                   // Clock doubler OFF
            {OV2640_REG1_COM2, OV2640_COM2_DRIVE_2X},    // 2X drive select
            {OV2640_REG1_REG04,                          // Mirror + ?
             OV2640_REG04_HFLIP | 0x20 | OV2640_REG04_HREF0},
            {OV2640_REG1_COM8, 0xC0 | OV2640_COM8_BANDING |
                                   OV2640_COM8_AGC_AUTO | OV2640_COM8_EXP_AUTO},
            {OV2640_REG1_COM9, OV2640_COM9_AGC_GAIN_8X | 0x08},
            {0x2C, 0x0c},            // Reserved
            {0x33, 0x78},            // Reserved
            {0x3A, 0x33},            // Reserved
            {0x3B, 0xfB},            // Reserved
            {0x3E, 0x00},            // Reserved
            {0x43, 0x11},            // Reserved
            {0x16, 0x10},            // Reserved
            {0x4A, 0x81},            // Reserved
            {0x21, 0x99},            // Reserved
            {OV2640_REG1_AEW, 0x40}, // High range for AEC/AGC
            {OV2640_REG1_AEB, 0x38}, // Low range for AEC/AGC
            {OV2640_REG1_VV, 0x82},  // Fast mode thresholds
            {0x5C, 0x00},            // Reserved
            {0x63, 0x00},            // Reserved
            {OV2640_REG1_FLL, 0x3F}, // Frame length adjustment LSBs
            {OV2640_REG1_COM3, 0x38 | OV2640_COM3_BANDING_50HZ},
            {OV2640_REG1_HISTO_LOW, 0x70},
            {OV2640_REG1_HISTO_HIGH, 0x80},
            {0x7C, 0x05}, // Reserved
            {0x20, 0x80}, // Reserved
            {0x28, 0x30}, // Reserved
            {0x6C, 0x00}, // Reserved
            {0x6D, 0x80}, // Reserved
            {0x6E, 0x00}, // Reserved
            {0x70, 0x02}, // Reserved
            {0x71, 0x94}, // Reserved
            {0x73, 0xC1}, // Reserved
            {0x3D, 0x34}, // Reserved
            {0x5A, 0x57}, // Reserved
            {OV2640_REG1_COM7, OV2640_COM7_RES_UXGA},
            {OV2640_REG1_CLKRC, 0x00},   // Clock doubler off
            {OV2640_REG1_HREFST, 0x11},  // Horiz window start MSB
            {OV2640_REG1_HREFEND, 0x75}, // Horiz window end MSB
            {OV2640_REG1_VSTRT, 0x01},   // Vert window start MSB
            {OV2640_REG1_VEND, 0x97},    // Vert window end MSB
            {OV2640_REG1_REG32, 0x36},   // Horiz window LSBs
            {OV2640_REG1_COM1, 0x0F},    // Vert window LSBs
            {0x37, 0x40},                // Reserved
            {OV2640_REG1_BD50, 0xBB},    // 50 Hz banding AEC MSBs
            {OV2640_REG1_BD60, 0x9C},    // 60 Hz banding AEC MSBs
            {0x5A, 0x57},                // Reserved
            {0x6D, 0x80},                // Reserved
            {0x6D, 0x38},                // Reserved (2nd ref in a row?)
            {0x39, 0x02},                // Reserved
            {0x35, 0x88},                // Reserved
            {0x22, 0x0A},                // Reserved
            {0x37, 0x40},                // Reserved
            {0x23, 0x00},                // Reserved
            {OV2640_REG1_ARCOM2, 0xA0},  // ?
            {0x36, 0x1A},                // Reserved
            {0x06, 0x02},                // Reserved
            {0x07, 0xC0},                // Reserved
            {OV2640_REG1_COM4, 0xB7},
            {0x0E, 0x01},                             // Reserved
            {0x4C, 0x00},                             // Reserved
            {OV2640_REG_RA_DLMT, OV2640_RA_DLMT_DSP}, // DSP bank select 0
            {0xE5, 0x7F},                             // Reserved
            {OV2640_REG0_MC_BIST,
             OV2640_MC_BIST_RESET | OV2640_MC_BIST_BOOTROM},
            {0x41, 0x24}, // Reserved
            {OV2640_REG0_RESET, OV2640_RESET_JPEG | OV2640_RESET_DVP},
            {0x76, 0xFF}, // Reserved
            {0x33, 0xA0}, // Reserved
            {0x42, 0x20}, // Reserved
            {0x43, 0x18}, // Reserved
            {0x4C, 0x00}, // Reserved
            {OV2640_REG0_CTRL3, OV2640_CTRL3_BPC | OV2640_CTRL3_WPC | 0x10},
            {0x88, 0x3F}, // Reserved
            {0xD7, 0x03}, // Reserved
            {0xD9, 0x10}, // Reserved
            {OV2640_REG0_R_DVP_SP, OV2640_R_DVP_SP_AUTO | 0x02},
            {0xC8, 0x08}, // Reserved
            {0xC9, 0x80}, // Reserved
            {OV2640_REG0_BPDATA, 0x00},
            {OV2640_REG0_BPADDR, 0x03},
            {OV2640_REG0_BPDATA, 0x48},
            {OV2640_REG0_BPADDR, 0x08},
            {OV2640_REG0_BPDATA, 0x20},
            {OV2640_REG0_BPDATA, 0x10},
            {OV2640_REG0_BPDATA, 0x0E},
            {0x90, 0x00},                             // Reserved (addr/data?)
            {0x91, 0x0E},                             // Reserved
            {0x91, 0x1A},                             // Reserved
            {0x91, 0x31},                             // Reserved
            {0x91, 0x5A},                             // Reserved
            {0x91, 0x69},                             // Reserved
            {0x91, 0x75},                             // Reserved
            {0x91, 0x7E},                             // Reserved
            {0x91, 0x88},                             // Reserved
            {0x91, 0x8F},                             // Reserved
            {0x91, 0x96},                             // Reserved
            {0x91, 0xA3},                             // Reserved
            {0x91, 0xaf},                             // Reserved
            {0x91, 0xc4},                             // Reserved
            {0x91, 0xd7},                             // Reserved
            {0x91, 0xe8},                             // Reserved
            {0x91, 0x20},                             // Reserved
            {0x92, 0x00},                             // Reserved (addr/data?)
            {0x93, 0x06},                             // Reserved
            {0x93, 0xe3},                             // Reserved
            {0x93, 0x02},                             // Reserved
            {0x93, 0x02},                             // Reserved
            {0x93, 0x00},                             // Reserved
            {0x93, 0x04},                             // Reserved
            {0x93, 0x00},                             // Reserved
            {0x93, 0x03},                             // Reserved
            {0x93, 0x00},                             // Reserved
            {0x93, 0x00},                             // Reserved
            {0x93, 0x00},                             // Reserved
            {0x93, 0x00},                             // Reserved
            {0x93, 0x00},                             // Reserved
            {0x93, 0x00},                             // Reserved
            {0x93, 0x00},                             // Reserved (end data?)
            {0x96, 0x00},                             // Reserved (addr/data?)
            {0x97, 0x08},                             // Reserved
            {0x97, 0x19},                             // Reserved
            {0x97, 0x02},                             // Reserved
            {0x97, 0x0c},                             // Reserved
            {0x97, 0x24},                             // Reserved
            {0x97, 0x30},                             // Reserved
            {0x97, 0x28},                             // Reserved
            {0x97, 0x26},                             // Reserved
            {0x97, 0x02},                             // Reserved
            {0x97, 0x98},                             // Reserved
            {0x97, 0x80},                             // Reserved
            {0x97, 0x00},                             // Reserved
            {0x97, 0x00},                             // Reserved
            {OV2640_REG0_CTRL1, ~OV2640_CTRL1_DG},    // Enable all but DG
            {OV2640_REG_RA_DLMT, OV2640_RA_DLMT_DSP}, // DSP bank select 0
            {0xBA, 0xDC},                             // Reserved
            {0xBB, 0x08},                             // Reserved
            {0xB6, 0x24},                             // Reserved
            {0xB8, 0x33},                             // Reserved
            {0xB7, 0x20},                             // Reserved
            {0xB9, 0x30},                             // Reserved
            {0xB3, 0xB4},                             // Reserved
            {0xB4, 0xCA},                             // Reserved
            {0xB5, 0x43},                             // Reserved
            {0xB0, 0x5C},                             // Reserved
            {0xB1, 0x4F},                             // Reserved
            {0xB2, 0x06},                             // Reserved
            {0xC7, 0x00},                             // Reserved
            {0xC6, 0x51},                             // Reserved
            {0xC5, 0x11},                             // Reserved
            {0xC4, 0x9C},                             // Reserved
            {0xBF, 0x00},                             // Reserved
            {0xBC, 0x64},                             // Reserved
            {0xA6, 0x00},                             // Reserved (addr/data?)
            {0xA7, 0x1E},                             // Reserved
            {0xA7, 0x6b},                             // Reserved
            {0xA7, 0x47},                             // Reserved
            {0xA7, 0x33},                             // Reserved
            {0xA7, 0x00},                             // Reserved
            {0xA7, 0x23},                             // Reserved
            {0xA7, 0x2E},                             // Reserved
            {0xA7, 0x85},                             // Reserved
            {0xA7, 0x42},                             // Reserved
            {0xA7, 0x33},                             // Reserved
            {0xA7, 0x00},                             // Reserved
            {0xA7, 0x23},                             // Reserved
            {0xA7, 0x1B},                             // Reserved
            {0xA7, 0x74},                             // Reserved
            {0xA7, 0x42},                             // Reserved
            {0xA7, 0x33},                             // Reserved
            {0xA7, 0x00},                             // Reserved
            {0xA7, 0x23},                             // Reserved
            {OV2640_REG0_HSIZE8, 0xC8},               // Horiz size MSBs
            {OV2640_REG0_VSIZE8, 0x96},               // Vert size MSBs
            {OV2640_REG0_SIZEL, 0x00},                // Size bits
            {OV2640_REG0_CTRL2, OV2640_CTRL2_DCW | OV2640_CTRL2_SDE |
                                    OV2640_CTRL2_UV_ADJ | OV2640_CTRL2_UV_AVG |
                                    OV2640_CTRL2_CMX},
            {OV2640_REG0_CTRLI, OV2640_CTRLI_LP_DP | 0x82}, // H/V dividers
            {OV2640_REG0_HSIZE, 0x90},                      // H_SIZE low bits
            {OV2640_REG0_VSIZE, 0x2C},                      // V_SIZE low bits
            {OV2640_REG0_XOFFL, 0x00},                      // OFFSET_X LSBs
            {OV2640_REG0_YOFFL, 0x00},                      // OFFSET_Y LSBs
            {OV2640_REG0_VHYX, 0x88},                       // V/H/Y/X MSBs
            {OV2640_REG0_ZMOW, 0x50},                       // OUTW low bits
            {OV2640_REG0_ZMOH, 0x3C},                       // OUTH low bits
            {OV2640_REG0_ZMHH, 0x00},                       // OUTW/H high bits
            {OV2640_REG0_R_DVP_SP, 0x04},                   // Manual DVP PCLK
            {0x7F, 0x00},                                   // Reserved
            {OV2640_REG0_IMAGE_MODE, 0x00},                 // YUV MSB first
            {0xE5, 0x1F},                                   // Reserved
            {0xE1, 0x67},                                   // Reserved
            {OV2640_REG0_RESET, 0x00},                      // Reset nothing?
            {0xDD, 0x7F},                                   // Reserved
            {OV2640_REG0_R_BYPASS, OV2640_R_BYPASS_DSP_ENABLE},
            {OV2640_REG_RA_DLMT, OV2640_RA_DLMT_DSP}, // DSP bank select 0
            {OV2640_REG0_RESET, OV2640_RESET_DVP},
            {OV2640_REG0_HSIZE8, 0xC8}, // Image horiz size MSBs
            {OV2640_REG0_VSIZE8, 0x96}, // Image vert size MSBs
            {OV2640_REG0_CTRL2, OV2640_CTRL2_DCW | OV2640_CTRL2_SDE |
                                    OV2640_CTRL2_UV_ADJ | OV2640_CTRL2_UV_AVG |
                                    OV2640_CTRL2_CMX},
            {OV2640_REG0_CTRLI, OV2640_CTRLI_LP_DP | 0x12},
            {OV2640_REG0_HSIZE, 0x90}, // H_SIZE low bits
            {OV2640_REG0_VSIZE, 0x2C}, // V_SIZE low bits
            {OV2640_REG0_XOFFL, 0x00}, // OFFSET_X low bits
            {OV2640_REG0_YOFFL, 0x00}, // OFFSET_y low bits
            {OV2640_REG0_VHYX, 0x88},  // V/H/Y/X high bits
            {OV2640_REG0_TEST, 0x00},
            {OV2640_REG0_ZMOW, 0x50},                 // OUTW low bits
            {OV2640_REG0_ZMOH, 0x3C},                 // OUTH low bits
            {OV2640_REG0_ZMHH, 0x00},                 // OUTW/H high bits
            {OV2640_REG0_R_DVP_SP, 0x04},             // Manual DVP PCLK
            {0xE0, 0x00},                             // Reset nothing?
            {OV2640_REG_RA_DLMT, OV2640_RA_DLMT_DSP}, // DSP bank select 0
            {OV2640_REG0_R_BYPASS, OV2640_R_BYPASS_DSP_ENABLE},
            {OV2640_REG0_IMAGE_MODE, OV2640_IMAGE_MODE_DVP_RGB565},
            {OV2640_REG0_IMAGE_MODE,
             OV2640_IMAGE_MODE_DVP_RGB565 | OV2640_IMAGE_MODE_BYTE_SWAP},
            {0x98, 0x00},                             // Reserved
            {0x99, 0x00},                             // Reserved
            {0x00, 0x00},                             // Reserved
            {OV2640_REG_RA_DLMT, OV2640_RA_DLMT_DSP}, // DSP bank select 0
            {OV2640_REG0_RESET, OV2640_RESET_DVP},
            {OV2640_REG0_HSIZE8, 0xC8}, // H_SIZE high bits
            {OV2640_REG0_VSIZE8, 0x96}, // V_SIZE high bits
            {OV2640_REG0_CTRL2, OV2640_CTRL2_DCW | OV2640_CTRL2_SDE |
                                    OV2640_CTRL2_UV_ADJ | OV2640_CTRL2_UV_AVG |
                                    OV2640_CTRL2_CMX},
            {OV2640_REG0_CTRLI, OV2640_CTRLI_LP_DP | 0x09},
            {OV2640_REG0_HSIZE, 0x90}, // H_SIZE low bits
            {OV2640_REG0_VSIZE, 0x2C}, // V_SIZE low bits
            {OV2640_REG0_XOFFL, 0x00}, // OFFSET_X low bits
            {OV2640_REG0_YOFFL, 0x00}, // OFFSET_Y low bits
            {OV2640_REG0_VHYX, 0x88},  // V/H/Y/X high bits
            {OV2640_REG0_TEST, 0x00},
            {OV2640_REG0_ZMOW, 0xA0},     // OUTW low bits
            {OV2640_REG0_ZMOH, 0x78},     // OUTH low bits
            {OV2640_REG0_ZMHH, 0x00},     // OUTW/H high bits
            {OV2640_REG0_R_DVP_SP, 0x02}, // Manual DVP PCLK setting
            {OV2640_REG0_RESET, 0x00}},   // Reset nothing?
    ov2640_qqvga[] = {
        // Configure OV2640 for QQVGA output
        {OV2640_REG_RA_DLMT, OV2640_RA_DLMT_DSP}, // DSP bank select 0
        {OV2640_REG0_RESET, OV2640_RESET_DVP},
        {OV2640_REG0_HSIZE8, 0x64}, // HSIZE high bits
        {OV2640_REG0_VSIZE8, 0x4B}, // VSIZE high bits
        {OV2640_REG0_CTRL2, OV2640_CTRL2_DCW | OV2640_CTRL2_SDE |
                                OV2640_CTRL2_UV_AVG | OV2640_CTRL2_CMX},
        {OV2640_REG0_CTRLI, OV2640_REG0_TEST | 0x12},
        {OV2640_REG0_HSIZE, 0xC8},    // H_SIZE low bits
        {OV2640_REG0_VSIZE, 0x96},    // V_SIZE low bits
        {OV2640_REG0_XOFFL, 0x00},    // OFFSET_X low bits
        {OV2640_REG0_YOFFL, 0x00},    // OFFSET_Y low bits
        {OV2640_REG0_VHYX, 0x00},     // V/H/Y/X high bits
        {OV2640_REG0_TEST, 0x00},     // ?
        {OV2640_REG0_ZMOW, 0x28},     // OUTW low bits
        {OV2640_REG0_ZMOH, 0x1E},     // OUTH low bits
        {OV2640_REG0_ZMHH, 0x00},     // OUTW/H high bits
        {OV2640_REG0_R_DVP_SP, 0x08}, // Manual DVP PCLK setting
        {OV2640_REG0_RESET, 0x00}};   // Reset nothing?

OV7670_status OV7670_begin(OV7670_host *host, OV7670_colorspace colorspace,
                           OV7670_size size, float fps) {
  OV7670_status status;

  // I2C must already be set up and running (@ 100 KHz) in calling code

  // Do device-specific (but platform-agnostic) setup. e.g. on SAMD this
  // function will fiddle registers to start a timer for XCLK output and
  // enable the parallel capture peripheral.
  status = OV7670_arch_begin(host);
  if (status != OV7670_STATUS_OK) {
    return status;
  }

  // Unsure of camera startup time from beginning of input clock.
  // Let's guess it's similar to tS:REG (300 ms) from datasheet.
  OV7670_delay_ms(300);

  // ENABLE AND/OR RESET CAMERA --------------------------------------------

  if (host->pins->enable >= 0) { // Enable pin defined?
    OV7670_pin_output(host->pins->enable)
        OV7670_pin_write(host->pins->enable, 0); // PWDN low (enable)
    OV7670_delay_ms(300);
  }

  if (host->pins->reset >= 0) { // Hard reset pin defined?
    OV7670_pin_output(host->pins->reset);
    OV7670_pin_write(host->pins->reset, 0);
    OV7670_delay_ms(1);
    OV7670_pin_write(host->pins->reset, 1);
  } else { // Soft reset, doesn't seem reliable, might just need more delay?
    OV7670_write_register(host->platform, OV7670_REG_COM7, OV7670_COM7_RESET);
  }
  OV7670_delay_ms(1); // Datasheet: tS:RESET = 1 ms

#if 1
  (void)OV7670_set_fps(host->platform, fps); // Timing
  if (colorspace == OV7670_COLOR_RGB) {
    OV7670_write_list(host->platform, OV7670_rgb);
  } else {
    OV7670_write_list(host->platform, OV7670_yuv);
  }
  OV7670_write_list(host->platform, OV7670_init); // Other config
  OV7670_set_size(host->platform, size);          // Frame size
#else
  // Hacked OV2640 setup (WIP)
  OV7670_write_list_len(host->platform, ov2640_vga,
                        sizeof ov2640_vga / sizeof ov2640_vga[0]);
  OV7670_write_list_len(host->platform, ov2640_qqvga,
                        sizeof ov2640_qqvga / sizeof ov2640_qqvga[0]);
  // Set RGB565 output mode
  OV7670_write_register(host->platform, OV2640_REG_RA_DLMT, OV2640_RA_DLMT_DSP);
  OV7670_delay_ms(1); // Required, else lockup on init
  OV7670_write_register(host->platform, 0xDA,
                        (OV7670_read_register(host->platform, 0xDA) & 0xC) |
                            0x8);
  OV7670_delay_ms(1); // Required, else lockup on init
#endif

  OV7670_delay_ms(300); // tS:REG = 300 ms (settling time = 10 frames)

  return OV7670_STATUS_OK;
}

// MISCELLANY AND CAMERA CONFIG FUNCTIONS ----------------------------------

// Configure camera frame rate. Actual resulting frame rate (returned) may
// be different depending on available clock frequencies. Result will only
// exceed input if necessary for minimum supported rate, but this is very
// rare, typically below 1 fps. In all other cases, result will be equal
// or less than the requested rate, up to a maximum of 30 fps (the "or less"
// is because requested fps may be based on other host hardware timing
// constraints (e.g. screen) and rounding up to a closer-but-higher frame
// rate would be problematic). There is no hardcoded set of fixed frame
// rates because it varies with architecture, depending on OV7670_XCLK_HZ.
// If platform is NULL, no registers are set, a fps request/return can be
// evaluated without reconfiguring the camera, or without it even started.

float OV7670_set_fps(void *platform, float fps) {

  // Pixel clock (PCLK), which determines overall frame rate, is a
  // function of XCLK input frequency (OV7670_XCLK_HZ), a PLL multiplier
  // and then an integer division factor (1-32). These are the available
  // OV7670 PLL ratios:
  static const uint8_t pll_ratio[] = {1, 4, 6, 8};
  const uint8_t num_plls = sizeof pll_ratio / sizeof pll_ratio[0];

  // Constrain frame rate to upper and lower limits
  fps = (fps > 30) ? 30 : fps;               // Max 30 FPS
  float pclk_target = fps * 4000000.0 / 5.0; // Ideal PCLK Hz for target FPS
  uint32_t pclk_min = OV7670_XCLK_HZ / 32;   // Min PCLK determines min FPS
  if (pclk_target < (float)pclk_min) {       // If PCLK target is below limit
    if (platform) {
      OV7670_write_register(platform, OV7670_REG_DBLV, 0);   // 1:1 PLL
      OV7670_write_register(platform, OV7670_REG_CLKRC, 31); // 1/32 div
    }
    return (float)(pclk_min * 5 / 4000000); // Return min frame rate
  }

  // Find nearest available FPS without going over. This is done in a
  // brute-force manner, testing all 127 PLL-up by divide-down permutations
  // and tracking the best fit. I’m certain there are shortcuts but was
  // having trouble with my math, might revisit later. It's not a huge
  // bottleneck...MCUs are fast now, many cases are quickly discarded, and
  // this operation is usually done only once on startup (the I2C transfers
  // probably take longer).

  uint8_t best_pll = 0;    // Index (not value) of best PLL match
  uint8_t best_div = 1;    // Value of best division factor match
  float best_delta = 30.0; // Best requested vs actual FPS (init to "way off")

  for (uint8_t p = 0; p < num_plls; p++) {
    uint32_t xclk_pll = OV7670_XCLK_HZ * pll_ratio[p]; // PLL'd freq
    uint8_t first_div = p ? 2 : 1; // Min div is 1 for PLL 1:1, else 2
    for (uint8_t div = first_div; div <= 32; div++) {
      uint32_t pclk_result = xclk_pll / div; // PCLK-up-down permutation
      if (pclk_result > pclk_target) {       // Exceeds target?
        continue;                            //  Skip it
      }
      float fps_result = (float)pclk_result * 5.0 / 4000000.0;
      float delta = fps - fps_result; // How far off?
      if (delta < best_delta) {       // Best match yet?
        best_delta = delta;           //  Save delta,
        best_pll = p;                 //  pll and
        best_div = div;               //  div for later use
      }
    }
  }

  if (platform) {
    // Set up DBLV and CLKRC registers with best PLL and div values
    if (pll_ratio[best_pll] == best_div) { // If PLL and div are same (1:1)
      // Bypass PLL, use external clock directly
      OV7670_write_register(platform, OV7670_REG_DBLV, 0);
      OV7670_write_register(platform, OV7670_REG_CLKRC, 0x40);
    } else {
      // Set DBLV[7:6] for PLL, CLKRC[5:0] for div-1 (1-32 stored as 0-31)
      OV7670_write_register(platform, OV7670_REG_DBLV, best_pll << 6);
      OV7670_write_register(platform, OV7670_REG_CLKRC, best_div - 1);
    }
  }

  return fps - best_delta; // Return actual frame rate
}

// Sets up PCLK dividers and sets H/V start/stop window. Rather than
// rolling this into OV7670_set_size(), it's kept separate so test code
// can experiment with different settings to find ideal defaults.
void OV7670_frame_control(void *platform, uint8_t size, uint8_t vstart,
                          uint16_t hstart, uint8_t edge_offset,
                          uint8_t pclk_delay) {
  uint8_t value;

  // Enable downsampling if sub-VGA, and zoom if 1:16 scale
  value = (size > OV7670_SIZE_DIV1) ? OV7670_COM3_DCWEN : 0;
  if (size == OV7670_SIZE_DIV16)
    value |= OV7670_COM3_SCALEEN;
  OV7670_write_register(platform, OV7670_REG_COM3, value);

  // Enable PCLK division if sub-VGA 2,4,8,16 = 0x19,1A,1B,1C
  value = (size > OV7670_SIZE_DIV1) ? (0x18 + size) : 0;
  OV7670_write_register(platform, OV7670_REG_COM14, value);

  // Horiz/vert downsample ratio, 1:8 max (H,V are always equal for now)
  value = (size <= OV7670_SIZE_DIV8) ? size : OV7670_SIZE_DIV8;
  OV7670_write_register(platform, OV7670_REG_SCALING_DCWCTR, value * 0x11);

  // Pixel clock divider if sub-VGA
  value = (size > OV7670_SIZE_DIV1) ? (0xF0 + size) : 0x08;
  OV7670_write_register(platform, OV7670_REG_SCALING_PCLK_DIV, value);

  // Apply 0.5 digital zoom at 1:16 size (others are downsample only)
  value = (size == OV7670_SIZE_DIV16) ? 0x40 : 0x20; // 0.5, 1.0
  // Read current SCALING_XSC and SCALING_YSC register values because
  // test pattern settings are also stored in those registers and we
  // don't want to corrupt anything there.
  uint8_t xsc = OV7670_read_register(platform, OV7670_REG_SCALING_XSC);
  uint8_t ysc = OV7670_read_register(platform, OV7670_REG_SCALING_YSC);
  xsc = (xsc & 0x80) | value; // Modify only scaling bits (not test pattern)
  ysc = (ysc & 0x80) | value;
  // Write modified result back to SCALING_XSC and SCALING_YSC
  OV7670_write_register(platform, OV7670_REG_SCALING_XSC, xsc);
  OV7670_write_register(platform, OV7670_REG_SCALING_YSC, ysc);

  // Window size is scattered across multiple registers.
  // Horiz/vert stops can be automatically calc'd from starts.
  uint16_t vstop = vstart + 480;
  uint16_t hstop = (hstart + 640) % 784;
  OV7670_write_register(platform, OV7670_REG_HSTART, hstart >> 3);
  OV7670_write_register(platform, OV7670_REG_HSTOP, hstop >> 3);
  OV7670_write_register(platform, OV7670_REG_HREF,
                        (edge_offset << 6) | ((hstop & 0b111) << 3) |
                            (hstart & 0b111));
  OV7670_write_register(platform, OV7670_REG_VSTART, vstart >> 2);
  OV7670_write_register(platform, OV7670_REG_VSTOP, vstop >> 2);
  OV7670_write_register(platform, OV7670_REG_VREF,
                        ((vstop & 0b11) << 2) | (vstart & 0b11));

  OV7670_write_register(platform, OV7670_REG_SCALING_PCLK_DELAY, pclk_delay);
}

void OV7670_set_size(void *platform, OV7670_size size) {
  // Array of five window settings, index of each (0-4) aligns with the five
  // OV7670_size enumeration values. If enum changes, list must change!
  static struct {
    uint8_t vstart;
    uint8_t hstart;
    uint8_t edge_offset;
    uint8_t pclk_delay;
  } window[] = {
      // Window settings were tediously determined empirically.
      // I hope there's a formula for this, if a do-over is needed.
      {9, 162, 2, 2},  // SIZE_DIV1  640x480 VGA
      {10, 174, 4, 2}, // SIZE_DIV2  320x240 QVGA
      {11, 186, 2, 2}, // SIZE_DIV4  160x120 QQVGA
      {12, 210, 0, 2}, // SIZE_DIV8  80x60   ...
      {15, 252, 3, 2}, // SIZE_DIV16 40x30
  };

  OV7670_frame_control(platform, size, window[size].vstart, window[size].hstart,
                       window[size].edge_offset, window[size].pclk_delay);
}

// Select one of the camera's night modes (or disable).
// Trades off frame rate for less grainy images in low light.
// Note: seems that frame rate is somewhat automatic despite
//       the requested setting, i.e. if 1:8 is selected, might
//       still get normal frame rate or something higher than
//       1:8, if the scene lighting permits. Also the setting
//       seems to 'stick' in some cases when trying to turn
//       this off. Might want to try always having night mode
//       enabled but using 1:1 frame setting as 'off'.
void OV7670_night(void *platform, OV7670_night_mode night) {
  // Table of bit patterns for the different supported night modes.
  // There's a "same frame rate" option for OV7670 night mode but it
  // doesn't seem to do anything useful and can be skipped over.
  static const uint8_t night_bits[] = {0b00000000, 0b10100000, 0b11000000,
                                       0b11100000};
  // Read current COM11 register setting so unrelated bits aren't corrupted
  uint8_t com11 = OV7670_read_register(platform, OV7670_REG_COM11);
  com11 &= 0b00011111;        // Clear night mode bits
  com11 |= night_bits[night]; // Set night bits for desired mode
  // Write modified result back to COM11 register
  OV7670_write_register(platform, OV7670_REG_COM11, com11);
}

// Flips camera output on horizontal and/or vertical axes.
// Note: datasheet refers to horizontal flip as "mirroring," but
// avoiding that terminology here that it might be mistaken for a
// split-down-middle-and-reflect funhouse effect, which it isn't.
// Also note: mirrored image isn't always centered quite the same,
// looks like frame control settings might need to be tweaked
// depending on flips. Similar issue to color bars?
void OV7670_flip(void *platform, bool flip_x, bool flip_y) {
  // Read current MVFP register setting, so we don't corrupt any
  // reserved bits or the "black sun" bit if it was previously set.
  uint8_t mvfp = OV7670_read_register(platform, OV7670_REG_MVFP);
  if (flip_x) {
    mvfp |= OV7670_MVFP_MIRROR; // Horizontal flip
  } else {
    mvfp &= ~OV7670_MVFP_MIRROR;
  }
  if (flip_y) {
    mvfp |= OV7670_MVFP_VFLIP; // Vertical flip
  } else {
    mvfp &= ~OV7670_MVFP_VFLIP;
  }
  // Write modified result back to MVFP register
  OV7670_write_register(platform, OV7670_REG_MVFP, mvfp);
}

// Selects one of the camera's test patterns (or disable).
// See Adafruit_OV7670.h for notes about minor visual bug here.
void OV7670_test_pattern(void *platform, OV7670_pattern pattern) {
  // Read current SCALING_XSC and SCALING_YSC register settings,
  // so image scaling settings aren't corrupted.
  uint8_t xsc = OV7670_read_register(platform, OV7670_REG_SCALING_XSC);
  uint8_t ysc = OV7670_read_register(platform, OV7670_REG_SCALING_YSC);
  if (pattern & 1) {
    xsc |= 0x80;
  } else {
    xsc &= ~0x80;
  }
  if (pattern & 2) {
    ysc |= 0x80;
  } else {
    ysc &= ~0x80;
  }
  // Write modified results back to SCALING_XSC and SCALING_YSC registers
  OV7670_write_register(platform, OV7670_REG_SCALING_XSC, xsc);
  OV7670_write_register(platform, OV7670_REG_SCALING_YSC, ysc);
}

// Reformat YUV gray component to RGB565 for TFT preview.
// Big-endian in and out.
void OV7670_Y2RGB565(uint16_t *ptr, uint32_t len) {
  while (len--) {
    uint8_t y = *ptr & 0xFF; // Y (brightness) component of YUV
    uint16_t rgb = ((y >> 3) * 0x801) | ((y & 0xFC) << 3); // to RGB565
    *ptr++ = __builtin_bswap16(rgb); // Big-endianify RGB565 for TFT
  }
}
