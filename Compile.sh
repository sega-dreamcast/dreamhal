#!/bin/bash
#
# GCC SH4 Linux Compile Script (Meant for GCC >= 9.2.0 and Binutils >= 2.33.1)
#
#
# Made with special permission for DreamHAL.
#

#
# set +v disables displaying all of the code you see here in the command line
#

set +v

#
# Set various paths needed for portable compilation
#

CurDir=$PWD
GCC_FOLDER_NAME=../gcc-sh4
BINUTILS_FOLDER_NAME=../binutils-sh4
LinkerScript="shlelf.xc"

# So that GCC knows where to find as and ld
# Interestingly, GCC uses sh4-elf-ld, but needs non-prefixed "as" for some reason...
# Well, we have both, so add them to the path like this.
export PATH=$BINUTILS_FOLDER_NAME/bin:$BINUTILS_FOLDER_NAME/sh4-elf/bin:$PATH

#
# These help with debugging the PATH to make sure it is set correctly
#

#echo $PATH
#read -n1 -r -p "Press any key to continue..."

#
# First things first, delete the objects list to rebuild it later
#

rm objects.list

#
# Create the HFILES variable, which contains the massive set of includes (-I)
# needed by GCC.
#
# Two of the include folders are always included, and they
# are $CurDir/inc/ (the user-header directory) and $CurDir/startup/
#

HFILES=-I$CurDir/inc/\ -I$CurDir/startup/

#
# Loop through the h_files.txt file and turn each include directory into -I strings
# IMPORTANT: h_files.txt needs to have LF line endings.
#

while read h; do
  HFILES=$HFILES\ -I$h
done < $CurDir/h_files.txt

#
# These are useful for debugging this script, namely to make sure you aren't
# missing any include directories.
#

#echo $HFILES
#read -n1 -r -p "Press any key to continue..."

#
# Compile the .c files in the startup folder
#

