/* ---------------------------------------------------------------
 * Neptune Smart Reset Button for PIC16F630/76
 * with In-Game Reset (IGR)
 * 2026 Electroanalog(c) VICE
 *
 * Based on Saturn Smart Reset Button (SAT-SRB)
 * ---------------------------------------------------------------
 * This is a derivative work licensed under the GNU General Public 
 * License, as published by the Free Software Foundation; either 
 * version 2 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This version features:
 * - RGB LED feedback with customizable color by region
 * - Region switching: USA / Japan / Europe
 * - Video frequency control: 50 Hz / 60 Hz with LED blink feedback
 * - Reset button with short/mid/long press detection
 * - In-Game Reset (IGR) via controller combo with configurable 
 *   hold time and accidental trigger protection
 * - Replacement for standard switchless region and 50/60 Hz mods
 * ----------------------------------------------------------------
 * 
 * PIC16F630/76 Pinout:
 *  1 - VDD  = +5V
 *  2 - RA5  = IGR TR input (Controller port pin 9)
 *  3 - RA4  = IGR TL input (Controller port pin 6)
 *  4 - RA3  = IGR UP input (controller pin 1) | ICSP MCLR/VPP
 *  5 - RC5  = Green LED | A[+] sourcing
 *  6 - RC4  = Red LED | A[+] sourcing
 *  7 - RC3  = (unused)
 *  8 - RC2  = Blue LED | A[+] sourcing
 *  9 - RC1  = IGR TH input (Controller port pin 7)
 * 10 - RC0  = JAP (Region)
 * 11 - RA2  = WRES (Reset Out)
 * 12 - RA1  = NTSC (VF) | ICSP CLK
 * 13 - RA0  = RESET SW (BUTTON) | ICSP DAT
 * 14 - VSS  = GND
 * 
 * ---------------------------------------------------------------
 * HARDWARE NOTES
 * ---------------------------------------------------------------
 * - RGB LED type: Common-cathode
 *
 * - Region switches:
 *   JAP (JP1-2)   NTSC (JP3-4)
 *   -----------------------
 *   USA: HI(EXP)  HI(NTSC)
 *   JAP: LO(JAP)  HI(NTSC)
 *   EUR: HI(EXP)  LO(PAL)
 *
 * - Series resistors:
 *   RGB LEDs:                 ~1k
 *   IGR IN (RA3-RA4-RA5-RC1):  1k
 * 
 * The optional series resistors on RA3/RA4/RA5/RC1 improve robustness when
 * passively monitoring controller lines for IGR detection.
 * 
 */

#include <xc.h>
#include <stdint.h>

// BASIC CONFIG
#define _XTAL_FREQ 4000000      // Clock frequency
#pragma config MCLRE = OFF      // MCLR pin function is digital input
#pragma config BOREN = ON       // Brown-out Reset enabled
#pragma config PWRTE = ON       // Power-up Timer Enable bit
#pragma config WDTE = OFF       // Watchdog Timer Disable bit
#pragma config FOSC = INTRCIO   // Internal RC oscillator
#pragma config CP = OFF         // Code Protection disabled

// EEPROM DEFINITION
#define WRITE_EEPROM(addr, val)  eeprom_write((unsigned char)(addr), (unsigned char)(val))

__EEPROM_DATA( 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 );
/* EEPROM Layout:
 * Byte 0: Current region index (0 = USA, 1 = JAP, 2 = EUR)
 */

// FEATURE ENABLE
#define IGR_ENABLE    1      // 1 = Enable In-Game Reset, 0 = Disable

// PIN DEFINITIONS
#define BTTN    RA0     // Reset button input
#define NTSC    RA1     // PAL/NTSC control output (JP3-4)
#define WRES    RA2     // Reset signal output
#define RST     TRISA2  // TRIS bit for RA2
#define JAP     RC0     // JAP/EXP region control output (JP1-2)
#define LED_B   RC2     // Blue LED output (A+)
#define LED_R   RC4     // Red LED output (A+)
#define LED_G   RC5     // Green LED output (A+)
#define LED_OFF  do { LED_R = 0; LED_G = 0; LED_B = 0; } while(0)

