/* vita-elf-create appends ~4 KB of module info after the text segment and
 * fails with "segment 1 overlaps" when the data segment starts too close.
 * Skipping 8 KB before the data segment is aligned guarantees the room,
 * whatever the code size. */
SECTIONS
{
  . = . + 0x2000;
}
INSERT AFTER .eh_frame;