set -v
for f in $CurDir/startup/*.c; do
  echo "$GCC_FOLDER_NAME/bin/sh4-elf-gcc" -ml -m4-single-only -mno-accumulate-outgoing-args -mpretend-cmove -mfsca -mfsrra -O3 -ffreestanding -ffp-contract=fast -fno-unsafe-math-optimizations -fno-finite-math-only -fomit-frame-pointer -fno-delete-null-pointer-checks -fno-common -fno-zero-initialized-in-bss -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector -fno-stack-check -fno-strict-aliasing -fno-merge-all-constants -fno-merge-constants --std=gnu11 $HFILES -g3 -Wall -Wextra -Wdouble-promotion -Wpedantic -fmessage-length=0 -c -MMD -MP -Wa,-adghlmns="${f%.*}.out" -MF"${f%.*}.d" -MT"${f%.*}.o" -o "${f%.*}.o" "${f%.*}.c"
  "$GCC_FOLDER_NAME/bin/sh4-elf-gcc" -ml -m4-single-only -mno-accumulate-outgoing-args -mpretend-cmove -mfsca -mfsrra -O3 -ffreestanding -ffp-contract=fast -fno-unsafe-math-optimizations -fno-finite-math-only -fomit-frame-pointer -fno-delete-null-pointer-checks -fno-common -fno-zero-initialized-in-bss -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector -fno-stack-check -fno-strict-aliasing -fno-merge-all-constants -fno-merge-constants --std=gnu11 $HFILES -g3 -Wall -Wextra -Wdouble-promotion -Wpedantic -fmessage-length=0 -c -MMD -MP -Wa,-adghlmns="${f%.*}.out" -MF"${f%.*}.d" -MT"${f%.*}.o" -o "${f%.*}.o" "${f%.*}.c" &
done
set +v

#
# Compile the .S files in the startup folder (Any assembly files needed to
# initialize the system)
#

# "as" version
#set -v
#for f in $CurDir/startup/*.s; do
#  echo "$BINUTILS_FOLDER_NAME/bin/sh4-elf-as" -little $HFILES -g -o "${f%.*}.o" "${f%.*}.s"
#  "$BINUTILS_FOLDER_NAME/bin/sh4-elf-as" -little $HFILES -g -o "${f%.*}.o" "${f%.*}.s" &
#done
#set +v

# "gcc" version
set -v
for f in $CurDir/startup/*.S; do
  echo "$GCC_FOLDER_NAME/bin/sh4-elf-gcc" -ml -m4-single-only -mno-accumulate-outgoing-args -mpretend-cmove -mfsca -mfsrra -O3 -ffreestanding -ffp-contract=fast -fno-unsafe-math-optimizations -fno-finite-math-only -fomit-frame-pointer -fno-delete-null-pointer-checks -fno-common -fno-zero-initialized-in-bss -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector -fno-stack-check -fno-strict-aliasing -fno-merge-all-constants -fno-merge-constants --std=gnu11 $HFILES -g3 -Wall -Wextra -Wdouble-promotion -Wpedantic -fmessage-length=0 -c -MMD -MP -Wa,-adghlmns="${f%.*}.out" -MF"${f%.*}.d" -MT"${f%.*}.o" -o "${f%.*}.o" "${f%.*}.S"
  "$GCC_FOLDER_NAME/bin/sh4-elf-gcc" -ml -m4-single-only -mno-accumulate-outgoing-args -mpretend-cmove -mfsca -mfsrra -O3 -ffreestanding -ffp-contract=fast -fno-unsafe-math-optimizations -fno-finite-math-only -fomit-frame-pointer -fno-delete-null-pointer-checks -fno-common -fno-zero-initialized-in-bss -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector -fno-stack-check -fno-strict-aliasing -fno-merge-all-constants -fno-merge-constants --std=gnu11 $HFILES -g3 -Wall -Wextra -Wdouble-promotion -Wpedantic -fmessage-length=0 -c -MMD -MP -Wa,-adghlmns="${f%.*}.out" -MF"${f%.*}.d" -MT"${f%.*}.o" -o "${f%.*}.o" "${f%.*}.S" &
done
set +v

#
# Compile user .c files
#

set -v
for f in $CurDir/src/*.c; do
  echo "$GCC_FOLDER_NAME/bin/sh4-elf-gcc" -ml -m4-single-only -mno-accumulate-outgoing-args -mpretend-cmove -mfsca -mfsrra -Og -ffreestanding -ffp-contract=fast -fno-unsafe-math-optimizations -fno-finite-math-only -fomit-frame-pointer -fno-delete-null-pointer-checks -fno-common -fno-zero-initialized-in-bss -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector -fno-stack-check -fno-strict-aliasing -fno-merge-all-constants -fno-merge-constants --std=gnu11 $HFILES -g3 -Wall -Wextra -Wdouble-promotion -Wpedantic -fmessage-length=0 -c -MMD -MP -Wa,-adghlmns="${f%.*}.out" -MF"${f%.*}.d" -MT"${f%.*}.o" -o "${f%.*}.o" "${f%.*}.c"
  "$GCC_FOLDER_NAME/bin/sh4-elf-gcc" -ml -m4-single-only -mno-accumulate-outgoing-args -mpretend-cmove -mfsca -mfsrra -Og -ffreestanding -ffp-contract=fast -fno-unsafe-math-optimizations -fno-finite-math-only -fomit-frame-pointer -fno-delete-null-pointer-checks -fno-common -fno-zero-initialized-in-bss -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector -fno-stack-check -fno-strict-aliasing -fno-merge-all-constants -fno-merge-constants --std=gnu11 $HFILES -g3 -Wall -Wextra -Wdouble-promotion -Wpedantic -fmessage-length=0 -c -MMD -MP -Wa,-adghlmns="${f%.*}.out" -MF"${f%.*}.d" -MT"${f%.*}.o" -o "${f%.*}.o" "${f%.*}.c" &
done
set +v

#
# Wait for compilation to complete
#

echo
echo Waiting for compilation to complete...
echo

wait

#
# Add compiled .o files from the startup directory to objects.list
#

# "startup.o" MUST be first in the linking order to be able to convert ELFs to BINs
echo "$CurDir/startup/startup.o" | tee -a objects.list

for f in $CurDir/startup/*.o; do
  if [ "$f" != "$CurDir/startup/startup.o" ]
  then
    echo "$f" | tee -a objects.list
  fi
done

#
# Add compiled user .o files to objects.list
#

for f in $CurDir/src/*.o; do
  echo "$f" | tee -a objects.list
done

#
# Link the object files using all the objects in objects.list and an optional
# linker script (it would go in the Backend/Linker directory) to generate the
# output binary, which is called "program.elf"
#
# NOTE: Linkerscripts may be needed for bigger projects
#

set -v
"$GCC_FOLDER_NAME/bin/sh4-elf-gcc" -T$LinkerScript -static -nostdlib -nostartfiles -s -Wl,-Ttext=0x8c010000 -Wl,--warn-common -Wl,--no-undefined -Wl,-z,text -Wl,-z,norelro -Wl,-z,now -Wl,-Map=output.map -o "program.elf" @"objects.list"
set +v
# Remove -s in the above command to keep debug symbols in the output binary.

#
# Output program section details and program size information
#

echo
"$BINUTILS_FOLDER_NAME/bin/sh4-elf-readelf" -egnrud "program.elf"
echo
echo Generating binary and Printing size information:
echo
"$BINUTILS_FOLDER_NAME/bin/sh4-elf-objcopy" -O binary "program.elf" "program.bin"
"$BINUTILS_FOLDER_NAME/bin/sh4-elf-size" "program.elf"
echo

#
# Prompt user for next action
#

read -p "Cleanup, recompile, or done? [c for cleanup, r for recompile, any other key for done] " UPL

echo
echo "**********************************************************"
echo

case $UPL in
  [cC])
    exec ./Cleanup.sh
  ;;
  [rR])
    exec ./Compile.sh
  ;;
  *)
  ;;
esac