#if IGR_ENABLE
// IGR INPUTS (Controller lines)
#define IGR_TH   RC1    // TH (controller pin 7)
#define IGR_UP   RA3    // UP (controller pin 1)
#define IGR_TL   RA4    // TL (controller pin 6)
#define IGR_TR   RA5    // TR (controller pin 9)
#define IGR_ACTIVE  ((IGR_TL == 0) && (IGR_TR == 0))
#define SMS_COMBO   (IGR_UP == 0)
#endif

// LED COLOR CODES (bit2=Red, bit1=Green, bit0=Blue)
#define LED_RED     0b100
#define LED_GREEN   0b010
#define LED_BLUE    0b001
#define LED_YELLOW  0b110  // R+G
#define LED_CYAN    0b011  // G+B
#define LED_PURPLE  0b101  // R+B
#define LED_WHITE   0b111  // R+G+B

// ** LED COLOR ASSIGNMENT **
#define COLOR_USA   LED_RED 
#define COLOR_JAP   LED_GREEN
#define COLOR_EUR   LED_YELLOW

// USER TIMING CONSTANTS
#define STD_PRESS   300     // ms | Short < STD_PRESS, Medium >= STD_PRESS
#define EXT_PRESS   1000    // ms | Long press threshold
#define IGR_HOLD    1000    // ms | IGR safe hold time (controller combo)
#define RST_PULSE   500     // ms | Reset pulse width

// SYSTEM TIMING CONSTANTS
#define DEBOUNCE_MS 50      // ms | Debounce time
#define POLL_MS     100     // ms | Button sampling interval

// TIMER0 CONFIG (1 ms tick @ 4 MHz, prescaler 1:32)
#define TMR0_PRELOAD  0xE1

// REGION IDENTIFIERS
#define REGION_USA   0  // NTSC_US
#define REGION_JAP   1  // NTSC_JP
#define REGION_EUR   2  // PAL_EU

// VF IDENTIFIERS
#define VF60      1 // 60Hz
#define VF50      0 // 50Hz

// GLOBAL STATE VARIABLES
uint8_t current_region = REGION_USA;   // Loaded from EEPROM, default USA
uint8_t current_vf     = VF60;         // Derived from region (EUR forces 50Hz, others 60Hz)
volatile uint16_t sys_ms;              // 1 ms system timebase (Timer0 tick)
volatile uint8_t led_busy = 0;
volatile uint8_t led_phase = 0;
volatile uint8_t led_remaining = 0;
volatile uint16_t led_timer = 0;
volatile uint16_t led_period = 0;
#if IGR_ENABLE
uint8_t igr_block = 0;                 // Prevents retrigger while combo is held
#endif

// BUTTON TIMING TRACKING
uint16_t press_start_ms;
uint16_t press_duration;
uint16_t cycle_ms = 0;                 // Counts 1-second intervals during long-press cycle preview

// LONG-PRESS CYCLING
uint8_t cycle_mode = 0;                // 0 = inactive, 1 = active (region preview in progress)
uint8_t cycle_region = REGION_USA;     // Region being previewed during long press cycle

