# literal_remover

Experimental fork of ZetaSQL's ReplaceLiteralsByParameters without analyzer. It doesn't require catalog.

## Usage

```sh
$ cat example.sql
SELECT *, 0, 0.0, TRUE, FALSE, "", b"", NUMERIC "0", TIMESTAMP "1970-01-01T00:00:00Z", DATE "1970-01-01",
       STRUCT(1), STRUCT(CURRENT_DATE()), -- a struct constructor containing only literals is a literal
       [1], [CURRENT_DATE()],             -- a array constructor containing only literals is a literal
FROM Singers AS s JOIN@{FORCE_JOIN_ORDER=TRUE} Albums AS a
ON s.SingerId = a.Singerid
WHERE s.LastName LIKE '%x%' AND a.AlbumTitle LIKE '%love%';

$ bazel run --ui_event_filters=stdout --noshow_progress :main < example.sql
SELECT *, @_p0_INT64, @_p1_FLOAT64, @_p2_BOOL, @_p3_BOOL, @_p4_STRING, @_p5_BYTES, @_p6_NUMERIC, @_p7_TIMESTAMP, @_p8_DATE,
       @_p9_STRUCT, STRUCT(CURRENT_DATE()), -- a struct constructor containing only literals is a literal
       @_p10_ARRAY, [CURRENT_DATE()],             -- a array constructor containing only literals is a literal
FROM Singers AS s JOIN@{FORCE_JOIN_ORDER=TRUE} Albums AS a
ON s.SingerId = a.Singerid
WHERE s.LastName LIKE @_p11_STRING AND a.AlbumTitle LIKE @_p12_STRING;
```

## References

- https://github.com/google/zetasql
