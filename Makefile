O_LEVEL=-O0
LLVM_PATH=/usr/local/Cellar/llvm/10.0.0_3/bin
TIME=/usr/bin/time
CC=clang
CFLAGS=`${LLVM_PATH}/llvm-config --cflags` ${O_LEVEL}
LD=clang++
LDFLAGS=-lffi `${LLVM_PATH}/llvm-config --cxxflags --ldflags --libs core executionengine interpreter mcjit x86 --system-libs` ${O_LEVEL}
LLC=${LLVM_PATH}/llc
LLVM_DIS=${LLVM_PATH}/llvm-dis
LLDB=${LLVM_PATH}/lldb

all: compiler

compiler: compiler.o nebula.o rbb.o
	$(TIME) $(LD) $(LDFLAGS) -o $@ $^

%.o: lang/%.c
	$(TIME) $(CC) $(CFLAGS) -Wall -c $<

compiler.o: compiler.ll
	$(TIME) $(LLC) --filetype=obj -o=$@ $<

compiler.ll: lang/__init__.py lang/compiler.lisp
	$(TIME) poetry run lang lang/compiler.lisp

clean:
	rm -f *.o *.ll compiler bitception test

debug: compiler
	$(LLDB) $<

nebula.ll: lang/nebula.c
	$(TIME) $(CC) $(CFLAGS) -o $@ -S -emit-llvm $<

# self hostception

bitception: bitception.o
	$(TIME) $(LD) $(LDFLAGS) -o $@ $^

bitception.o: bitception.ll
	$(TIME) $(LLC) --filetype=obj -o=$@ $<

bitception.ll: lang/compiler.lisp compiler
	$(TIME) ./compiler $<

# test

testdebug: test
	$(LLDB) $<

test: test.o nebula.o rbb.o
	$(TIME) $(LD) $(LDFLAGS) -o $@ $^

test.o: test.ll
	$(TIME) $(LLC) --filetype=obj -o=$@ $<

test.ll: lang/__init__.py tests/test.lisp
	$(TIME) poetry run lang tests/test.lisp
