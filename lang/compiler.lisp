;; -*- eval: (sly-mode -1) -*-

;; This is the compiler, bootstrapped.

;; At the same time this is also the ultimate test, as it's the most
;; complex program written in this language.

;; Declarations of external functions.

;; C stdlib
(declare malloc void_ptr (i32))

;; libnebula
(declare nebula_debug void (void_ptr))
(declare type value (value))
(declare value_equal value (value value))
(declare print_value void (value))
(declare value_truthy bool (value))
(declare concat_strings value (value value))
(declare string_to_cons value (value))
(declare cons_to_string value (value))
(declare read_file value (value value))
(declare write_file value (value value value))
(declare read_line value ())
(declare car value (value))
(declare cdr value (value))
(declare alist value (value value value))
(declare aget value (value value))
(declare array value (i32 value))

;; LLVM
(declare LLVMModuleCreateWithName void_ptr (string))
(declare LLVMAddFunction void_ptr (void_ptr string void_ptr))
(declare LLVMFunctionType void_ptr (void_ptr void_ptr i32 i32))
(declare LLVMAppendBasicBlock void_ptr (void_ptr string))
(declare LLVMCreateBuilder void_ptr ())
(declare LLVMPositionBuilderAtEnd void (void_ptr void_ptr))
(declare LLVMGetParam void_ptr (void_ptr i32))
(declare LLVMBuildAdd void_ptr (void_ptr void_ptr void_ptr string))
(declare LLVMBuildRet void (void_ptr void_ptr))
(declare LLVMPrintModuleToString string (void_ptr))
(declare LLVMWriteBitcodeToFile void (void_ptr string))
(declare LLVMConstInt void_ptr (void_ptr i32 i32))
(declare LLVMInt32Type void_ptr ())

;; Convenience functions

;; I/O

(defun print (x)
  (print_value x))

(defun println (x)
  (print x)
  (print "\n"))

(defun slurp (file-name)
  (read_file file-name "r"))

(defun spit (file-name s)
  (write_file file-name "w" s))

;; Logic

(defun not (x)
  (if x false true))

(defun and (x y)
  (if x y false))

(defun or (x y)
  (if x true y))

(defun xor (x y)
  (if x y (not y)))

(defun if-not (condition then else)
  (if (not condition)
      then
      else))

;; Values

(defun = (x y)
  (value_equal x y))

(defun not= (x y)
  (not (= x y)))

(defun nil? (x)
  (= nil x))

(defun inc (x)
  (+ x 1))

(defun dec (x)
  (- x 1))

(defun zero? (x)
  (= 0 x))

(defun pos? (x)
  (< 0 x))

(defun neg? (x)
  (< x 0))

;; Lists

(defun cadr (x)
  (car (cdr x)))

(defun reverse (l)
  (let ((reverse* (lambda (from to)
                    (if (nil? from)
                        to
                        (recur (cdr from)
                               (cons (car from) to))))))
    (reverse* l nil)))

(defun count (l)
  (let ((count* (lambda (l n)
                  (if (nil? l)
                      n
                      (recur (cdr l) (inc n))))))
    (count* l 0)))

(defun last (l)
  (if (nil? (cdr l))
      (car l)
      (recur (cdr l))))

(defun nth (l i)
  (if (or (= 0 i)
          (nil? l))
      (car l)
      (recur (cdr l) (dec i))))

(defun take (n l)
  (let ((take* (lambda (n from to)
                 (if (or (nil? from)
                         (= 0 n))
                     to
                     (recur (dec n)
                            (cdr from)
                            (cons (car from) to))))))
    (reverse (take* n l nil))))

(defun drop (n l)
  (if (= 0 n)
      l
      (recur (dec n) (cdr l))))

(defun take-while (f l)
  (let ((take-while* (lambda (f from to)
                       (if (or (nil? from)
                               (not (f (car from))))
                           to
                           (recur f
                                  (cdr from)
                                  (cons (car from) to))))))
    (reverse (take-while* f l nil))))

(defun drop-while (f l)
  (if (or (nil? l)
          (not (f (car l))))
      l
      (recur f (cdr l))))

(defun filter (f l)
  (let ((filter* (lambda (f from to)
                   (if (nil? from)
                       to
                       (let ((this (car from)))
                         (recur f
                                (cdr from)
                                (if (f this)
                                    (cons this to)
                                    to)))))))
    (reverse (filter* f l nil))))

