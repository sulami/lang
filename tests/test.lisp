(declare malloc void_ptr (i32))
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
(declare hash i64 (string))

(defun print (x)
  (print_value x))

(defun println (x)
  (print x)
  (print "\n"))


;;;; testing passing strings over function boundaries

;; (defun return-a-string ()
;;   "a string that does things. this is indeed a very looong string.")

;; (defun call-a-fun ()
;;   (println (return-a-string)))

;; (defun print-a-string ()
;;   (println "this is a long inline string"))

;; (defun print-value-a-string ()
;;   (print_value "printing an inline string")
;;   (print_value (return-a-string)))

;; (call-a-fun)
;; (print-a-string)
;; (print-value-a-string)


;;; hash map testing


;; Wrapping RRB tree-based vector functions.

;; Note that due to the lack of an error system, these are currently
;; quite quiet on OOB errors.

(defun and (x y) (if x y false))
(defun = (x y) (value_equal x y))
(defun zero? (x) (= 0 x))
(defun dec (x) (- x 1))

(declare rrb_create void_ptr ())
(declare rrb_count i32 (void_ptr))
(declare rrb_nth value (void_ptr i32))
(declare rrb_pop void_ptr (void_ptr))
(declare rrb_peek value (void_ptr))
(declare rrb_push void_ptr (void_ptr value))
(declare rrb_update void_ptr (void_ptr i32 value))
(declare rrb_concat void_ptr (void_ptr void_ptr))
(declare rrb_slice void_ptr (void_ptr i32 i32))

(defun ->vec (ptr)
  (make_value 133 ptr))

(defun vector ()
  (->vec (rrb_create)))

(defun vpush (xs elm)
  (->vec (rrb_push xs elm)))

(defun vcount (xs)
  (rrb_count xs))

(defun vnth (xs i)
  (if (< i (vcount xs))
      (rrb_nth xs i)
      nil))

(defun vpop (xs)
  (if (zero? (vcount xs))
      xs
      (->vec (rrb_pop xs))))

(defun vpeek (xs)
  (if (zero? (vcount xs))
      nil
      (rrb_peek xs)))

(defun vupdate (xs i elm)
  (if (< i (vcount xs))
      (->vec (rrb_update xs i elm))
      xs))

(defun vconcat (xs ys)
  (->vec (rrb_concat xs ys)))

(defun vslice (xs from to)
  (let ((len (vcount xs)))
    (if (and (< from len)
             (< to len))
        (->vec (rrb_slice xs from to))
        xs)))

;; Bad hash-maps. Going to do proper "Ideal Hash Trees" eventually.

(defun ->hash-map (xs)
  (make_value 134 xs))

(defun hash-map ()
  (->hash-map (vector)))

;; Currently a vector containing vectors of length 3 each.
;; [ hashed key, key, value ]
;; The hashed key will eventually be used for calculating the path
;; inside a more deeply nested vector.

(defun assoc (xs k v)
  (let ((hashed-key (hash k))
        (new-vec (vector)))
    (->hash-map
     (vpush xs
            (vpush
             (vpush
              ;; (vpush new-vec hashed-key)
              new-vec
              hashed-key)
             v)))))

(defun get (xs k)
  (let ((get* (lambda (xs k i)
                 (if (zero? i)
                     nil
                     (let ((pair (vnth xs (dec i))))
                       (if (= k (vnth pair 0))
                           (vnth pair 1)
                           (recur xs k (dec i))))))))
    (get* xs (hash k) (vcount xs))))

(defun keys (xs)
  (let ((keys* (lambda (xs acc i)
                 (if (zero? i)
                     acc
                     (recur xs
                            (cons (vnth (vnth xs (dec i))
                                        1)
                                  acc)
                            (dec i))))))
    (keys* xs nil (vcount xs))))

;; (let ((hm (hash-map)))
;;   (let ((hm2 (assoc hm "foo" 42)))
;;     (println hm)))

(defun return-string ()
  "string")

(defun return-keyword ()
  :keyword)

(progn
  (println "done with setup")
  (println (return-string))
  (println (return-keyword))
  )
