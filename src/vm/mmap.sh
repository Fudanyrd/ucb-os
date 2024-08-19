#
# tests of mmap's functionality.
make tests/vm/mmap-read.result
make tests/vm/mmap-close.result
make tests/vm/mmap-unmap.result
make tests/vm/mmap-overlap.result
make tests/vm/mmap-twice.result
make tests/vm/mmap-write.result
make tests/vm/mmap-exit.result
make tests/vm/mmap-shuffle.result
make tests/vm/mmap-bad-fd.result
make tests/vm/mmap-clean.result
make tests/vm/mmap-inherit.result
make tests/vm/mmap-misalign.result
make tests/vm/mmap-null.result
make tests/vm/mmap-over-code.result
make tests/vm/mmap-over-data.result
make tests/vm/mmap-over-stk.result
make tests/vm/mmap-remove.result
make tests/vm/mmap-zero.result
