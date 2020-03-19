(ns eval
  "Evals an AST directly, without any further processing.")

(def initial-vm-state
  {:return-value nil
   :ns {}})

(defn truthy? [value]
  (boolean value))

(defn eval-expression [vm-state expression]
  (if (vector? expression)
    (case (first expression)
      :def
      (let [[_def name value] expression]
        (-> vm-state
            (update :ns assoc name value)
            (assoc :return-value value)))

      :if
      (let [[_if condition then else] expression]
        (recur
         vm-state
         (if (truthy? (:return-value (eval-expression vm-state condition)))
           then
           else)))

      :symbol
      (let [[_symbol name] expression]
        (assoc vm-state :return-value (-> vm-state
                                          :ns
                                          (get name nil)))))

    (assoc vm-state :return-value expression)))

(defn eval-ast [vm-state ast]
  (reduce eval-expression vm-state ast))