// FUNCTION PROTOTYPES
void io_init(void);                    // Configure ports, pull-ups, and disable comparators
void _load(void);                      // Load region from EEPROM and apply default VF (no LED blink)
void _save(void);                      // Save region to EEPROM only if changed
void setLeds(void);                    // Set LED according to current_region
void led_set_color(uint8_t color);     // Write RGB bits to LED pins
void darkenLeds(uint16_t ms);          // Temporarily turn LED off, then restore region color
void led_blink_start(uint8_t count, uint16_t period_ms, uint8_t dunkel);
void display5060_start(uint8_t dunkel);
void display5060(uint8_t dunkel);      // LED blink feedback: slow=50Hz, fast=60Hz (only for VF toggle, IGR or region apply)
void delay(uint16_t ms);               // Wrapper around __delay_ms()
void reset(void);                      // Generate reset pulse on WRES (active-low), then return RA2 to Hi-Z
void apply_region(uint8_t region);     // Apply region lines + default VF + LED + optional VF blink
void apply_vf(uint8_t vf);             // Apply VF line (NTSC=60Hz / PAL=50Hz)
void toggle_vf(void);                  // EUR only: toggle 50/60Hz with LED feedback (no reset)
uint8_t next_region(uint8_t region);   // Returns next region in USA -> JAP -> EUR -> USA sequence
uint8_t eeprom_read(uint8_t addr);     // Read from EEPROM
void eeprom_write(uint8_t addr, uint8_t value);  // Write to EEPROM with proper unlock sequence
void timer0_init(void);                // Initialize Timer0 for 1 ms system timebase (sys_ms increment)
#if IGR_ENABLE
uint8_t igr_combo(void);               // Detect full IGR combo using continuous sampling (TL/TR + TH + hold time)
#endif

// -- ISR --
void __interrupt() isr(void)
{
    if (INTCONbits.T0IF)
    {
        INTCONbits.T0IF = 0;
        TMR0 = TMR0_PRELOAD;
        sys_ms++;
        // -------------------------------------------------
        // LED blink engine
        // -------------------------------------------------
        if (led_busy)
        {
            if (++led_timer >= led_period)
            {
                led_timer = 0;

                if (led_phase == 0)
                {
                    // ON -> OFF
                    LED_OFF;
                    led_phase = 1;
                }
                else
                {
                    // OFF -> ON
                    setLeds();
                    led_phase = 0;

                    if (--led_remaining == 0)
                    {
                        led_busy = 0;
                    }
                }
            }
        }
    }
}

/* MAIN:
 * - Short press (<STD_PRESS): Reset only (all regions).
 * - Medium press (>=STD_PRESS and <EXT_PRESS):
 *      USA/JAP -> reset
 *      EUR     -> toggle VF only (no reset)
 * - Long press (>=EXT_PRESS):
 *      Enter region-cycle preview (every 1s)
 *      On release: apply region, apply default VF, save EEPROM, reset
 */
