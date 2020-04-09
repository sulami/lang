;; This tests that we can declare a function and call it.

(defn (int double) ((int x))
  (* 2 x))

(c/print_int (double 21))
