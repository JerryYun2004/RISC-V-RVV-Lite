    .section .text
    .globl rgba2luma_vec
    .align 2

/* ============================================================
 * rgba2luma_vec(r=a0, g=a1, b=a2, out=a3, n=a4)
 *   r   : uint32_t R plane, low 8 bits used
 *   g   : uint32_t G plane, low 8 bits used
 *   b   : uint32_t B plane, low 8 bits used
 *   out : uint32_t luma output, low 8 bits used
 *   n   : number of pixels
 *
 * Planar RGB layout, using RVV-Lite A.1 style e32 unit-stride ops.
 * ============================================================ */

#define OPV      0x57
#define LOADFP   0x07
#define STOREFP  0x27

#define RVV_VSETVLI(rd, rs1, vtypei) \
  .word (((vtypei) << 20) | ((rs1) << 15) | (7 << 12) | ((rd) << 7) | OPV)

#define RVV_VLE32(vd, rs1) \
  .word ((1 << 25) | ((rs1) << 15) | (6 << 12) | ((vd) << 7) | LOADFP)

#define RVV_VSE32(vs3, rs1) \
  .word ((1 << 25) | ((rs1) << 15) | (6 << 12) | ((vs3) << 7) | STOREFP)

#define F3_OPIVV 0
#define F3_OPIVX 4

#define F6_VADD 0x00
#define F6_VMUL 0x25
#define F6_VAND 0x09

#define RVV_OPIVV(f6, vd, vs1, vs2) \
  .word (((f6) << 26) | (1 << 25) | ((vs2) << 20) | ((vs1) << 15) | (F3_OPIVV << 12) | ((vd) << 7) | OPV)

#define RVV_OPIVX(f6, vd, rs1, vs2) \
  .word (((f6) << 26) | (1 << 25) | ((vs2) << 20) | ((rs1) << 15) | (F3_OPIVX << 12) | ((vd) << 7) | OPV)

rgba2luma_vec:
1:
    /* vtype = e32,m1,ta,ma */
    RVV_VSETVLI(5, 14, 0x0D0)     /* rd=t0(x5), rs1=a4(x14) */

    /* Prepare scalar constants once per stripmine iteration. */
    li  t2, 0xFF
    li  t3, 77
    li  t4, 150
    li  t5, 29

    /* Load planar channels. */
    RVV_VLE32(1, 10)              /* v1 = R plane, rs1=a0 */
    RVV_VLE32(2, 11)              /* v2 = G plane, rs1=a1 */
    RVV_VLE32(3, 12)              /* v3 = B plane, rs1=a2 */

    /* Keep low 8 bits only, for safety. */
    RVV_OPIVX(F6_VAND, 1, 7, 1)   /* v1 &= 0xFF */
    RVV_OPIVX(F6_VAND, 2, 7, 2)   /* v2 &= 0xFF */
    RVV_OPIVX(F6_VAND, 3, 7, 3)   /* v3 &= 0xFF */

    /* 77*r, 150*g, 29*b */
    RVV_OPIVX(F6_VMUL, 1, 28, 1)  /* v1 *= 77  */
    RVV_OPIVX(F6_VMUL, 2, 29, 2)  /* v2 *= 150 */
    RVV_OPIVX(F6_VMUL, 3, 30, 3)  /* v3 *= 29  */

    /* sum = v1 + v2 + v3 */
    RVV_OPIVV(F6_VADD, 4, 1, 2)
    RVV_OPIVV(F6_VADD, 4, 4, 3)

    /* Divide by 256 using scalar ALU per element would be expensive;
     * keep the same vector shift-right path as the packed kernel.
     */
    .word (((0x28) << 26) | (1 << 25) | ((4) << 20) | ((8) << 15) | (3 << 12) | ((4) << 7) | OPV) /* vsrl.vi v4,v4,8 */

    /* Store 32-bit outputs. */
    RVV_VSE32(4, 13)              /* vse32.v v4, (a3) */

    /* Advance all pointers by VL * 4 bytes. */
    slli t1, t0, 2
    add  a0, a0, t1
    add  a1, a1, t1
    add  a2, a2, t1
    add  a3, a3, t1
    sub  a4, a4, t0
    bnez a4, 1b

    ret
