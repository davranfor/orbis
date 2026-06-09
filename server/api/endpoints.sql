-- @path
GET /api/session
-- @eval
(any)
-- @stmt
SELECT 1 FROM users WHERE id = ? AND role = ? AND token = ?;

-- @path
POST /api/login
-- @eval
(object
  (property "path")
  (property "params" (null))
  (property "content"
    (object
      (property "email" (string (minLength 3) (format "email")))
      (property "password" (string (minLength 8) (maxLength 20)))
    )
  )
  (property "session"
    (object
      (property "role" (integer (enum 0 1 2 3)))
      (etc)
    )
  )
)
-- @stmt
UPDATE users SET token = new_token(id, role, token, $SESSION)
WHERE email = :email AND password = :password;

-- @path
POST /api/logout
-- @eval
(object
  (property "path")
  (property "params" (null))
  (property "content" (null))
  (property "session"
    (object
      (property "role" (integer (enum 1 2 3)))
      (etc)
    )
  )
)
-- @stmt
UPDATE users SET token = '' WHERE id = $USER;

-- @path
GET /api/users
-- @eval
(object
  (property "path")
  (property "params" (null))
  (property "content" (null))
  (property "session"
    (object
      (property "role" (integer (enum 1 2 3)))
      (etc)
    )
  )
)
-- @stmt
SELECT json_group_array(json_array(id, name, email)) FROM users;

-- @path
GET /api/users
-- @eval
(object
  (property "path")
  (property "params"
    (object
      (property "id" (string (minLength 1) (mask "099999")))
    )
  )
  (property "content" (null))
  (property "session"
    (object
      (property "role" (integer (enum 1 2 3)))
      (etc)
    )
  )
)
-- @stmt
SELECT json_array(id, name, email) FROM users WHERE id = @id;

-- @path
POST /api/users
-- @eval
(object
  (property "path")
  (property "params" (null))
  (property "content"
    (object
      (property "id" (integer (min 1) (max 999999)))
      (property "role" (integer (min 1) (max 3)))
      (property "name" (string (minLength 1) (maxLength 50)))
      (property "email" (string (minLength 3) (format "email")))
      (property "password" (string (minLength 8) (maxLength 20)))
    )
  )
  (property "session"
    (object
      (property "role" (integer (enum 1 2 3)))
      (etc)
    )
  )
)
-- @stmt
INSERT INTO users (id, role, name, email, password)
VALUES (:id, :role, :name, :email, :password);

-- @path
PATCH /api/users
-- @eval
(object
  (property "path")
  (property "params"
    (object
      (property "id" (string (minLength 1) (mask "099999")))
    )
  )
  (property "content"
    (object
      (property "role" (integer (min 1) (max 3)))
      (property "name" (string (minLength 1) (maxLength 50)))
      (property "email" (string (minLength 3) (format "email")))
    )
  )
  (property "session"
    (object
      (property "role" (integer (enum 1 2 3)))
      (etc)
    )
  )
)
-- @stmt
UPDATE users SET role = :role, name = :name, email = :email WHERE id = @id;    

-- @path
DELETE /api/users
-- @eval
(object
  (property "path")
  (property "params"
    (object
      (property "id" (string (minLength 1) (mask "099999")))
    )
  )
  (property "content" (null))
  (property "session"
    (object
      (property "role" (integer (const 1)))
      (etc)
    )
  )
)
-- @stmt
DELETE FROM users WHERE id = @id;    

-- @path
POST /api/exec
-- @eval
(object
  (property "path")
  (property "params" (null))
  (property "content" (string (minLength 1) (maxLength 4096)))
  (property "session"
    (object
      (property "role" (integer (const 1)))
      (etc)
    )
  )
)
-- @stmt
none

-- @path
POST /api/backup
-- @eval
(object
  (property "path")
  (property "params" (null))
  (property "content" (null))
  (property "session"
    (object
      (property "role" (integer (const 1)))
      (etc)
    )
  )
)
-- @stmt
none

-- @path
POST /api/vacuum
-- @eval
(object
  (property "path")
  (property "params" (null))
  (property "content" (null))
  (property "session"
    (object
      (property "role" (integer (const 1)))
      (etc)
    )
  )
)
-- @stmt
none

-- @path
POST /api/reload
-- @eval
(object
  (property "path")
  (property "params" (null))
  (property "content" (null))
  (property "session"
    (object
      (property "role" (integer (const 1)))
      (etc)
    )
  )
)
-- @stmt
none

-- @path
POST /api/stop
-- @eval
(object
  (property "path")
  (property "params" (null))
  (property "content" (null))
  (property "session"
    (object
      (property "role" (integer (const 1)))
      (etc)
    )
  )
)
-- @stmt
none

