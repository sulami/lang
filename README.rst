How this works:

Useful links:

* `LLVM 7 Manual <https://releases.llvm.org/7.0.0/docs/LangRef.html>`_
* `llvmlite bindings manual <https://llvmlite.readthedocs.io/en/v0.31.0/user-guide/ir/values.html>`_
* `Lisp -> LLVM compiler <https://github.com/eatonphil/ulisp>`_
  _

::
 poetry run lang > out.ll
 llc out.ll --filetype=obj
 cc out.o

