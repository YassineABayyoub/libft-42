*This project has been created as part of the 42 curriculum by yabouayy.*

# Libft

## Description

**Libft** is the first project of the 42 common core. The goal is to build, from scratch, a static C library (`libft.a`) that re-implements a set of standard `libc` functions, adds a second set of useful utility functions, and (as a bonus) provides a small linked-list API.

The library is meant to be reused in every later 42 project, so it has to be:

- **Correct**: each `libc` clone behaves like the original, including edge cases.
- **Safe**: no crashes, no leaks, no undefined behaviour on valid input.
- **Norm-compliant**: written according to the 42 Norm, with no forbidden functions (only `malloc`, `free` and `write` are allowed beyond the functions being re-implemented).

The project is split in three parts:

1. **Part 1 - Libc functions**: re-implementations of standard functions (`ft_strlen`, `ft_memcpy`, `ft_atoi`, ...).
2. **Part 2 - Additional functions**: functions that are not in `libc`, or are available only in a different form (`ft_substr`, `ft_split`, `ft_itoa`, ...).
3. **Bonus - Linked lists**: a `t_list` structure and the functions to create, modify, iterate over and free a singly linked list.

## Instructions

### Requirements

- A C compiler (`cc`), `make` and `ar`.
- The code is compiled with `-Wall -Wextra -Werror`.

### Compilation

```bash
make          # builds libft.a (mandatory part)
make bonus    # builds libft.a with the linked-list functions added
make clean    # removes the object files
make fclean   # removes the object files and libft.a
make re       # fclean + all
```

The Makefile does not relink: running `make` or `make bonus` twice in a row does nothing the second time.

### Usage

Include the header in your code and link against the library:

```c
#include "libft.h"

int main(void)
{
	char	*s = ft_strjoin("Hello, ", "42!");

	ft_putendl_fd(s, 1);
	free(s);
	return (0);
}
```

```bash
cc -Wall -Wextra -Werror main.c -L. -lft -o demo
./demo
```

### Project structure

```
.
├── Makefile
├── README.md
├── libft.h        # prototypes, t_list definition
└── ft_*.c         # one function per file
```

## Library description

All functions are declared in `libft.h`, which also includes `<stddef.h>`, `<stdlib.h>` and `<unistd.h>`.

### Part 1 - Libc functions

**Character checks and conversion**

| Function | Description |
| --- | --- |
| `int ft_isalpha(int c)` | Returns non-zero if `c` is a letter (`A-Z`, `a-z`), 0 otherwise. |
| `int ft_isdigit(int c)` | Returns non-zero if `c` is a decimal digit (`0-9`). |
| `int ft_isalnum(int c)` | Returns non-zero if `c` is a letter or a digit. |
| `int ft_isascii(int c)` | Returns non-zero if `c` fits in the ASCII table (0-127). |
| `int ft_isprint(int c)` | Returns non-zero if `c` is a printable character (32-126). |
| `int ft_toupper(int c)` | Converts a lowercase letter to uppercase, leaves other characters unchanged. |
| `int ft_tolower(int c)` | Converts an uppercase letter to lowercase, leaves other characters unchanged. |

**Memory functions**

| Function | Description |
| --- | --- |
| `void *ft_memset(void *s, int c, size_t n)` | Fills the first `n` bytes of `s` with the byte `c`. |
| `void ft_bzero(void *dest, size_t n)` | Sets the first `n` bytes of `dest` to zero. |
| `void *ft_memcpy(void *s1, const void *s2, size_t n)` | Copies `n` bytes from `s2` to `s1`. The areas must not overlap. |
| `void *ft_memmove(void *dest, const void *src, size_t n)` | Copies `n` bytes from `src` to `dest`, handling overlapping areas correctly. |
| `void *ft_memchr(const void *s, int c, size_t n)` | Returns a pointer to the first occurrence of byte `c` in the first `n` bytes of `s`, or `NULL`. |
| `int ft_memcmp(const void *s1, const void *s2, size_t n)` | Compares the first `n` bytes of two memory areas; returns the difference of the first mismatching bytes (as `unsigned char`). |
| `void *ft_calloc(size_t count, size_t size)` | Allocates `count * size` bytes, zero-initialised. Returns `NULL` on allocation failure or overflow. |

**String functions**

