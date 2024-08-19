#
# tests of vm's implementation.
make tests/vm/pt-grow-stack.result
make tests/vm/pt-grow-pusha.result
make tests/vm/pt-grow-bad.result
make tests/vm/pt-big-stk-obj.result
make tests/vm/pt-bad-addr.result
make tests/vm/pt-bad-read.result
make tests/vm/pt-write-code.result
make tests/vm/pt-write-code2.result
make tests/vm/pt-grow-stk-sc.result
make tests/vm/page-linear.result
make tests/vm/page-parallel.result
make tests/vm/page-merge-seq.result
make tests/vm/page-merge-par.result
make tests/vm/page-merge-stk.result
make tests/vm/page-merge-mm.result
make tests/vm/page-shuffle.result