rm filesys.dsk
pintos-mkdisk filesys.dsk --filesys-size=4
pintos -- -f -q

# binary file
pintos -p ../../examples/cat -a cat -- -q   # read the content of a file
pintos -p ../../examples/echo -a echo -- -q
pintos -p ../../examples/fio -a fio -- -q

# text file 
pintos -p ../../examples/echo.c -a echo.c -- -q
pintos -p ../../examples/empty.txt -a empty.txt -- -q
pintos -p ../../examples/fio.c -a fio.c -- -q
