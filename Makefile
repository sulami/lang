O_LEVEL=-Ofast
LLVM_PATH=/usr/local/Cellar/llvm/10.0.0_3/bin
CC=clang
CFLAGS=`${LLVM_PATH}/llvm-config --cflags` ${O_LEVEL}
LD=clang++
LDFLAGS=-lffi `${LLVM_PATH}/llvm-config --cxxflags --ldflags --libs core executionengine interpreter mcjit x86 --system-libs` ${O_LEVEL}
LLC=${LLVM_PATH}/llc
LLVM_DIS=${LLVM_PATH}/llvm-dis
LLDB=${LLVM_PATH}/lldb

all: out

out: out.o nebula.o rbb.o
	$(LD) $(LDFLAGS) -o $@ $^

%.o: lang/%.c
	$(CC) $(CFLAGS) -Wall -c $<

out.o: out.ll
	$(LLC) --filetype=obj -o=$@ $<

out.ll: lang/__init__.py lang/compiler.lisp
	poetry run lang lang/compiler.lisp

clean:
	rm -f *.o *.ll out bitception

debug: out
	$(LLDB) $<

nebula.ll: lang/nebula.c
	$(CC) $(CFLAGS) -o $@ -S -emit-llvm $<

# self hostception

bitception: bitception.o
	$(LD) $(LDFLAGS) -o $@ $^

bitception.o: bitception.ll
	$(LLC) --filetype=obj -o=$@ $<

bitception.ll: lang/compiler.lisp out
	./out $<