(defun map (f l)
  (let ((map* (lambda (f from to)
                (if (nil? from)
                    to
                    (recur f
                           (cdr from)
                           (cons (f (car from)) to))))))
    (reverse (map* f l nil))))

(defun reduce (f l)
  (let ((first-result (f (car l) (car (cdr l))))
        (remaining-list (cdr (cdr l)))
        (reduce* (lambda (f from to)
                   (if (nil? from)
                       to
                       (recur f
                              (cdr from)
                              (f to (car from)))))))
    (reduce* f remaining-list first-result)))

;; Chars

(defun whitespace? (c)
  (or (= c \space)
      (or (= c \tab)
          (= c \newline))))

;; Strings

(defun concat (x y)
  (concat_strings x y))

(defun str->cons (s)
  (string_to_cons s))

(defun cons->str (l)
  (cons_to_string l))

;; Types
;; There's currently no notation for types, so we just derive them.

(def NIL (type nil))
(def BOOL (type true))
(def INT (type 42))
(def FLOAT (type 3.14159))
(def CHAR (type \a))
(def TYPE (type (type nil)))
(def STRING (type "foo"))
(def CONS (type (cons 1 nil)))
(def FUNCTION (type (lambda (x) x)))

;; nil? is defined in terms of its value above.
(defun bool? (x) (= BOOL (type x)))
(defun int? (x) (= INT (type x)))
(defun float? (x) (= FLOAT (type x)))
(defun char? (x) (= CHAR (type x)))
(defun type? (x) (= TYPE (type x)))
(defun string? (x) (= STRING (type x)))
(defun cons? (x) (= CONS (type x)))
(defun function? (x) (= FUNCTION (type x)))

;; The actual compiler.

(def *debug* false)

(defun debugp (n x)
  (if *debug*
      (progn
        (print "[DEBUG] ")
        (print (concat n ": "))
        (println x))
      nil))

;; Parser

(defun parse (unparsed ast)
  (let ((c (car unparsed))
        (current-word (car ast))
        (rest (cdr ast)))

    (debugp "reading" c)
    (if (nil? c)
        ;; We're done parsing.
        (cons unparsed
              (cons (reverse ast)
                    nil))

        (if (= \\ c)
            ;; Escaped character, add as token.
            ;; XXX Currently only a single char.
            ;; FIXME Can't be the last char.
            (recur (drop 2 unparsed)
                   (cons (nth unparsed 1)
                         ast))

            (if (= \; c)
                ;; A comment, skip until the newline.
                (recur (drop-while (lambda (x) (not= \newline x))
                                   unparsed)
                       ast)

                (if (whitespace? c)
                    ;; Just skip ahead.
                    (recur (drop-while whitespace? unparsed)
                           ast)

                    (if (= \( c)
                        ;; Inner sexp, call down one level.
                        (let ((inner (parse (cdr unparsed)
                                            nil
                                            false)))
                          (let ((inner-unparsed (car inner))
                                (inner-ast (cadr inner)))
                            (debugp "inner ast" inner-ast)
                            (recur inner-unparsed
                                   (cons inner-ast
                                         ast))))

                        (if (= \) c)
                            ;; Done in this sexp, return up one level.
                            (cons (cdr unparsed)
                                  (cons (reverse ast) nil))

                            ;; Default case: take a word
                            (let ((take-word (lambda (x)
                                               (if (nil? x)
                                                   false
                                                   (and (not (whitespace? x))
                                                        (and (not= \( x)
                                                             (and (not= \) x)
                                                                  (not= \; x))))))))
                              (debugp "ast" ast)
                              (debugp "the word" (cons->str (take-while take-word unparsed)))
                              (recur (drop-while take-word unparsed)
                                     (cons (cons->str (take-while take-word unparsed))
                                           ast)))))))))))

(defun parse-string (s)
  (cadr (parse (str->cons s) nil)))

(defun compile-source (args)
  (let ((source-file (nth args 1)))
    (let ((source-code (slurp source-file)))
      (map println (parse-string source-code)))))

(def I32 (LLVMInt32Type))

