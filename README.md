# literal_remover

Experimental fork of ZetaSQL's ReplaceLiteralsByParameters without analyzer. It doesn't require catalog.

## Usage

```sh
$ cat example.sql
SELECT *
FROM Singers AS s JOIN@{FORCE_JOIN_ORDER=TRUE} Albums AS a
ON s.SingerId = a.Singerid
WHERE s.LastName LIKE '%x%' AND a.AlbumTitle LIKE '%love%';
$ bazel run :main < example.sql
SELECT *
FROM Singers AS s JOIN@{FORCE_JOIN_ORDER=TRUE} Albums AS a
ON s.SingerId = a.Singerid
WHERE s.LastName LIKE @_p0_STRING AND a.AlbumTitle LIKE @_p1_STRING;
```

## References

- https://github.com/google/zetasql