void main(void)
{
    io_init();              // Initialize MCU I/O
    timer0_init();          // Initialize Timer0
    INTCONbits.GIE  = 1;    // Global interrupt enable
    _load();                // Load region from EEPROM and apply defaults
    apply_vf(current_vf);   // Ensure NTSC line matches loaded VF
    setLeds();              // Show region LED color on boot

    uint8_t bttn_last = 1; // Last sampled state
    uint8_t bttn_curr = 1; // Current sampled state

    while (1)
    {
        bttn_curr = BTTN;   // Read button (1 = released, 0 = pressed)

        // ------------------------------------------------------------
        // BUTTON PRESSED (ACTIVE LOW)
        // ------------------------------------------------------------
        if (bttn_curr == 0)
        {
            // Initial debounce on press
            delay(DEBOUNCE_MS);
            if (BTTN != 0)
            {
                bttn_last = bttn_curr;
                continue;               // False trigger
            }

            // First valid press ? start timing immediately
            press_start_ms = sys_ms;

            // --------------------------------------------------------
            // HOLDING THE BUTTON
            // --------------------------------------------------------
            while (BTTN == 0)
            {
                delay(POLL_MS);
                press_duration = sys_ms - press_start_ms;

                // ---------- ENTER LONG-PRESS REGION CYCLE ----------
                if (press_duration >= EXT_PRESS && cycle_mode == 0)
                {
                    cycle_mode  = 1;
                    cycle_region = current_region;
                    cycle_ms    = 0;
                }

                // ---------- LONG-PRESS PREVIEW MODE ----------
                if (cycle_mode)
                {
                    cycle_ms += POLL_MS;

                    // Advance preview every 1000ms
                    if (cycle_ms >= 1000)
                    {
                        cycle_ms = 0;
                        cycle_region = next_region(cycle_region);

                        // Show region preview color
                        led_set_color(
                            (cycle_region == REGION_USA) ? COLOR_USA :
                            (cycle_region == REGION_JAP) ? COLOR_JAP :
                                                           COLOR_EUR
                        );
                    }
                }
            }

            // Button is now released -> the RELEASE handler below will process the event.
            bttn_last = 0;              // Last state was "pressed"
            continue;
        }

        // ------------------------------------------------------------
        // BUTTON RELEASED -> HANDLE EVENT
        // ------------------------------------------------------------
        if (bttn_last == 0 && bttn_curr == 1)
        {
            // Release debounce
            delay(DEBOUNCE_MS);
            if (BTTN == 0)
            {
                // Still pressed ? ignore
                bttn_last = bttn_curr;
                continue;
            }

            // ---------------- LONG PRESS -----------------------
            if (press_duration >= EXT_PRESS)
            {
                // Apply region + default VF + feedback blink
                apply_region(cycle_region);

                // Save region to EEPROM
                _save();
                
                // VF feedback
                display5060_start(1);

                // Safety gap before reset (avoids stale lines)
                __delay_ms(50);

                // Perform system reset
                reset();
                while (led_busy)
                {
                }
            }

            // ---------------- MEDIUM PRESS ---------------------
            else if (press_duration >= STD_PRESS)
            {
                if (current_region == REGION_EUR)
                {
                    // EUR: toggle VF only (with VF blink), no reset
                    toggle_vf();
                }
                else
                {
                    // USA/JAP: medium press = reset
                    reset();
                }
            }

            // ---------------- SHORT PRESS ----------------------
            else
            {
                // Short press always = reset (all regions)
                reset();
            }

            // Clear tracking after event processing
            press_duration = 0;
            cycle_ms   = 0;
            cycle_mode = 0;

            bttn_last = bttn_curr;
            delay(POLL_MS);
            continue;
        }

        // ------------------------------------------------------------
        // NO EVENT -> IDLE LOOP
        // ------------------------------------------------------------
        bttn_last = bttn_curr;
        #if IGR_ENABLE
        // ---------------- IGR HANDLING ----------------
        if (BTTN) // Ignore IGR while physical reset button is pressed
        {
            if (!igr_block)
            {
                if (igr_combo())
                {
                    igr_block = 1;
                    display5060_start(1);
                    reset();

                    while (led_busy)
                    {
                    }
                }
            }
        }

        // Release IGR block only when combo is fully released
        if (!IGR_ACTIVE)
        {
            igr_block = 0;
        }
        #endif
        delay(POLL_MS);
    }
}

// Initialize I/O ports and MCU configuration
void io_init(void) {
    // Disable comparators
    CMCON = 0x07;
    // ----- PORTA configuration -----
    TRISA0 = 1;   // RA0 = Reset button input (weak pull-up enabled)
    TRISA1 = 0;   // RA1 = NTSC dipswitch (VF) output
    TRISA2 = 1;   // RA2 = WRES idle in Hi-Z (reset line is driven only during reset pulse)
    TRISA3 = 1;   // RA3 = UP input (controller pin 1)
    TRISA4 = 1;   // RA4 = TL input (controller pin 6)
    TRISA5 = 1;   // RA5 = TR input (controller pin 9)
    // Enable global weak pull-ups
    OPTION_REGbits.nGPPU = 0;
    // Enable weak pull-up
    WPUAbits.WPUA0 = 1; // RA0 (Reset button)
    // ----- PORTC configuration -----
    TRISC0 = 0;   // RC0 = JAP dipswitch output
    TRISC1 = 1;   // RC1 = TH input (controller pin 7)
    TRISC2 = 0;   // RC2 = Blue LED (A+ source)
    TRISC3 = 1;   // RC3 = Unused (input)
    TRISC4 = 0;   // RC4 = Red LED (A+ source)
    TRISC5 = 0;   // RC5 = Green LED (A+ source)
    // Ensure LEDs are OFF at startup
    LED_OFF;
    // Default safe state before EEPROM configuration is applied
    JAP  = 1;     // EXP mode (USA/EUR)
    NTSC = 1;     // 60Hz default
}

