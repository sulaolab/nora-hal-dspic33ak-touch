/*
 * Minimal key integration example for nora-hal-dspic33ak-touch.
 *
 * Three self-capacitance electrodes, polled from the main loop, turning into
 * press and release events. This is the whole of the ordinary use of this HAL;
 * everything else in the header is diagnostics and tuning.
 *
 * What the board must do before app_touch_init():
 *
 *   1. Enable the clock generator that feeds the ITC (CLKGEN6 on dsPIC33AK) and
 *      know its frequency. It is passed in, not detected, and it has no default
 *      -- nora_touch_init() refuses 0. Acquisition times are held in
 *      nanoseconds (2,000 ns charge, 1,000 ns balance) and converted to timer
 *      counts against this number, so a wrong frequency reports no error at
 *      all: the pads simply read weak.
 *   2. Leave the electrode pins as analog inputs. The ITC drives and senses them
 *      itself, so they need no GPIO or PPS configuration -- but a pin left
 *      digital, or claimed by another peripheral, will read as a dead key.
 *   3. Nothing else. There is no interrupt to hook and no timer to provide:
 *      scanning is non-blocking and this layer counts scans, not milliseconds.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "nora_touch.h"

/* Supplied by the application: what a key actually does. */
extern void app_on_key_pressed( uint8_t key );
extern void app_on_key_released( uint8_t key );

/* CVDANx analog-input numbers, which are a fact about the board's wiring and
 * therefore the caller's to supply. These three are the Curiosity Platform's
 * touch pads; on your board they come from the schematic. */
static const uint8_t s_touch_cvdan[] = { 1u, 8u, 10u };
#define APP_TOUCH_KEY_COUNT   ( sizeof( s_touch_cvdan ) / sizeof( s_touch_cvdan[0] ) )

/* The frequency of the clock feeding the ITC. Replace with your board's. */
#define APP_TOUCH_CLOCK_HZ    ( 200000000UL )

static bool s_touch_ready;

bool app_touch_init( void )
{
    nora_touch_config_t cfg;

    /* Take the library's bench-derived defaults, then change only what is a
     * fact about this board. Filling the struct by hand instead would silently
     * inherit whatever the compiler left in the fields added by a later
     * version. */
    nora_touch_default_config( &cfg );
    cfg.clock_hz = APP_TOUCH_CLOCK_HZ;

    /* Per-pad threshold learning is on by default (cfg.learn_presses): each pad
     * walks its own threshold down towards its own measured press as it is
     * used, because idle noise was measured NOT to predict a press. It stops at
     * max(700, idle_ref * 6), so a pad cannot learn a threshold its own idle
     * noise would trip; nora_touch_set_key_thresholds() is the way past that.
     * Set learn_presses to 0 to keep the shipped pair fixed for good. Nothing is
     * stored across a power cycle either way, so boot behaviour never depends on
     * the last session. */

    s_touch_ready = nora_touch_init( s_touch_cvdan, (uint8_t)APP_TOUCH_KEY_COUNT, &cfg );
    return s_touch_ready;
}

/* Call as often as the main loop comes round; it is cheap when the scan in
 * flight has not finished (~5 ms at the default accumulation depth) and it never
 * busy-waits. */
void app_touch_task( void )
{
    uint8_t key;

    if( !s_touch_ready )
    {
        return;
    }

    nora_touch_process();

    for( key = 0u; key < (uint8_t)APP_TOUCH_KEY_COUNT; key++ )
    {
        /* One event per occurrence, cleared by reading. Read it once per pass
         * and act on it; reading it twice loses the second look. */
        switch( nora_touch_get_event( key ) )
        {
        case NORA_TOUCH_EVENT_PRESSED:
            app_on_key_pressed( key );
            break;

        case NORA_TOUCH_EVENT_RELEASED:
            app_on_key_released( key );
            break;

        case NORA_TOUCH_EVENT_NONE:
        default:
            break;
        }
    }
}

/* nora_touch_is_pressed() answers the other question -- held state rather than a
 * transition -- and does not consume anything, so it is safe to ask repeatedly
 * (a key held down while something else is decided, for instance). */
bool app_touch_any_held( void )
{
    uint8_t key;

    for( key = 0u; key < (uint8_t)APP_TOUCH_KEY_COUNT; key++ )
    {
        if( nora_touch_is_pressed( key ) )
        {
            return true;
        }
    }

    return false;
}

/* Health, for a status command or a startup check. Two of these numbers are the
 * ones worth watching: if rejected_scans or implausible_samples climbs while
 * nobody is touching the board, the acquisition is wrong and no amount of
 * threshold tuning will help. */
void app_touch_report( void )
{
    nora_touch_status_t status;

    nora_touch_get_status( &status );

    printf( "touch: %u key(s), %lu scans, %lu rejected, %lu implausible\r\n",
            (unsigned)status.key_count,
            (unsigned long)status.scans,
            (unsigned long)status.rejected_scans,
            (unsigned long)status.implausible_samples );
}
