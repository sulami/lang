(ns parser
  "Converts a string into a nested vector, vaguely resembling an
  AST.")

(def initial-parser-state
  {:depth 0
   :inside-word? false
   :ast []
   :unparsed ""})

(defn ->initial-state [string]
  (assoc initial-parser-state :unparsed string))

(defn successful-parse? [state]
  (and (= 0 (:depth state))
       (empty? (:unparsed state))))

(defn parse [state]
  (if-let [c (first (:unparsed state))]
    (cond
      (= \( c)
      (let [inner-state (parse (-> state
                                   (update :depth inc)
                                   (update :unparsed rest)
                                   (assoc :ast [])))]
        (recur
         (update inner-state :ast #(conj (:ast state) %))))

      (= \) c)
      (-> state
          (update :depth dec)
          (assoc :inside-word? false)
          (update :unparsed rest))

      (Character/isWhitespace c)
      (recur
       (-> state
           (assoc :inside-word? false)
           (update :unparsed rest)))

      :else
      (recur
       (if (:inside-word? state)
         (-> state
             (update-in [:ast (-> state :ast count dec)] str c)
             (update :unparsed rest))
         (-> state
             (update :ast #(conj % (str c)))
             (assoc :inside-word? true)
             (update :unparsed rest)))))

    state))
