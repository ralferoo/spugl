#!/usr/bin/perl

open(DEF, ">client/gen_command_defs.h") || die "Can't open defs file\n";
open(EXT, ">server/spu/gen_command_exts.inc") || die "Can't open exts file\n";
open(TBL, ">server/spu/gen_command_table.inc") || die "Can't open table file\n";

@x=();

while (<>) {
	if (/^\s*\/\*\s*(\d+)\s*\*\/int\s*imp([^\s(]*)\s*\(\s*\w*\s*(\w*\s*)\*\s*/) {
		($ord,$name,$real)=($1,$2,$2);

		while ($name=~s/([A-Z])/sprintf("_%c",ord($1)+32)/seg) {}
#		$name=~s/_//g;
		$name=~tr/[a-z]/[A-Z]/;

		print DEF "#define FIFO_COMMAND$name $ord\n";
		print EXT "extern FIFO_COMMAND imp$real;\n";
		print TBL "[FIFO_COMMAND$name] = (FIFO_COMMAND*) &imp$real,\n";

		if (defined($x{$ord})) {
			die "Ordinal $ord redefined on 'imp$real', previous definition was 'imp$x{$ord}'\n";
		}
		$x{$ord}=$real;
	}
}

close DEF;
close EXT;
close TBL;

