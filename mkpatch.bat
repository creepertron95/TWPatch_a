@cls
@pushd build
@bin2c -o ../mkpatch_a.h redshift.bin hole.bin reloc.bin agb_wide.bin twl_wide.bin trainer.bin ehandler.bin ehook.bin
@popd
@gcc -o mkpatch_b.exe -O2 mkpatch_b.c soos/lz.c soos/pat.c soos/red.c sha256.c -Ibuild -Wno-discarded-qualifiers -DLZ_SILENT && mkpatch_b %*