// Load settings from EEPROM (region only; VF is always derived)
// No LED blinking must occur during system startup.
void _load(void)
{
    // Read region index from EEPROM
    uint8_t r = eeprom_read(0x00);
    // Validate stored region. If invalid, default to USA.
    if (r > REGION_EUR)
        r = REGION_USA;
    current_region = r;
    // Derive default VF from region:
    // EUR = 50Hz, USA/JAP = 60Hz
    current_vf = (current_region == REGION_EUR) ? VF50 : VF60;
    // ----------------------------------------------------
    // Apply region lines immediately (NO LED BLINKING)
    // ----------------------------------------------------
    // JAP/EXP control line (JP1-JP2)
    if (current_region == REGION_JAP)
        JAP = 0;        // JAP region
    else
        JAP = 1;        // USA/EUR = EXP
    // NTSC/PAL control line (JP3-JP4)
    NTSC = (current_vf == VF60) ? 1 : 0;
    // Show region LED color on boot (no fade or blink)
    setLeds();
}

// Save settings to EEPROM if values have changed
// Only the region is persisted; VF is always derived dynamically.
void _save(void)
{
    uint8_t stored = eeprom_read(0x00);
    // Only write if region has changed (minimize EEPROM wear)
    if (stored != current_region)
    {
        WRITE_EEPROM(0x00, current_region);
    }
}

// Set LED color based on current region
void setLeds(void)
{
    switch (current_region)
    {
        case REGION_USA:
            led_set_color(COLOR_USA);
            break;
        case REGION_JAP:
            led_set_color(COLOR_JAP);
            break;
        case REGION_EUR:
            led_set_color(COLOR_EUR);
            break;
    }
}

// Set LED RGB channels according to 3-bit color code
void led_set_color(uint8_t color)
{
    LED_OFF;   // Turn off all channels before applying new color
    if (color & LED_RED)
        LED_R = 1;
    if (color & LED_GREEN)
        LED_G = 1;
    if (color & LED_BLUE)
        LED_B = 1;
}

// Darken LED for a fixed time, then restore region color
void darkenLeds(uint16_t ms)
{
    LED_OFF;        // Turn LED off
    delay(ms);      // Hold dark interval
    setLeds();      // Restore region color
}

void led_blink_start(uint8_t count, uint16_t period_ms, uint8_t dunkel)
{
    // Optional initial LED blanking
    if (!dunkel)
    {
        darkenLeds(200);
    }

    setLeds();

    led_busy = 1;
    led_timer = 0;
    led_phase = 0;
    led_remaining = count;
    led_period = period_ms;
}

void display5060_start(uint8_t dunkel)
{
    if (current_vf == VF50)
    {
        led_blink_start(2, 150, dunkel);
    }
    else
    {
        led_blink_start(4, 75, dunkel);
    }
}

// LED feedback for VF mode (slow = 50Hz, fast = 60Hz)
void display5060(uint8_t dunkel)
{
    display5060_start(dunkel);

    while (led_busy)
    {
    }
}

// Apply VF mode to NTSC output line (1 = 60Hz, 0 = 50Hz)
void apply_vf(uint8_t vf)
{
    current_vf = vf;
    if (vf == VF60)
        NTSC = 1;     // 60Hz
    else
        NTSC = 0;     // 50Hz
}

// Toggle VF (50/60 Hz). Valid only in EUR.
// USA/JAP: medium press = reset.
void toggle_vf(void)
{
    // Only EUR supports VF switching
    if (current_region != REGION_EUR) {
        reset();            // Medium press = reset for USA/JAP
        return;
    }
    // Toggle 50/60 Hz
    current_vf = (current_vf == VF50 ? VF60 : VF50);
    // Apply VF to output
    apply_vf(current_vf);
    // LED feedback for new VF
    display5060(0);
}

