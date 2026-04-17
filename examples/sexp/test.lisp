(object
    (required "name" "age")
    (property "name" (string))
    (property "age" (number (min 0)))
    (property "address"
        (object
            (required "street" "city" "state" "postalCode")
            (property "street" (string))
            (property "city" (string))
            (property "state" (string))
            (property "postalCode" (string (pattern "\\d{5}")))
        )
    )
    (property "hobbies"
        (array
            (items (string))
        )
    )
)

