.text

	.global _start
_start:

	# magic at start of header

	.byte	's'
	.byte	'p'
	.byte	'u'
	.byte	'g'
	.byte	'l'
	.byte	'-'
	.byte	'p'
	.byte	's'

	# size and definition of pixel shader

	.long	_end - _start
	.long	0	#_PREFIX_Definition - _start

	# function offsets
	
	.long	_PREFIX_InitFunc - _start
	.long	_PREFIX_RenderFunc - _start

	# reserved

	.long	0

