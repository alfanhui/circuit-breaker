#!/bin/bash
./scripts/clean.bat
./scripts/compile.bat

# Get objdump metrics for function instruction sizes
find ../../jo_engine -type f -name "*.o" | \
    xargs ../../Compiler/WINDOWS/sh-elf/bin/objdump.exe -t | \
    awk -Wposix -v OFS="\t" '{if(NF>=4 && $3!=0) printf \
        "%s%s%s%s%s%s%s%s%s%s%d%s%s\n",$1,OFS,$2,OFS,$3,\
        OFS,$4,OFS,$5,OFS,"0x"$5,OFS,$6;else if ($3!=0) print $0}' > \
    objdump.map
find ./ -type f -name "*.o" | \
    xargs ../../Compiler/WINDOWS/sh-elf/bin/objdump.exe -t | \
    awk -Wposix -v OFS="\t" '{if(NF>=4 && $3!=0) printf \
        "%s%s%s%s%s%s%s%s%s%s%d%s%s\n",$1,OFS,$2,OFS,$3,\
        OFS,$4,OFS,$5,OFS,"0x"$5,OFS,$6;else if ($3!=0) print $0}' >> \
    objdump.map

./scripts/run_with_yabaSanshiro.bat
#./scripts/run_with_mednafen.bat
#./scripts/run_with_yabause.bat # Artifacts appear in background
#./scripts/run_with_daemon_tools_and_ssf.bat