| Function | Description |
| --- | --- |
| `size_t ft_strlen(const char *str)` | Returns the length of `str`, without the terminating `'\0'`. |
| `size_t ft_strlcpy(char *dest, const char *src, size_t size)` | Copies `src` into `dest` (at most `size - 1` characters), always null-terminates when `size > 0`. Returns the length of `src`. |
| `size_t ft_strlcat(char *dest, const char *src, size_t size)` | Appends `src` to `dest`, never writing beyond `size` bytes in total, and null-terminates. Returns the length it tried to create. |
| `char *ft_strchr(const char *s, int c)` | Returns a pointer to the first occurrence of `c` in `s` (the terminating `'\0'` counts), or `NULL`. |
| `char *ft_strrchr(const char *s, int c)` | Same as `ft_strchr`, but for the last occurrence. |
| `int ft_strncmp(const char *s1, const char *s2, size_t n)` | Compares at most `n` characters of two strings. |
| `char *ft_strnstr(const char *haystack, const char *needle, size_t len)` | Locates the first occurrence of `needle` in `haystack`, searching at most `len` characters. An empty `needle` returns `haystack`. |
| `int ft_atoi(const char *str)` | Converts the beginning of `str` to an `int`: skips whitespace, handles one optional sign, then reads digits. |
| `char *ft_strdup(const char *s1)` | Returns a newly allocated copy of `s1`. |

### Part 2 - Additional functions

| Function | Description |
| --- | --- |
| `char *ft_substr(char const *s, unsigned int start, size_t len)` | Allocates and returns a substring of `s` starting at index `start`, of at most `len` characters. |
| `char *ft_strjoin(char const *s1, char const *s2)` | Allocates and returns a new string that is the concatenation of `s1` and `s2`. |
| `char *ft_strtrim(char const *s1, char const *set)` | Allocates and returns a copy of `s1` with all characters in `set` removed from the beginning and the end. |
| `char **ft_split(char const *s, char c)` | Splits `s` using the delimiter `c`. Returns a `NULL`-terminated array of newly allocated strings, or `NULL` on failure (everything already allocated is freed). |
| `char *ft_itoa(int n)` | Allocates and returns the string representation of the integer `n`, including negative numbers and `INT_MIN`. |
| `char *ft_strmapi(char const *s, char (*f)(unsigned int, char))` | Applies `f` to each character of `s` (with its index) and returns a new string made of the results. |
| `void ft_striteri(char *s, void (*f)(unsigned int, char *))` | Applies `f` to each character of `s` (with its index), modifying the string in place. |
| `void ft_putchar_fd(char c, int fd)` | Writes the character `c` to the file descriptor `fd`. |
| `void ft_putstr_fd(char *s, int fd)` | Writes the string `s` to the file descriptor `fd`. |
| `void ft_putendl_fd(char *s, int fd)` | Writes the string `s` followed by a newline to the file descriptor `fd`. |
| `void ft_putnbr_fd(int n, int fd)` | Writes the integer `n` to the file descriptor `fd`. |

### Bonus - Linked lists

The list is built from the following structure:

```c
typedef struct s_list
{
	void			*content;
	struct s_list	*next;
}	t_list;
```

| Function | Description |
| --- | --- |
| `t_list *ft_lstnew(void *content)` | Allocates a new node holding `content`, with `next` set to `NULL`. |
| `void ft_lstadd_front(t_list **lst, t_list *new)` | Adds the node `new` at the beginning of the list. |
| `unsigned int ft_lstsize(t_list *lst)` | Returns the number of nodes in the list. |
| `t_list *ft_lstlast(t_list *lst)` | Returns the last node of the list. |
| `void ft_lstadd_back(t_list **lst, t_list *new)` | Adds the node `new` at the end of the list. |
| `void ft_lstdelone(t_list *lst, void (*del)(void *))` | Frees one node: calls `del` on its content, then frees the node itself. |
| `void ft_lstclear(t_list **lst, void (*del)(void *))` | Deletes and frees the given node and all the nodes after it, then sets the pointer to `NULL`. |
| `void ft_lstiter(t_list *lst, void (*f)(void *))` | Applies `f` to the content of every node. |
| `t_list *ft_lstmap(t_list *lst, void *(*f)(void *), void (*del)(void *))` | Creates a new list by applying `f` to the content of each node; uses `del` to clean up if an allocation fails. |

## Resources

### References

- `man 3` pages of the re-implemented functions (`man 3 strlcpy`, `man 3 memmove`, `man 3 atoi`, ...).
- [cppreference - C standard library](https://en.cppreference.com/w/c) for the behaviour of the standard functions.
- [GNU Make manual](https://www.gnu.org/software/make/manual/make.html) for writing the Makefile (rules, pattern rules, `.PHONY`).
- Kernighan & Ritchie, *The C Programming Language*.
- The 42 Norm and the Libft subject (42 intranet).

### Use of AI

AI (Claude) was used for the following tasks:

- Writing the `Makefile` and this `README.md`.
- Explaining the expected behaviour of the `libc` functions and their edge cases while learning.

The C source files were written, tested and reviewed by the author, who is able to explain every function.