(defun compile-module ()
  (def main-module (LLVMModuleCreateWithName "main-module"))
  ;; Array of pointers
  (def main-function-type (LLVMFunctionType I32
                                            (array 128 (cons I32
                                                             (cons I32
                                                                   nil)))
                                            2
                                            0))
  (def main-function (LLVMAddFunction main-module
                                      "main"
                                      main-function-type))
  (def main-entry (LLVMAppendBasicBlock main-function "entry"))
  (def main-builder (LLVMCreateBuilder))
  (LLVMPositionBuilderAtEnd main-builder main-entry)
  (def main-rv (LLVMConstInt I32 42 0))
  (LLVMBuildRet main-builder main-rv)
  (def module-ir (LLVMPrintModuleToString main-module))
  (spit "bitception.ll" module-ir))

(declare LLVMVerifyModule void (void_ptr void_ptr void_ptr))
(declare LLVMLinkInMCJIT void ())
(declare LLVMLinkInInterpreter void ())
(declare LLVMInitializeX86Target void ())
(declare LLVMCreateExecutionEngineForModule i32 (void_ptr void_ptr void_ptr))
(declare LLVMCreateGenericValueOfInt void_ptr (void_ptr i32 bool))
(declare LLVMRunFunction void_ptr (void_ptr void_ptr i32 void_ptr))
(declare puts void (void_ptr))

(defun jit-compile ()
  ;; This is mostly identitcal to the regular compile, just different
  ;; code. Note that `def' bindings are global on an LLVM-level, and
  ;; the Python compiler is not keeping track of where they are, so
  ;; they have to have unique names because we can't prefix them by
  ;; location. I don't think I'll fix this before bootstrapping.
  (def jit-module (LLVMModuleCreateWithName "jit-module"))
  (def jit-function-type (LLVMFunctionType I32
                                           (array 128 (cons I32
                                                            (cons I32
                                                                  nil)))
                                           2
                                           0))
  (def jit-function (LLVMAddFunction main-module
                                     "jit-add"
                                     jit-function-type))
  (def jit-entry (LLVMAppendBasicBlock jit-function "entry"))
  (def jit-builder (LLVMCreateBuilder))
  (LLVMPositionBuilderAtEnd jit-builder jit-entry)
  (def jit-rv (LLVMBuildAdd jit-builder
                            (LLVMGetParam jit-function 0)
                            (LLVMGetParam jit-function 1)
                            "rv"))
  (LLVMBuildRet jit-builder jit-rv)
  (def jit-verify-error 0)
  (LLVMVerifyModule jit-module 0 jit-verify-error)

  ;; This is the interesting part, setup an execution engine and JIT
  ;; compile the function above.
  (LLVMInitializeX86Target)
  (LLVMLinkInMCJIT)
  (LLVMLinkInInterpreter)
  ;; Pointers.
  (def jit-engine 0)
  (def jit-engine-error 0)
  (def jit-engine-creation (LLVMCreateExecutionEngineForModule jit-engine jit-module jit-engine-error))
  (if (not (= 0 jit-engine-creation))
      (progn (print "error creating execution engine: ")
             (println jit-engine-creation))
      nil)
  (def jit-args (array 128 (cons (LLVMCreateGenericValueOfInt I32 4 false)
                               (cons (LLVMCreateGenericValueOfInt I32 38 false)
                                     nil))))
  (def jit-llvm-result (LLVMRunFunction jit-engine jit-function 2 jit-args))
  nil
  )

(defun repl ()
  (print "> ")
  (let ((input (read_line)))
    (if (= "" input)
        (println "\nDone, bye.")
        (progn
          (map println (parse-string input))
          (recur)))))

(defun compily (args)
  (compile-source args)
  (compile-module)
  (jit-compile)
  (repl))

(compily argv)

;; (def error (make-pointer))
;; (LLVMVerifyModule main-module LLVMAbortProcessAction error)

;; (LLVMInitializeX86Target)

;; (def execution-engine-ref (make-pointer))
;; (def error (make-pointer))
;; (LLVMCreateExecutionEngineForModule execution-engine-ref main-module error)
;; (declare LLVMSetTarget string (void_ptr string))
;; (LLVMSetTarget main-module "x86_64-apple-darwin19.4.0") ;; apparently the local default triple
;; (LLVMWriteBitcodeToFile main-module "bitception.bc")

