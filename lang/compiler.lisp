;; This is the compiler, bootstrapped.

;; At the same time this is also the ultimate test, as it's the most
;; complex program written in this language.

;; Declarations of external functions.

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
(declare car value (value))
(declare cdr value (value))
(declare alist value (value value value))
(declare aget value (value value))
(declare array value (i32 value))

;; LLVM
(declare LLVMModuleCreateWithName void_ptr (string))
(declare LLVMAddFunction void_ptr (void_ptr string void_ptr))
(declare LLVMFunctionType void_ptr (void_ptr void_ptr i32 bool))
(declare LLVMAppendBasicBlock void_ptr (void_ptr string))
(declare LLVMCreateBuilder void_ptr ())
(declare LLVMPositionBuilderAtEnd void (void_ptr void_ptr))
(declare LLVMBuildRet void (void_ptr void_ptr))
(declare LLVMInitializeNativeTarget void ())
(declare LLVMCreateExecutionEngineForModule void_ptr (void_ptr void_ptr void_ptr))
(declare LLVMDumpModule void (void_ptr))
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
                                       ast))))))))))

(def main-module (LLVMModuleCreateWithName "foo"))
(def I32 (LLVMInt32Type))
;; Array of pointers
(def main-param-types (array 128 (cons I32
                                   (cons I32
                                         nil))))

(def main-type (LLVMFunctionType I32
                                 main-param-types
                                 2
                                 false))
(def main-function (LLVMAddFunction main-module
                                    "mainception"
                                    main-type))
(def entry (LLVMAppendBasicBlock main-function "entry"))
(def builder (LLVMCreateBuilder))
(LLVMPositionBuilderAtEnd builder entry)
(def rv (LLVMConstInt I32 42 0))
(LLVMBuildRet builder rv)
(LLVMInitializeNativeTarget)
(LLVMCreateExecutionEngineForModule nil main-module nil)
(LLVMDumpModule main-module)

(defun compile (args)
  (nebula_debug main-module)
  (let ((source-file (nth args 1)))
    (print "Compiling ")
    (print source-file)
    (println "...")
    (let ((source-code (str->cons (slurp source-file))))
      (println (cadr (parse source-code nil))))))

(compile argv)

