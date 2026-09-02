/*
 * Spatial-only link stubs for Metal temporal entry points.
 *
 * Why this file exists
 * --------------------
 * Published `lib/mac_arm/libmagic_sr.a` (and mac_x86) references these
 * symbols from `MC_Process` / `MC_Uninit` but does **not** contain
 * `magic_sr_metal_temporal.o`. iOS `lib/ios/libmagic_sr.a` defines them.
 * `nm -u` on the mac_arm archive lists exactly:
 *   metal_temporal_set_device
 *   metal_temporal_init_output
 *   metal_temporal_is_inited
 *   metal_temporal_matches_size
 *   metal_temporal_process
 *   metal_temporal_encode
 *   metal_temporal_free
 *
 * Why not dead_strip / extract spatial objects
 * --------------------------------------------
 * The undefined refs live in the same `magic_process.o` that exports
 * `MC_Init` / `MC_Process` / `MC_Uninit`. Linking the Session API always
 * pulls that object, so `-dead_strip` cannot drop the temporal calls.
 * Repacking the core archive is forbidden for this demo.
 *
 * Official pattern
 * ----------------
 * Product tests generate the same filename from
 * `project/tools/mac_api_test_common.sh` (`metal_temporal_link_stubs.c`)
 * with matching signatures from `src/metal/magic_sr_metal_temporal.h`.
 * Those test stubs return 0 (success) so tests can inject results.
 * This demo is spatial-only: every temporal entry fails deterministically.
 * Test-only helpers (`metal_temporal_test_*`) are not undefined in the
 * published archive and are not provided here (would be unused).
 *
 * This sample never calls TEMPORAL_* modes. Do not treat a linked binary
 * as having real temporal Metal capability — `nm` of the final binary
 * should show these T (stub) symbols, not the iOS archive implementations.
 */
#include "mc_interface.h"

void metal_temporal_set_device(void *metal_handle, void *metal_device)
{
    (void)metal_handle;
    (void)metal_device;
}

int metal_temporal_init_output(void *metal_handle, int width, int height,
                               int out_width, int out_height)
{
    (void)metal_handle;
    (void)width;
    (void)height;
    (void)out_width;
    (void)out_height;
    return MC_ERROR_INIT_BACKEND_UNAVAILABLE;
}

int metal_temporal_is_inited(void *metal_handle)
{
    (void)metal_handle;
    return 0;
}

int metal_temporal_matches_size(void *metal_handle, int width, int height,
                                int out_width, int out_height)
{
    (void)metal_handle;
    (void)width;
    (void)height;
    (void)out_width;
    (void)out_height;
    return 0;
}

int metal_temporal_process(void *metal_handle, magic_frame_t *frame)
{
    (void)metal_handle;
    (void)frame;
    return MC_ERROR_INIT_BACKEND_UNAVAILABLE;
}

int metal_temporal_encode(void *metal_handle, magic_frame_t *frame, void *command_buffer)
{
    (void)metal_handle;
    (void)frame;
    (void)command_buffer;
    return MC_ERROR_TEMPORAL_COMMAND_BUFFER;
}

void metal_temporal_free(void *metal_handle)
{
    (void)metal_handle;
}
