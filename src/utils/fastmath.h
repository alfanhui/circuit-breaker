#ifndef __FASTMATH_H__
#define __FASTMATH_H__

/*
 * Arithmetic the SH-2 does slowly, done the quick way (the user, 2026-11-19: "Lets work on slow
 * routines"). jo's tools/slow_calls.py on game.elf found the city's per-frame code calling
 * libgcc's slow routines: the SH-2 has no barrel shifter and no divide instruction (jo's
 * .claude/skills/jo-engine/references/performance.md has the costs).
 *
 * Pure C: the PC's tests build it too (there, plain C does the same).
 */

/*
 * x >> n, arithmetic (rounded towards minus infinity, as GCC's >> on a signed int), for a
 * constant n. The SH-2 shifts a signed value right only a bit at a time (shar), so GCC calls
 * ___ashiftrt_r4_N for any n but 1, 2 or 16: N one-bit shifts, and the call (~20 cycles for
 * >> 10). Its logical shifts go by 1, 2, 8 and 16 bits (shlr, shlr2, shlr8, shlr16), so: x plus
 * 2^31 is never negative as an unsigned (x is -2^31 .. 2^31 - 1), its logical shift is
 * (x >> n) + 2^(31 - n), and that's taken off again. Exact for every x. Six instructions for
 * >> 10, no call (checked in the SH-2 compiler's output, 2026-11-19: it doesn't turn it back).
 * Use it only where x can be negative; a value surely 0 or more: (unsigned) it, one shift less
 */
#define ASR(x, n) ((int)(((unsigned int)(x) + 0x80000000u) >> (n)) - (int)(0x80000000u >> (n)))

/*
 * Bit `bit` (0 .. 7) of a byte, without a shift by a variable count (a call to ___ashrsi3 on the
 * SH-2): a table of the eight masks
 */
extern const unsigned char fastmath_bit[8];
#define BIT_SET(byte, bit) (((byte) & fastmath_bit[(bit)]) != 0)

/*
 * dividend / divisor, both 0 .. 2^31 - 1, divisor not 0 (the unit divides a signed 64-bit
 * dividend): on the Saturn by the SH-2's division unit (39 cycles, no call: SH7604 manual, DIVU;
 * jo's jo_uint_div does the same), elsewhere plain C.
 * For a division that has to stay in a per-frame path (CLAUDE.md): GCC's __sdivsi3 is a loop of
 * tens of cycles and a call. Not from an interrupt: the unit's registers aren't saved
 */
#if defined(__sh__)
static inline unsigned int fast_udiv(unsigned int dividend, unsigned int divisor)
{
	*(volatile unsigned int *)0xFFFFFF00 = divisor;	 // DVSR
	*(volatile unsigned int *)0xFFFFFF10 = 0;		 // DVDNTH: the high half of 64 bits
	*(volatile unsigned int *)0xFFFFFF14 = dividend; // DVDNTL: writing it starts the division
	return *(volatile unsigned int *)0xFFFFFF14;	 // the quotient (the read waits for it)
}
#else
static inline unsigned int fast_udiv(unsigned int dividend, unsigned int divisor)
{
	return dividend / divisor;
}
#endif

#endif
