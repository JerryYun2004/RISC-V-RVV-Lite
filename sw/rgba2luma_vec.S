    .section .text
    .globl rgba2luma_vec
    .align 2

/* ============================================================
 * rgba2luma_vec(in=a0, out=a1, n=a2)
 *   in : uint32_t pixels (0xAABBGGRR)
 *   out: uint32_t luma (one 32-bit per pixel, low 8 bits used)
 *   n  : number of pixels
 *
 * Requires RVV standard encodings in hardware.
 * Assembler does NOT need -march=...v because we emit raw .word.
 * ============================================================ */

/* ---- Helpers to build 32-bit instruction words ---- */
#define OPV      0x57
#define LOADFP   0x07
#define STOREFP  0x27

/* vsetvli: opcode=0x57, funct3=111, imm[10:0]=vtype */
#define RVV_VSETVLI(rd, rs1, vtypei) \
  .word (((vtypei) << 20) | ((rs1) << 15) | (7 << 12) | ((rd) << 7) | OPV)

/* vle32.v: opcode=0x07, width(funct3)=110, unit-stride: nf=0,mew=0,mop=00, lumop=0, vm=1 */
#define RVV_VLE32(vd, rs1) \
  .word ((1 << 25) | ((rs1) << 15) | (6 << 12) | ((vd) << 7) | LOADFP)

/* vse32.v: opcode=0x27, width(funct3)=110, unit-stride: nf=0,mew=0,mop=00, sumop=0, vm=1 */
#define RVV_VSE32(vs3, rs1) \
  .word ((1 << 25) | ((rs1) << 15) | (6 << 12) | ((vs3) << 7) | STOREFP)

/* OP-V formats (opcode=0x57) */
#define F3_OPIVV 0   /* vv */
#define F3_OPIVX 4   /* vx */
#define F3_OPIVI 3   /* vi */

/* funct6 values (must match your RTL):
 * VADD: 000000
 * VMUL: 100101
 * VAND: 001001
 * VSRL: 101000
 */
#define F6_VADD 0x00
#define F6_VMUL 0x25
#define F6_VAND 0x09
#define F6_VSRL 0x28

#define RVV_OPIVV(f6, vd, vs1, vs2) \
  .word (((f6) << 26) | (1 << 25) | ((vs2) << 20) | ((vs1) << 15) | (F3_OPIVV << 12) | ((vd) << 7) | OPV)

#define RVV_OPIVX(f6, vd, rs1, vs2) \
  .word (((f6) << 26) | (1 << 25) | ((vs2) << 20) | ((rs1) << 15) | (F3_OPIVX << 12) | ((vd) << 7) | OPV)

#define RVV_OPIVI(f6, vd, imm5, vs2) \
  .word (((f6) << 26) | (1 << 25) | ((vs2) << 20) | ((imm5) << 15) | (F3_OPIVI << 12) | ((vd) << 7) | OPV)

rgba2luma_vec:
1:
    /* vtype for e32,m1,ta=1,ma=1:
     * vlmul=0, vsew=2, vta=1, vma=1  => vtype = 0xD0
     */
    RVV_VSETVLI(5, 12, 0x0D0)     /* rd=t0(x5), rs1=a2(x12) */

    /* v0 = load 8 pixels (or less if n<8) */
    RVV_VLE32(0, 10)              /* vd=v0, rs1=a0 */

    /* Prepare scalar constants */
    li  t2, 0xFF
    li  t3, 77
    li  t4, 150
    li  t5, 29

    /* r = p & 0xFF */
    RVV_OPIVX(F6_VAND, 1, 7, 0)   /* v1 = vand.vx(v0, t2) ; rs1=t2(x7) */

    /* g = (p >> 8) & 0xFF */
    RVV_OPIVI(F6_VSRL, 2, 8, 0)   /* v2 = vsrl.vi(v0, 8) */
    RVV_OPIVX(F6_VAND, 2, 7, 2)   /* v2 = vand.vx(v2, t2) */

    /* b = (p >> 16) & 0xFF */
    RVV_OPIVI(F6_VSRL, 3, 16, 0)  /* v3 = vsrl.vi(v0, 16) */
    RVV_OPIVX(F6_VAND, 3, 7, 3)   /* v3 = vand.vx(v3, t2) */

    /* 77*r, 150*g, 29*b */
    RVV_OPIVX(F6_VMUL, 1, 28, 1)  /* v1 = vmul.vx(v1, t3) ; rs1=t3(x28) */
    RVV_OPIVX(F6_VMUL, 2, 29, 2)  /* v2 = vmul.vx(v2, t4) ; rs1=t4(x29) */
    RVV_OPIVX(F6_VMUL, 3, 30, 3)  /* v3 = vmul.vx(v3, t5) ; rs1=t5(x30) */

    /* sum = v1 + v2 + v3 */
    RVV_OPIVV(F6_VADD, 4, 1, 2)   /* v4 = vadd.vv(v1, v2) */
    RVV_OPIVV(F6_VADD, 4, 4, 3)   /* v4 = vadd.vv(v4, v3) */

    /* y = sum >> 8 */
    RVV_OPIVI(F6_VSRL, 4, 8, 4)

    /* store 32-bit results */
    RVV_VSE32(4, 11)              /* vse32.v v4, (a1) */

    /* advance pointers by VL (t0) */
    slli t1, t0, 2                /* bytes = vl * 4 */
    add  a0, a0, t1
    add  a1, a1, t1
    sub  a2, a2, t0
    bnez a2, 1b

    ret