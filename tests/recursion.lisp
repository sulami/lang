;; This tests that we can do simple recursion.
;; TODO something is fishy here for targets > 100.

(defn (int count) ((int i))
  (c/print_value i)
  (c/print_value "\n")
  (if (< i 10)
      (count (+ 1 i))
      i))

(c/print_value (count 0))
(c/print_value "\n")

;; This tests that we can recurse without blowing the stack

;; (defn (int countmore) ((int i))
;;   (if (< i 100)
;;       (countmore (+ i i))
;;       i))

;; (c/print_value (countmore 2))
;; (c/print_value "\n")
