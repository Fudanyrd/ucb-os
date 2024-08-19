#
# test the functionality of other system calls.
make tests/userprog/bad-read.result
make tests/userprog/bad-write.result
make tests/userprog/bad-read2.result
make tests/userprog/bad-write2.result
make tests/userprog/bad-jump.result
make tests/userprog/bad-jump2.result
make tests/userprog/bad-stack.result
make tests/userprog/good-stack.result
make tests/userprog/good-stack-2.result
make tests/userprog/args-none.result
make tests/userprog/args-single.result
make tests/userprog/args-multiple.result
make tests/userprog/args-many.result
make tests/userprog/args-dbl-space.result
make tests/userprog/sc-bad-sp.result
make tests/userprog/sc-bad-arg.result
make tests/userprog/sc-boundary.result
make tests/userprog/sc-boundary-2.result
make tests/userprog/sc-boundary-3.result
make tests/userprog/halt.result
make tests/userprog/exit.result
