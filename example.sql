SELECT *, 0, 0.0, TRUE, FALSE, "", b"", NUMERIC "0", TIMESTAMP "1970-01-01T00:00:00Z", DATE "1970-01-01",
       STRUCT(1), STRUCT(CURRENT_DATE()), -- a struct constructor containing only literals is aliteral
       [1], [CURRENT_DATE()],             -- a array constructor containing only literals is a literal
FROM Singers AS s JOIN@{FORCE_JOIN_ORDER=TRUE} Albums AS a
ON s.SingerId = a.Singerid
WHERE s.LastName LIKE '%x%' AND a.AlbumTitle LIKE '%love%';