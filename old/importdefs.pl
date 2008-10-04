#!/usr/bin/perl

open(DEF, ">gen_spu_command_defs.h") || die "Can't open defs file\n";
open(EXT, ">gen_spu_command_exts.h") || die "Can't open exts file\n";
open(TBL, ">gen_spu_command_table.h") || die "Can't open table file\n";

@x=();

while (<>) {
	if (/^\s*\/\*\s*(\d+)\s*\*\/void\s*\*\s*imp([^\s(]*)\s*\(\s*\w*\s*\*\s*/) {
		($ord,$name,$real)=($1,$2,$2);

		while ($name=~s/([A-Z])/sprintf("_%c",ord($1)+32)/seg) {}
#		$name=~s/_//g;
		$name=~tr/[a-z]/[A-Z]/;

		print DEF "#define SPU_COMMAND$name $ord\n";
		print EXT "extern SPU_COMMAND imp$real;\n";
		print TBL "[SPU_COMMAND$name] = (SPU_COMMAND*) &imp$real,\n";

		if (defined($x{$ord})) {
			die "Ordinal $ord redefined on 'imp$real', previous definition was 'imp$x{$ord}'\n";
		}
		$x{$ord}=$real;
	}
}

close DEF;
close EXT;
close TBL;

