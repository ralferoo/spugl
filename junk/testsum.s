	.file	"testsum.c"
	.section	.rodata.cst16,"aM",@progbits,16
	.align	4
.LC0:
	.byte	4
	.byte	5
	.byte	6
	.byte	7
	.byte	-128
	.byte	-128
	.byte	-128
	.byte	-128
	.byte	12
	.byte	13
	.byte	14
	.byte	15
	.byte	-128
	.byte	-128
	.byte	-128
	.byte	-128
.text
	.align	3
	.global	add
	.type	add, @function
add:
	hbr	.L3,$lr
	lqr	$2,.LC0		// shift pattern
	rotqmbyi	$4,$4,-4	// promote unsigned long to long long
	cg	$5,$3,$4	// $5 = carry
	shufb	$6,$5,$5,$2	// $6 = $5 << 32
	addx	$6,$3,$4	// $6 = (carry<<32) + $3 + $4
	ori	$3,$6,0		// move back to $3
	nop	$127
.L3:
	bi	$lr
	.size	add, .-add
	.ident	"GCC: (GNU) 4.0.2 (CELL 3-2, Apr 11 2006)"
