;; This is a comment

;; C functions are in the c pseudo-namespace.
(c/printf "Hello from Lisp!\n")

(c/printf "Printing some unicode: Hélèneちゃんは%sですね\n" "可愛い")

(c/printf "An integer is: %d\n" 42)

(c/printf "A float is: %f\n" 3.14159)

(c/printf "A boolean is: %d or %d\n" true false)

(if (c/random_bool)
    (c/printf "If is true\n")
    (c/printf "If is false\n"))

nil ;; is a value as well
