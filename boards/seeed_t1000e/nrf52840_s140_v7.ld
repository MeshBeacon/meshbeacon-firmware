/* Linker script for nRF52840 + SoftDevice S140 v7.x
 * App start: 0x27000  (S140 v7 occupies 0x0000–0x26FFF, one page more than v6)
 * Adapted from nrf52840_s140_v6.ld shipped with Adafruit nRF52 BSP.
 *
 * Identical to boards/seeed_wio_tracker_l1/nrf52840_s140_v7.ld -- same
 * SoftDevice version (s140 7.3.0) and flash/RAM layout convention, just
 * duplicated here so board_build.variants_dir=boards resolves it under
 * this board's own variant directory.
 */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  FLASH (rx)     : ORIGIN = 0x27000, LENGTH = 0xED000 - 0x27000

  /* SRAM required by Softdevice depend on
   * - Attribute Table Size (Number of Services and Characteristics)
   * - Vendor UUID count
   * - Max ATT MTU
   * - Concurrent connection peripheral + central + secure links
   * - Event Len, HVN queue, Write CMD queue
   */
  RAM (rwx) :  ORIGIN = 0x20006000, LENGTH = 0x20040000 - 0x20006000
}

SECTIONS
{
  . = ALIGN(4);
  .svc_data :
  {
    PROVIDE(__start_svc_data = .);
    KEEP(*(.svc_data))
    PROVIDE(__stop_svc_data = .);
  } > RAM

  .fs_data :
  {
    PROVIDE(__start_fs_data = .);
    KEEP(*(.fs_data))
    PROVIDE(__stop_fs_data = .);
  } > RAM
} INSERT AFTER .data;

INCLUDE "nrf52_common.ld"