// Apply region outputs and default VF (no reset, no save)
void apply_region(uint8_t region)
{
    current_region = region;       // Update region state
    // JAP/EXP line
    if (current_region == REGION_JAP)
        JAP = 0;                   // JAP = low
    else
        JAP = 1;                   // USA / EUR = EXP = high
    // Default VF by region
    current_vf = (current_region == REGION_EUR ? VF50 : VF60);
    // Apply VF to NTSC line
    apply_vf(current_vf);
    // Region LED
    setLeds();
}

// Return next region in USA > JAP > EUR > USA cycle
uint8_t next_region(uint8_t region)
{
    switch (region) {
        case REGION_USA: return REGION_JAP;
        case REGION_JAP: return REGION_EUR;
        default:         return REGION_USA;
    }
}

// Simple millisecond delay wrapper
void delay(uint16_t ms)
{
    while (ms--) {
        __delay_ms(1);
    }
}

// Generate RESET pulse on RA2 (WRES). Active-low, then return to Hi-Z.
void reset(void)
{
    RST = 0;            // RA2 as output
    WRES = 0;           // Active low for reset
    delay(RST_PULSE);   // Hold low time
    WRES = 1;           // Release reset
    RST = 1;            // Return RA2 to Hi-Z
    // Restore LED state after reset pulse completes
    setLeds();
    // [disabled] Ensure VF line matches current region default after reset
    // [disabled] current_vf = (current_region == REGION_EUR ? VF50 : VF60);
    apply_vf(current_vf);
}

// Read one byte from EEPROM
uint8_t eeprom_read(uint8_t addr)
{
    while (WR);       // Wait for any write in progress
    EEADR = addr;     // Set EEPROM address
    RD = 1;           // Start read
    return EEDATA;    // Return read data
}

// Write one byte to EEPROM
void eeprom_write(uint8_t addr, uint8_t value)
{
    while (WR);        // Wait for any previous write
    EEADR  = addr;     // Set EEPROM address
    EEDATA = value;    // Set data to write
    WREN   = 1;        // Enable write
    EECON2 = 0x55;
    EECON2 = 0xAA;
    WR = 1;            // Start write
    while (WR);        // Wait until write completes
    WREN = 0;          // Disable write
}

void timer0_init(void)
{
    OPTION_REGbits.T0CS = 0;   // Timer0 clock = Fosc/4
    OPTION_REGbits.PSA  = 0;   // Prescaler assigned to Timer0
    OPTION_REGbits.PS   = 0b100; // 1:32 prescaler

    TMR0 = TMR0_PRELOAD;

    INTCONbits.T0IF = 0;
    INTCONbits.T0IE = 1;       // Enable Timer0 interrupt
}

#if IGR_ENABLE
/* Detect full IGR combo using continuous sampling
 * - Requires TL/TR active during entire window
 * - Detects TH behavior:
 *      - Multiplexed (Genesis): requires both phases for A+B+C+Start
 *      - Static TH (Master System): requires UP+B+C
 * - Samples TH every ~50 us to detect controller multiplexing
 * - Hold time is measured using the 1 ms system timer (IGR_HOLD)
 */
uint8_t igr_combo(void)
{
    uint16_t start_ms = sys_ms;
    uint8_t th_high = 0;
    uint8_t th_low = 0;
    uint8_t th_toggle = 0;
    uint8_t sms_valid = 1;

    while (IGR_ACTIVE)
    {
        if (IGR_TH)
            th_high = 1;
        else
            th_low = 1;
        // Detect TH toggling (Genesis controller)
        if (th_high && th_low)
            th_toggle = 1;
        if (!SMS_COMBO)
            sms_valid = 0;
        if ((uint16_t)(sys_ms - start_ms) >= IGR_HOLD)
        {
            // --- Genesis controller (TH multiplexed) ---
            if (th_toggle)
                return 1;      // Genesis: A+B+C+Start
            // --- Master System controller (TH static) ---
            if (sms_valid)
                return 1;      // Master: B+C+UP
            return 0;
        }
        __delay_us(50);
    }
    return 0;
}
// th_toggle = 1 -> multiplexed TH polling (Genesis mode)
// th_toggle = 0 -> static TH polling (Master System mode)
#endif
