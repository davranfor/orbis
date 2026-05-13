(object
    (property "name" (string (minLength 1) (maxLength 50)))
    (property "age" (integer (min 0) (max 120)))
    (property "address"
        (object
            (property "street" (string))
            (property "city" (string))
            (property "state" (string))
            (property "postalCode" (string (pattern "^[0-9]{5}$")))
        )
    )
    (property "items" (array (integer) (minItems 3) (maxItems 3)))
    (property "tuple" (tuple (string) (string) (string)))
    (property "end" (null))
)

