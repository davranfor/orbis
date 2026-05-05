(object
    (property "name" (string))
    (property "age" (integer (min 0) (max 120)))
    (property "address"
        (object
            (property "street" (string (maxLength 50)))
            (property "city" (string))
            (property "state" (string))
            (property "postalCode" (string (pattern "\\d{5}")))
        )
    )
    (property "hobbies"
        (array
            (item (string))
        )
    )
)

