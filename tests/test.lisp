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

(defun print-a-string ()
  (println "this is a long inline string"))

(call-a-fun)
(print-a-string)
