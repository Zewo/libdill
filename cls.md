# cls(3) manual page

## NAME

cls - get coroutine-local storage

## SYNOPSIS

```c
#include <libdill.h>
void *cls(void);
```

## DESCRIPTION

Returns coroutine-local storage, a single pointer that is accessible from anywhere within the same coroutine.

You can set the coroutine-local storage using `setcls` function. If it was not previously set, `cls` will return `NULL`.

## RETURN VALUE

The coroutine-local storage.

## ERRORS

None.

## EXAMPLE

```c
char *str = "Hello, world!";
setcls(str);
printf("%s\n", (char*)cls());
```
