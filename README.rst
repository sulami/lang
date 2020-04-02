How this works:

```
poetry run lang > out.ll
llc out.ll --filetype=obj
cc out.o
```
