;; This tests that we can declare a function and call it.

(defn (int double) ((int x))
  (c/print_value "Doubling to: ")
  (let ((result (* 2 x)))
    (c/print_value result)
    (c/print_value "\n")
    result))

(c/print_value (double 21))
