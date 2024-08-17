#
# this file contains basic tests for file system, such as 
# create, lg, sm, synchronization, open, close, read, write, 
# remove, rox,
make tests/userprog/rox-simple.result
make tests/userprog/rox-child.result
make tests/userprog/rox-multichild.result
make tests/userprog/create-normal.result
make tests/userprog/create-empty.result
make tests/userprog/create-null.result
make tests/userprog/create-bad-ptr.result
make tests/userprog/create-long.result
make tests/userprog/create-exists.result
make tests/userprog/create-bound.result
make tests/filesys/base/lg-create.result
make tests/filesys/base/lg-full.result
make tests/filesys/base/lg-random.result
make tests/filesys/base/lg-seq-block.result
make tests/filesys/base/lg-seq-random.result
make tests/filesys/base/sm-create.result
make tests/filesys/base/sm-full.result
make tests/filesys/base/sm-random.result
make tests/filesys/base/sm-seq-block.result
make tests/filesys/base/sm-seq-random.result
make tests/filesys/base/syn-read.result
make tests/filesys/base/syn-remove.result
make tests/filesys/base/syn-write.result
make tests/userprog/open-normal.result
make tests/userprog/open-missing.result
make tests/userprog/open-boundary.result
make tests/userprog/open-empty.result
make tests/userprog/open-null.result
make tests/userprog/open-bad-ptr.result
make tests/userprog/open-twice.result
make tests/userprog/close-normal.result
make tests/userprog/close-twice.result
make tests/userprog/close-stdin.result
make tests/userprog/close-stdout.result
make tests/userprog/close-bad-fd.result
make tests/userprog/read-normal.result
make tests/userprog/read-bad-ptr.result
make tests/userprog/read-boundary.result
make tests/userprog/read-zero.result
make tests/userprog/read-stdout.result
make tests/userprog/read-bad-fd.result
make tests/userprog/write-normal.result
make tests/userprog/write-bad-ptr.result
make tests/userprog/write-boundary.result
make tests/userprog/write-zero.result
make tests/userprog/write-stdin.result
make tests/userprog/write-bad-fd.result