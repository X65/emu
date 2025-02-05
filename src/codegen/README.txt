This directory contains code-generation python scripts which will generate the
w65c816s.h and m6502.h headers.

In a bash compatible shell run:

```sh
./w65c816s_gen.sh
./m6502_gen.sh
```

This will run Python3 inside a virtual environment and read/write
the `chips/w65c816s.h` and `chips/m6502.h` headers.
