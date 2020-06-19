(declare print_value void (value))

(defun print (x)
  (print_value x))

(defun println (x)
  (print x)
  (print "\n"))

(defun return-a-string ()
  "a string that does things. this is indeed a very looong string.")

(defun call-a-fun ()
  (println (return-a-string)))

(call-a-fun)
