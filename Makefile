LLVM_PATH=/usr/local/Cellar/llvm/10.0.0_3/bin
CC=clang
CFLAGS=-g `${LLVM_PATH}/llvm-config --cflags`
LD=clang++
LDFLAGS=`${LLVM_PATH}/llvm-config --cxxflags --ldflags --libs core --system-libs`
LLC=${LLVM_PATH}/llc
LLVM_DIS=${LLVM_PATH}/llvm-dis
LLDB=${LLVM_PATH}/lldb

all: out

out: out.o nebula.o
	$(LD) $(LDFLAGS) -o $@ $^

nebula.o: lang/nebula.c
	$(CC) $(CFLAGS) -Wall -c $<

out.o: out.ll
	$(LLC) --filetype=obj -o=$@ $<

out.ll: lang/__init__.py lang/compiler.lisp
	poetry run lang lang/compiler.lisp

clean:
	rm -f out.ll nebula.o out

debug: out
	$(LLDB) $<
