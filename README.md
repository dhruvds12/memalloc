# Custom Memory Allocator

## Implemented Functions
- malloc
- free
- calloc
- realloc

## Compilation
Compile as a shared library with:
```bash
g++ -fPIC -shared -o memalloc.so memalloc.cpp -pthread
```

## Testing
Set the LD_PRELOAD variable:
```bash
export LD_PRELOAD=$PWD/memalloc.so
```
Run a test program (e.g., `ls`):
```bash
ls
```
Unset LD_PRELOAD when done:
```bash
unset LD_PRELOAD
```

## Debugging
To enable debug output, compile with the `-DDEBUG` flag.
