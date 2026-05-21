(object
    (property "code" (string (minLength 1) (mask "09999")))
    (property "name" (string (minLength 1) (maxLength 50)))
    (property "age" (integer (min 0) (max 120) (const 42)))
    (property "address"
        (object
            (property "street" (string))
            (property "city" (string))
            (property "state" (string (enum "Baleares" "Canarias")))
            (property "postalCode" (string (pattern "^[0-9]{5}$")))
        )
    )
    (property "email" (string (format "email")))
    (property "items" (array (integer) (minItems 3) (maxItems 3) (uniqueItems)))
    (property "tuple" (tuple (number) (any) (null)))
    (property "end" (integer) (nullable))
    (property "extra" (optional))
    (property)
)

