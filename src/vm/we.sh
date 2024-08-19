#
# test for exec-wait functionality.
make tests/userprog/exec-once.result
make tests/userprog/exec-arg.result
make tests/userprog/exec-bound.result
make tests/userprog/exec-bound-2.result
make tests/userprog/exec-bound-3.result
make tests/userprog/exec-multiple.result
make tests/userprog/exec-missing.result
make tests/userprog/exec-bad-ptr.result
make tests/userprog/wait-simple.result
make tests/userprog/wait-twice.result
make tests/userprog/wait-killed.result
make tests/userprog/wait-bad-pid.result
make tests/userprog/multi-recurse.result
make tests/userprog/multi-child-fd.result
