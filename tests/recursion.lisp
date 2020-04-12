;; This tests that we can do simple recursion.
;; TODO something is fishy here for targets > 100.

(defn count (i)
  (c/print_value "counting: ")
  (c/print_value i)
  (c/print_value "\n")
  (if (< i 10)
      (count (+ 1 i))
      i))

(c/print_value (count 0))
(c/print_value "\n")

;; This tests that we can recurse without blowing the stack

(defn countmore (i)
  (if (< i 10000)
      (recur (+ 1 i))
      i))

(c/print_value (countmore 2))
(c/print_value "\n")
