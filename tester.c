/* ************************************************************************** */
/*   libft tester - mandatory part only (no bonus)                            */
/*                                                                            */
/*   Every test case runs in its own process: a segfault, an abort or an      */
/*   infinite loop (3 s timeout) is reported as KO and the run continues.     */
/*                                                                            */
/*   Build (inside your libft folder, after `make`):                          */
/*       cc -Wall -Wextra tester.c -I. -L. -lft -o tester                     */
/*   Run:                                                                     */
/*       ./tester                 all functions                               */
/*       ./tester -ko             only show KO / WARN lines                   */
/*       ./tester ft_split strlen only the named functions                    */
/*       ./tester -nc             no colors                                   */
/*                                                                            */
/*   Memory errors (overflows, invalid free...) are best caught with ASan:    */
/*       cc -g -fsanitize=address,undefined tester.c ft_*.c -I. -o t_asan     */
/*       ./t_asan -ko                                                         */
/*                                                                            */
/*   [OK]   behaves like libc / the subject                                   */
/*   [KO]   wrong result, crash or timeout                                    */
/*   [WARN] not required by the subject (NULL arguments, overflow of atoi,    */
/*          ...): good to handle, but not a failure                           */
/* ************************************************************************** */

#define _GNU_SOURCE
#include "libft.h"
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define MSGSZ 4096
#define TIMEOUT 3
#define MAXF 64

/* ------------------------------------------------------------------------- */
/*  framework                                                                */
/* ------------------------------------------------------------------------- */

typedef struct s_fn
{
	const char	*name;
	int			ok;
	int			ko;
	int			warn;
	int			hdr;
}	t_fn;

static char			*g_msg;
static t_fn			g_f[MAXF];
static int			g_nf;
static t_fn			*g_cur;
static int			g_only_ko;
static int			g_color;
static const char	*g_filter[64];
static int			g_nfilter;

#define COL(code) (g_color ? code : "")
#define C_RED COL("\033[1;31m")
#define C_GRN COL("\033[1;32m")
#define C_YEL COL("\033[1;33m")
#define C_BLD COL("\033[1m")
#define C_RST COL("\033[0m")

#define FAIL(...) do { snprintf(g_msg, MSGSZ, __VA_ARGS__); _exit(1); } while (0)
#define CHECK(cond, ...) do { if (!(cond)) FAIL(__VA_ARGS__); } while (0)

/* A test case is a block: CASE("description") { ... }
** Do NOT use break / continue / return inside a CASE block. */
#define CASE(desc) for (int _c = case_begin(desc, 0); _c; _c = case_end())
#define CASE_OPT(desc) for (int _c = case_begin(desc, 1); _c; _c = case_end())
#define FUNC(name) do { if (!func_begin(name)) return ; } while (0)

static int	wanted(const char *name)
{
	int	i;

	if (!g_nfilter)
		return (1);
	for (i = 0; i < g_nfilter; i++)
	{
		if (!strcmp(g_filter[i], name))
			return (1);
		if (!strncmp(name, "ft_", 3) && !strcmp(g_filter[i], name + 3))
			return (1);
	}
	return (0);
}

static void	print_header(void)
{
	if (g_cur->hdr)
		return ;
	printf("\n%s== %s ==%s\n", C_BLD, g_cur->name, C_RST);
	g_cur->hdr = 1;
}

static int	func_begin(const char *name)
{
	if (!wanted(name) || g_nf >= MAXF)
		return (0);
	g_cur = &g_f[g_nf++];
	memset(g_cur, 0, sizeof(*g_cur));
	g_cur->name = name;
	if (!g_only_ko)
		print_header();
	return (1);
}

static const char	*sig_text(int sig)
{
	static char	buf[96];

	if (sig == SIGSEGV)
		return ("crash: segmentation fault (invalid memory access / NULL deref)");
	if (sig == SIGBUS)
		return ("crash: bus error (invalid memory access)");
	if (sig == SIGFPE)
		return ("crash: floating point exception (division by zero?)");
	if (sig == SIGABRT)
		return ("crash: abort (heap corruption / invalid free detected by libc?)");
	if (sig == SIGALRM)
		return ("timeout after 3 s (infinite loop?)");
	snprintf(buf, sizeof(buf), "killed by signal %d", sig);
	return (buf);
}

static void	report(const char *desc, int st, int optional)
{
	const char	*why;
	int			bad;

	bad = !(WIFEXITED(st) && WEXITSTATUS(st) == 0);
	why = NULL;
	if (bad)
	{
		if (WIFSIGNALED(st))
			why = sig_text(WTERMSIG(st));
		else if (g_msg[0])
			why = g_msg;
		else
			why = "non-zero exit (ASan/UBSan error? see the report above)";
	}
	if (!bad)
		g_cur->ok++;
	else if (optional)
		g_cur->warn++;
	else
		g_cur->ko++;
	if (!bad && g_only_ko)
		return ;
	print_header();
	if (!bad)
		printf("  %s[OK]%s   %s\n", C_GRN, C_RST, desc);
	else if (optional)
		printf("  %s[WARN]%s %s\n         -> %s\n", C_YEL, C_RST, desc, why);
	else
		printf("  %s[KO]%s   %s\n         -> %s\n", C_RED, C_RST, desc, why);
}

static int	case_begin(const char *desc, int optional)
{
	pid_t	pid;
	int		st;

	g_msg[0] = '\0';
	fflush(stdout);
	pid = fork();
	if (pid < 0)
	{
		perror("fork");
		exit(1);
	}
	if (pid == 0)
	{
		alarm(TIMEOUT);
		return (1);
	}
	waitpid(pid, &st, 0);
	report(desc, st, optional);
	return (0);
}

static int	case_end(void)
{
	_exit(0);
	return (0);
}

/* ------------------------------------------------------------------------- */
/*  helpers                                                                  */
/* ------------------------------------------------------------------------- */

static const char	*shown(const char *s, size_t n)
{
	static char		ring[6][400];
	static unsigned	k;
	char			*o;
	size_t			i;
	size_t			w;
	unsigned char	c;

	o = ring[k++ % 6];
	w = 0;
	o[w++] = '"';
	for (i = 0; i < n && w < 330; i++)
	{
		c = (unsigned char)s[i];
		if (c == '\n')
		{
			o[w++] = '\\';
			o[w++] = 'n';
		}
		else if (c == '\t')
		{
			o[w++] = '\\';
			o[w++] = 't';
		}
		else if (c == '"' || c == '\\')
		{
			o[w++] = '\\';
			o[w++] = (char)c;
		}
		else if (c < 32 || c > 126)
			w += snprintf(o + w, 8, "\\x%02x", c);
		else
			o[w++] = (char)c;
	}
	if (i < n)
	{
		memcpy(o + w, "...", 3);
		w += 3;
	}
	o[w++] = '"';
	o[w] = '\0';
	return (o);
}

static const char	*show(const char *s)
{
	if (!s)
		return ("NULL");
	return (shown(s, strlen(s)));
}

static const char	*chr(int c)
{
	static char		ring[4][8];
	static unsigned	k;
	char			*o;

	o = ring[k++ % 4];
	if (c >= 33 && c <= 126)
		snprintf(o, 8, "%c", c);
	else if (c == ' ')
		snprintf(o, 8, "space");
	else
		snprintf(o, 8, "\\x%02x", (unsigned)c & 0xff);
	return (o);
}

static const char	*pdesc(const char *base, const void *p)
{
	static char		ring[4][32];
	static unsigned	k;
	char			*o;

	if (!p)
		return ("NULL");
	o = ring[k++ % 4];
	snprintf(o, 32, "s+%ld", (long)((const char *)p - base));
	return (o);
}

static int	sgn(int x)
{
	return ((x > 0) - (x < 0));
}

static unsigned char	pat(size_t i)
{
	return ((unsigned char)(i * 7 + 3));
}

static void	fill_pat(unsigned char *b, size_t n)
{
	size_t	i;

	for (i = 0; i < n; i++)
		b[i] = pat(i);
}

static char	*mk(size_t n, char c)
{
	char	*s;

	s = malloc(n + 1);
	if (!s)
		FAIL("tester: malloc failed");
	memset(s, c, n);
	s[n] = '\0';
	return (s);
}

static void	expect_same(const char *call, const void *got, const void *exp, size_t n)
{
	const unsigned char	*a = got;
	const unsigned char	*b = exp;
	size_t				i;

	for (i = 0; i < n; i++)
		if (a[i] != b[i])
			FAIL("%s: byte %zu is 0x%02x ('%s'), expected 0x%02x ('%s')",
				call, i, a[i], chr(a[i]), b[i], chr(b[i]));
}

/* ------------------------------------------------------------------------- */
/*  reference implementations (BSD semantics) for functions not in glibc     */
/* ------------------------------------------------------------------------- */

static size_t	ref_strlcpy(char *dst, const char *src, size_t siz)
{
	const char	*osrc = src;
	size_t		nleft = siz;

	if (nleft != 0)
	{
		while (--nleft != 0)
			if ((*dst++ = *src++) == '\0')
				break ;
	}
	if (nleft == 0)
	{
		if (siz != 0)
			*dst = '\0';
		while (*src++)
			;
	}
	return ((size_t)(src - osrc - 1));
}

static size_t	ref_strlcat(char *dst, const char *src, size_t siz)
{
	char		*d = dst;
	const char	*s = src;
	size_t		n = siz;
	size_t		dlen;

	while (n-- != 0 && *d != '\0')
		d++;
	dlen = (size_t)(d - dst);
	n = siz - dlen;
	if (n == 0)
		return (dlen + strlen(s));
	while (*s != '\0')
	{
		if (n != 1)
		{
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';
	return (dlen + (size_t)(s - src));
}

static char	*ref_strnstr(const char *h, const char *n, size_t len)
{
	size_t	nl;
	size_t	i;

	nl = strlen(n);
	if (!nl)
		return ((char *)h);
	for (i = 0; h[i] && i + nl <= len; i++)
		if (!strncmp(h + i, n, nl))
			return ((char *)h + i);
	return (NULL);
}

static char	*ref_substr(const char *s, unsigned int start, size_t len)
{
	size_t	sl;
	size_t	n;
	char	*r;

	sl = strlen(s);
	n = 0;
	if (start < sl)
	{
		n = sl - start;
		if (len < n)
			n = len;
	}
	r = malloc(n + 1);
	if (n)
		memcpy(r, s + start, n);
	r[n] = '\0';
	return (r);
}

static char	*ref_strtrim(const char *s, const char *set)
{
	size_t	st;
	size_t	en;
	char	*r;

	st = 0;
	en = strlen(s);
	while (s[st] && strchr(set, s[st]))
		st++;
	while (en > st && strchr(set, s[en - 1]))
		en--;
	r = malloc(en - st + 1);
	memcpy(r, s + st, en - st);
	r[en - st] = '\0';
	return (r);
}

/* ------------------------------------------------------------------------- */
/*  prototypes                                                               */
/* ------------------------------------------------------------------------- */

#define PROTO(call, type, label) \
	CASE(label) CHECK(_Generic((call), type : 1, default : 0), \
		"wrong return type in libft.h, the subject expects: " label)

static void	test_prototypes(void)
{
	FUNC("prototypes (return types in libft.h)");
	PROTO(ft_isalpha(0), int, "int ft_isalpha(int)");
	PROTO(ft_isdigit(0), int, "int ft_isdigit(int)");
	PROTO(ft_isalnum(0), int, "int ft_isalnum(int)");
	PROTO(ft_isascii(0), int, "int ft_isascii(int)");
	PROTO(ft_isprint(0), int, "int ft_isprint(int)");
	PROTO(ft_toupper(0), int, "int ft_toupper(int)");
	PROTO(ft_tolower(0), int, "int ft_tolower(int)");
	PROTO(ft_strlen(0), size_t, "size_t ft_strlen(const char *)");
	PROTO(ft_strlcpy(0, 0, 0), size_t, "size_t ft_strlcpy(...)");
	PROTO(ft_strlcat(0, 0, 0), size_t, "size_t ft_strlcat(...)");
	PROTO(ft_memset(0, 0, 0), void *, "void *ft_memset(...)");
	PROTO(ft_memcpy(0, 0, 0), void *, "void *ft_memcpy(...)");
	PROTO(ft_memmove(0, 0, 0), void *, "void *ft_memmove(...)");
	PROTO(ft_memchr(0, 0, 0), void *, "void *ft_memchr(...)  (libc returns void *)");
	PROTO(ft_memcmp(0, 0, 0), int, "int ft_memcmp(...)");
	PROTO(ft_calloc(0, 0), void *, "void *ft_calloc(size_t, size_t)");
	PROTO(ft_strchr(0, 0), char *, "char *ft_strchr(...)");
	PROTO(ft_strrchr(0, 0), char *, "char *ft_strrchr(...)");
	PROTO(ft_strnstr(0, 0, 0), char *, "char *ft_strnstr(...)");
	PROTO(ft_strncmp(0, 0, 0), int, "int ft_strncmp(...)");
	PROTO(ft_atoi(0), int, "int ft_atoi(const char *)");
	PROTO(ft_strdup(0), char *, "char *ft_strdup(const char *)");
	PROTO(ft_substr(0, 0, 0), char *, "char *ft_substr(...)");
	PROTO(ft_strjoin(0, 0), char *, "char *ft_strjoin(...)");
	PROTO(ft_strtrim(0, 0), char *, "char *ft_strtrim(...)");
	PROTO(ft_split(0, 0), char **, "char **ft_split(...)");
	PROTO(ft_itoa(0), char *, "char *ft_itoa(int)");
	PROTO(ft_strmapi(0, 0), char *, "char *ft_strmapi(...)");
}

/* ------------------------------------------------------------------------- */
/*  ctype                                                                    */
/* ------------------------------------------------------------------------- */

#define TEST_IS(NAME) \
static void	test_##NAME(void) \
{ \
	int	c; \
	FUNC("ft_" #NAME); \
	CASE("every value 0..127 (zero / non-zero like libc)") \
	{ \
		for (c = 0; c < 128; c++) \
			CHECK((ft_##NAME(c) != 0) == ((NAME)(c) != 0), \
				"ft_" #NAME "(%d '%s') returned %d, libc returned %d", \
				c, chr(c), ft_##NAME(c), (NAME)(c)); \
	} \
	CASE("every value 128..255 (libc returns 0 in the C locale)") \
	{ \
		for (c = 128; c < 256; c++) \
			CHECK((ft_##NAME(c) != 0) == ((NAME)(c) != 0), \
				"ft_" #NAME "(%d) returned %d, libc returned %d", \
				c, ft_##NAME(c), (NAME)(c)); \
	} \
	CASE("EOF (-1)") \
		CHECK((ft_##NAME(-1) != 0) == ((NAME)(-1) != 0), \
			"ft_" #NAME "(-1) returned %d, libc returned %d", \
			ft_##NAME(-1), (NAME)(-1)); \
}

TEST_IS(isalpha)
TEST_IS(isdigit)
TEST_IS(isalnum)
TEST_IS(isascii)
TEST_IS(isprint)

#define TEST_TO(NAME) \
static void	test_##NAME(void) \
{ \
	int	c; \
	FUNC("ft_" #NAME); \
	CASE("every value 0..127") \
	{ \
		for (c = 0; c < 128; c++) \
			CHECK(ft_##NAME(c) == (NAME)(c), \
				"ft_" #NAME "(%d '%s') returned %d, libc returned %d", \
				c, chr(c), ft_##NAME(c), (NAME)(c)); \
	} \
	CASE("every value 128..255 (must stay unchanged)") \
	{ \
		for (c = 128; c < 256; c++) \
			CHECK(ft_##NAME(c) == (NAME)(c), \
				"ft_" #NAME "(%d) returned %d, libc returned %d", \
				c, ft_##NAME(c), (NAME)(c)); \
	} \
	CASE("EOF (-1)") \
		CHECK(ft_##NAME(-1) == (NAME)(-1), \
			"ft_" #NAME "(-1) returned %d, libc returned %d", \
			ft_##NAME(-1), (NAME)(-1)); \
}

TEST_TO(toupper)
TEST_TO(tolower)

/* ------------------------------------------------------------------------- */
/*  ft_strlen                                                                */
/* ------------------------------------------------------------------------- */

static void	check_len(const char *s)
{
	size_t	g;
	size_t	e;

	g = ft_strlen(s);
	e = strlen(s);
	if (g != e)
		FAIL("ft_strlen(%s) returned %zu, expected %zu", show(s), g, e);
}

static void	test_strlen(void)
{
	char	*big;

	FUNC("ft_strlen");
	CASE("empty string") check_len("");
	CASE("one char") check_len("a");
	CASE("\"hello\"") check_len("hello");
	CASE("string with spaces, tab and newline") check_len("hello world\t!\n");
	CASE("stops at the first \\0 (\"ab\\0cd\" -> 2)") check_len("ab\0cd");
	CASE("non-ASCII bytes (\\xff\\xfe\\xfd)") check_len("\xff\xfe\xfd");
	CASE("100000 chars")
	{
		big = mk(100000, 'x');
		check_len(big);
	}
}

/* ------------------------------------------------------------------------- */
/*  ft_memset / ft_bzero                                                     */
/* ------------------------------------------------------------------------- */

static void	check_memset(size_t off, int c, size_t n, size_t sz)
{
	unsigned char	*a = malloc(sz);
	unsigned char	*b = malloc(sz);
	char			call[96];
	void			*r;

	fill_pat(a, sz);
	fill_pat(b, sz);
	snprintf(call, sizeof(call), "ft_memset(buf+%zu, %d, %zu)", off, c, n);
	r = ft_memset(a + off, c, n);
	memset(b + off, c, n);
	if (r != a + off)
		FAIL("%s did not return its first argument", call);
	expect_same(call, a, b, sz);
}

static void	test_memset(void)
{
	FUNC("ft_memset");
	CASE("fill 5 bytes with 'A'") check_memset(0, 'A', 5, 16);
	CASE("n = 0 (buffer must stay untouched)") check_memset(0, 'A', 0, 16);
	CASE("fill the whole buffer with 0") check_memset(0, 0, 16, 16);
	CASE("c = 'A' + 256 (converted to unsigned char)") check_memset(0, 'A' + 256, 8, 16);
	CASE("c = -1 (becomes 0xFF)") check_memset(0, -1, 8, 16);
	CASE("c = 0x1FF (becomes 0xFF)") check_memset(0, 0x1FF, 8, 16);
	CASE("unaligned start (buf+3), 7 bytes") check_memset(3, 'z', 7, 32);
	CASE("1 byte in the middle (must not touch the neighbours)") check_memset(1, 'q', 1, 3);
	CASE("100000 bytes") check_memset(0, 'x', 100000, 100000);
}

static void	check_bzero(size_t off, size_t n, size_t sz)
{
	unsigned char	*a = malloc(sz);
	unsigned char	*b = malloc(sz);
	char			call[96];

	fill_pat(a, sz);
	fill_pat(b, sz);
	snprintf(call, sizeof(call), "ft_bzero(buf+%zu, %zu)", off, n);
	ft_bzero(a + off, n);
	bzero(b + off, n);
	expect_same(call, a, b, sz);
}

static void	test_bzero(void)
{
	FUNC("ft_bzero");
	CASE("n = 0 (buffer must stay untouched)") check_bzero(0, 0, 16);
	CASE("n = 1") check_bzero(0, 1, 16);
	CASE("n = 5") check_bzero(0, 5, 16);
	CASE("whole buffer") check_bzero(0, 16, 16);
	CASE("unaligned start (buf+3), 7 bytes") check_bzero(3, 7, 32);
	CASE("100000 bytes") check_bzero(0, 100000, 100000);
}

/* ------------------------------------------------------------------------- */
/*  ft_memcpy / ft_memmove                                                   */
/* ------------------------------------------------------------------------- */

static void	check_memcpy(size_t sz, size_t doff, size_t soff, size_t n, int zero)
{
	unsigned char	*src = malloc(sz);
	unsigned char	*a = malloc(sz);
	unsigned char	*b = malloc(sz);
	char			call[96];
	void			*r;

	fill_pat(src, sz);
	if (zero && sz > 3)
		src[2] = 0;
	memset(a, 0xAA, sz);
	memset(b, 0xAA, sz);
	snprintf(call, sizeof(call), "ft_memcpy(dst+%zu, src+%zu, %zu)", doff, soff, n);
	r = ft_memcpy(a + doff, src + soff, n);
	memcpy(b + doff, src + soff, n);
	if (r != a + doff)
		FAIL("%s did not return dst", call);
	expect_same(call, a, b, sz);
}

static void	test_memcpy(void)
{
	FUNC("ft_memcpy");
	CASE("copy 10 bytes") check_memcpy(32, 0, 0, 10, 0);
	CASE("n = 0 (dst must stay untouched)") check_memcpy(32, 0, 0, 0, 0);
	CASE("n = 1") check_memcpy(32, 0, 0, 1, 0);
	CASE("unaligned offsets (dst+3, src+5), 20 bytes") check_memcpy(64, 3, 5, 20, 0);
	CASE("source contains \\0 bytes (must copy them)") check_memcpy(32, 0, 0, 10, 1);
	CASE("100000 bytes") check_memcpy(100000, 0, 0, 100000, 0);
}

static void	check_memmove(size_t sz, size_t doff, size_t soff, size_t n)
{
	unsigned char	*a = malloc(sz);
	unsigned char	*b = malloc(sz);
	char			call[96];
	void			*r;

	fill_pat(a, sz);
	fill_pat(b, sz);
	snprintf(call, sizeof(call), "ft_memmove(buf+%zu, buf+%zu, %zu)", doff, soff, n);
	r = ft_memmove(a + doff, a + soff, n);
	memmove(b + doff, b + soff, n);
	if (r != a + doff)
		FAIL("%s did not return dst", call);
	expect_same(call, a, b, sz);
}

static void	test_memmove(void)
{
	size_t	d;
	size_t	s;

	FUNC("ft_memmove");
	CASE("no overlap (dst far from src)") check_memmove(64, 0, 32, 10);
	CASE("n = 0 (nothing must change)") check_memmove(32, 0, 5, 0);
	CASE("dst == src") check_memmove(32, 4, 4, 10);
	CASE("overlap, dst before src (copy forward)") check_memmove(32, 0, 3, 10);
	CASE("overlap, dst after src (must copy backward)") check_memmove(32, 3, 0, 10);
	CASE("adjacent areas (no real overlap)") check_memmove(32, 0, 10, 10);
	CASE("overlap by only 1 byte, dst before src") check_memmove(32, 0, 9, 10);
	CASE("overlap by only 1 byte, dst after src") check_memmove(32, 9, 0, 10);
	CASE("n = 1 with overlap") check_memmove(8, 1, 0, 1);
	CASE("5000 bytes, dst = src + 1") check_memmove(5100, 1, 0, 5000);
	CASE("5000 bytes, dst = src - 1") check_memmove(5100, 0, 1, 5000);
	CASE("all offset combinations 0..16 with n = 20")
	{
		for (d = 0; d <= 16; d++)
			for (s = 0; s <= 16; s++)
				check_memmove(64, d, s, 20);
	}
}

/* ------------------------------------------------------------------------- */
/*  ft_strlcpy / ft_strlcat                                                  */
/* ------------------------------------------------------------------------- */

#define GUARD 64

/* the bytes right after the buffer must still be 'X' (no overflow) */
static void	guard_check(const char *call, const char *a, size_t bufsz)
{
	size_t	i;

	for (i = bufsz; i < bufsz + GUARD; i++)
		if (a[i] != 'X')
			FAIL("%s wrote past the end of the %zu-byte buffer (byte %zu is 0x%02x)",
				call, bufsz, i, (unsigned char)a[i]);
}

static void	check_strlcpy(const char *src, size_t size, size_t bufsz)
{
	char	*a = malloc(bufsz + GUARD);
	char	b[256];
	char	call[200];
	size_t	ra;
	size_t	rb;

	memset(a, 'X', bufsz + GUARD);
	memset(b, 'X', sizeof(b));
	snprintf(call, sizeof(call), "ft_strlcpy(dst, %s, %zu)", show(src), size);
	ra = ft_strlcpy(a, src, size);
	rb = ref_strlcpy(b, src, size);
	if (ra != rb)
		FAIL("%s returned %zu, expected %zu", call, ra, rb);
	expect_same(call, a, b, bufsz);
	guard_check(call, a, bufsz);
}

static void	test_strlcpy(void)
{
	char	*longsrc;

	FUNC("ft_strlcpy");
	CASE("size larger than src (fits)") check_strlcpy("hello", 20, 64);
	CASE("size == strlen(src) + 1 (exact fit)") check_strlcpy("hello", 6, 64);
	CASE("size == strlen(src) (truncated by one)") check_strlcpy("hello", 5, 64);
	CASE("size smaller than src (truncated)") check_strlcpy("hello world", 4, 64);
	CASE("size == 1 (only room for the \\0)") check_strlcpy("hello", 1, 64);
	CASE("size == 0 (must write NOTHING, return strlen(src))") check_strlcpy("hello", 0, 64);
	CASE("empty src, size 10") check_strlcpy("", 10, 64);
	CASE("empty src, size 1") check_strlcpy("", 1, 64);
	CASE("empty src, size 0") check_strlcpy("", 0, 64);
	CASE("60-char src copied with size 10 (return must be 60)")
	{
		longsrc = mk(60, 'a');
		check_strlcpy(longsrc, 10, 64);
	}
	CASE("100-char src with size 0 into a 16-byte buffer (overflow if size 0 is mishandled)")
	{
		longsrc = mk(100, 'b');
		check_strlcpy(longsrc, 0, 16);
	}
}

static void	check_strlcat(const char *dst, const char *src, size_t size, size_t bufsz)
{
	char	*a = malloc(bufsz + GUARD);
	char	b[256];
	char	call[200];
	size_t	ra;
	size_t	rb;

	memset(a, 'X', bufsz + GUARD);
	memset(b, 'X', sizeof(b));
	strcpy(a, dst);
	strcpy(b, dst);
	snprintf(call, sizeof(call), "ft_strlcat(dst=%s, %s, %zu)", show(dst), show(src), size);
	ra = ft_strlcat(a, src, size);
	rb = ref_strlcat(b, src, size);
	if (ra != rb)
		FAIL("%s returned %zu, expected %zu", call, ra, rb);
	expect_same(call, a, b, bufsz);
	guard_check(call, a, bufsz);
}

static void	test_strlcat(void)
{
	FUNC("ft_strlcat");
	CASE("plenty of room") check_strlcat("abc", "def", 10, 64);
	CASE("exact fit (size == len(dst)+len(src)+1)") check_strlcat("abc", "def", 7, 64);
	CASE("truncated by one (size = 6)") check_strlcat("abc", "def", 6, 64);
	CASE("truncated (size = 5)") check_strlcat("abc", "def", 5, 64);
	CASE("size = len(dst)+1 (no room to append, only the \\0)") check_strlcat("abc", "def", 4, 64);
	CASE("size == len(dst) (returns size + len(src), dst untouched)") check_strlcat("abc", "def", 3, 64);
	CASE("size < len(dst) (returns size + len(src), dst untouched)") check_strlcat("abc", "def", 2, 64);
	CASE("size == 0 (writes nothing, returns len(src))") check_strlcat("abc", "def", 0, 64);
	CASE("empty dst") check_strlcat("", "def", 10, 64);
	CASE("empty dst, size 1") check_strlcat("", "def", 1, 64);
	CASE("empty dst, size 0") check_strlcat("", "def", 0, 64);
	CASE("empty src") check_strlcat("abc", "", 10, 64);
	CASE("empty src, size == len(dst)") check_strlcat("abc", "", 3, 64);
	CASE("both empty, size 1") check_strlcat("", "", 1, 64);
	CASE("both empty, size 0") check_strlcat("", "", 0, 64);
	CASE("longer strings, truncated") check_strlcat("hello ", "world, how are you doing?", 20, 64);
}

/* ------------------------------------------------------------------------- */
/*  ft_strchr / ft_strrchr                                                   */
/* ------------------------------------------------------------------------- */

static void	check_strchr(const char *s, int c)
{
	char	*g;
	char	*e;

	g = ft_strchr(s, c);
	e = strchr(s, c);
	if (g != e)
		FAIL("ft_strchr(%s, %d '%s') returned %s, expected %s",
			show(s), c, chr(c), pdesc(s, g), pdesc(s, e));
}

static void	test_strchr(void)
{
	FUNC("ft_strchr");
	CASE("char at the start") check_strchr("hello", 'h');
	CASE("char in the middle (first occurrence of 'l')") check_strchr("hello", 'l');
	CASE("char at the end") check_strchr("hello", 'o');
	CASE("char not found") check_strchr("hello", 'z');
	CASE("c = '\\0' returns a pointer to the terminator") check_strchr("hello", '\0');
	CASE("empty string, c = '\\0'") check_strchr("", '\0');
	CASE("empty string, c = 'a'") check_strchr("", 'a');
	CASE("c = 'h' + 256 (converted to char)") check_strchr("hello", 'h' + 256);
	CASE("non-ASCII char (255) in the string") check_strchr("a\xff" "b", 255);
	CASE("c = -1 (converted to char 0xFF)") check_strchr("a\xff" "b", -1);
	CASE_OPT("c = 256 (becomes '\\0' after the cast; rarely tested)") check_strchr("abc", 256);
}

static void	check_strrchr(const char *s, int c)
{
	char	*g;
	char	*e;

	g = ft_strrchr(s, c);
	e = strrchr(s, c);
	if (g != e)
		FAIL("ft_strrchr(%s, %d '%s') returned %s, expected %s",
			show(s), c, chr(c), pdesc(s, g), pdesc(s, e));
}

static void	test_strrchr(void)
{
	FUNC("ft_strrchr");
	CASE("last occurrence of 'l'") check_strrchr("hello", 'l');
	CASE("char appears once") check_strrchr("hello", 'h');
	CASE("char at the end") check_strrchr("hello", 'o');
	CASE("char not found") check_strrchr("hello", 'z');
	CASE("c = '\\0' returns a pointer to the terminator") check_strrchr("hello", '\0');
	CASE("empty string, c = '\\0'") check_strrchr("", '\0');
	CASE("empty string, c = 'a'") check_strrchr("", 'a');
	CASE("many occurrences (\"a/b/c/d\", '/')") check_strrchr("a/b/c/d", '/');
	CASE("c = 'l' + 256 (converted to char)") check_strrchr("hello", 'l' + 256);
	CASE("non-ASCII char (255) in the string") check_strrchr("a\xff" "b\xff", 255);
	CASE_OPT("c = 256 (becomes '\\0' after the cast; rarely tested)") check_strrchr("abc", 256);
}

/* ------------------------------------------------------------------------- */
/*  ft_strncmp / ft_memcmp                                                   */
/* ------------------------------------------------------------------------- */

static void	check_strncmp(const char *a, const char *b, size_t n)
{
	int	g;
	int	e;

	g = ft_strncmp(a, b, n);
	e = strncmp(a, b, n);
	if (sgn(g) != sgn(e))
		FAIL("ft_strncmp(%s, %s, %zu) returned %d, libc returned %d (sign must match)",
			show(a), show(b), n, g, e);
}

static void	test_strncmp(void)
{
	FUNC("ft_strncmp");
	CASE("equal strings, n larger than the length") check_strncmp("hello", "hello", 100);
	CASE("n = 0 (always 0, even if strings differ)") check_strncmp("abc", "xyz", 0);
	CASE("differ at the first char") check_strncmp("abc", "xbc", 3);
	CASE("differ at the middle") check_strncmp("abcdef", "abcxef", 6);
	CASE("difference is after n (must return 0)") check_strncmp("abcdef", "abcxef", 3);
	CASE("s1 is a prefix of s2 (n large) -> negative") check_strncmp("abc", "abcd", 10);
	CASE("s2 is a prefix of s1 (n large) -> positive") check_strncmp("abcd", "abc", 10);
	CASE("s1 prefix of s2 but n stops before") check_strncmp("abc", "abcd", 3);
	CASE("both empty") check_strncmp("", "", 5);
	CASE("empty vs non-empty") check_strncmp("", "a", 5);
	CASE("non-empty vs empty") check_strncmp("a", "", 5);
	CASE("non-ASCII: \\xff vs \\x01 (compare as unsigned char -> positive)") check_strncmp("\xff", "\x01", 1);
	CASE("non-ASCII: \\x01 vs \\xff (negative)") check_strncmp("\x01", "\xff", 1);
	CASE("stops at \\0 (\"a\\0b\" vs \"a\\0c\", n = 3 -> 0)") check_strncmp("a\0b", "a\0c", 3);
	CASE("n = 1, same first char") check_strncmp("abc", "axx", 1);
	CASE("n exactly the length") check_strncmp("hello", "hellp", 5);
}

static void	check_memcmp(const void *a, const void *b, size_t n)
{
	int	g;
	int	e;

	g = ft_memcmp(a, b, n);
	e = memcmp(a, b, n);
	if (sgn(g) != sgn(e))
		FAIL("ft_memcmp(%s, %s, %zu) returned %d, libc returned %d (sign must match)",
			shown(a, n), shown(b, n), n, g, e);
}

static void	test_memcmp(void)
{
	FUNC("ft_memcmp");
	CASE("equal") check_memcmp("hello", "hello", 5);
	CASE("n = 0 (always 0)") check_memcmp("abc", "xyz", 0);
	CASE("differ at the first byte") check_memcmp("abc", "xbc", 3);
	CASE("differ at the last byte (s1 < s2)") check_memcmp("abc", "abd", 3);
	CASE("differ at the last byte (s1 > s2)") check_memcmp("abd", "abc", 3);
	CASE("difference after n (must return 0)") check_memcmp("abcdef", "abcxef", 3);
	CASE("embedded \\0 (does NOT stop): \"a\\0b\" vs \"a\\0c\"") check_memcmp("a\0b", "a\0c", 3);
	CASE("non-ASCII: \\xff vs \\x00 (unsigned compare -> positive)") check_memcmp("\xff", "\x00", 1);
	CASE("non-ASCII: \\x00 vs \\xff (negative)") check_memcmp("\x00", "\xff", 1);
	CASE("n = 1") check_memcmp("abc", "axx", 1);
	CASE("1000 equal bytes") check_memcmp(mk(1000, 'k'), mk(1000, 'k'), 1000);
}

/* ------------------------------------------------------------------------- */
/*  ft_memchr                                                                */
/* ------------------------------------------------------------------------- */

static void	check_memchr(const void *s, int c, size_t n)
{
	void	*g;
	void	*e;

	g = ft_memchr(s, c, n);
	e = memchr(s, c, n);
	if (g != e)
		FAIL("ft_memchr(%s, %d '%s', %zu) returned %s, expected %s",
			shown(s, n), c, chr(c), n, pdesc(s, g), pdesc(s, e));
}

static void	test_memchr(void)
{
	FUNC("ft_memchr");
	CASE("found at the start") check_memchr("hello", 'h', 5);
	CASE("found in the middle (first of two)") check_memchr("hello", 'l', 5);
	CASE("found at index n-1") check_memchr("hello", 'o', 5);
	CASE("not found") check_memchr("hello", 'z', 5);
	CASE("n = 0 (must return NULL)") check_memchr("hello", 'h', 0);
	CASE("char exists but only after n") check_memchr("hello", 'o', 4);
	CASE("search for \\0 inside the n bytes") check_memchr("hello", '\0', 6);
	CASE("\\0 is not inside the n bytes") check_memchr("hello", '\0', 5);
	CASE("does not stop at an embedded \\0 (\"ab\\0cd\", 'd')") check_memchr("ab\0cd", 'd', 5);
	CASE("c = 'l' + 256 (converted to unsigned char)") check_memchr("hello", 'l' + 256, 5);
	CASE("c = 255 with a 0xFF byte") check_memchr("a\xff" "b", 255, 3);
	CASE("c = -1 (converted to unsigned char 0xFF)") check_memchr("a\xff" "b", -1, 3);
}

/* ------------------------------------------------------------------------- */
/*  ft_strnstr                                                               */
/* ------------------------------------------------------------------------- */

static void	check_strnstr(const char *h, const char *n, size_t len)
{
	char	*g;
	char	*e;

	g = ft_strnstr(h, n, len);
	e = ref_strnstr(h, n, len);
	if (g != e)
		FAIL("ft_strnstr(%s, %s, %zu) returned %s, expected %s",
			show(h), show(n), len, pdesc(h, g), pdesc(h, e));
}

static void	test_strnstr(void)
{
	FUNC("ft_strnstr");
	CASE("empty needle returns haystack") check_strnstr("hello", "", 5);
	CASE("empty needle, len = 0 (still returns haystack)") check_strnstr("hello", "", 0);
	CASE("empty needle, empty haystack") check_strnstr("", "", 5);
	CASE("needle at the start") check_strnstr("hello world", "hello", 11);
	CASE("needle in the middle") check_strnstr("hello world", "o w", 11);
	CASE("needle at the very end, len covers it exactly") check_strnstr("hello world", "world", 11);
	CASE("needle found but len cuts it (len = 10)") check_strnstr("hello world", "world", 10);
	CASE("needle starts inside len but ends after it") check_strnstr("hello world", "lo wo", 6);
	CASE("needle not present") check_strnstr("hello world", "xyz", 11);
	CASE("needle longer than haystack") check_strnstr("abc", "abcdef", 10);
	CASE("empty haystack, non-empty needle") check_strnstr("", "a", 5);
	CASE("len = 0, non-empty needle (must be NULL)") check_strnstr("hello", "h", 0);
	CASE("false starts before the match (\"aaabc\", \"aab\")") check_strnstr("aaabc", "aab", 5);
	CASE("repeated pattern (\"ababab\", \"abab\")") check_strnstr("ababab", "abab", 6);
	CASE("needle == haystack") check_strnstr("hello", "hello", 5);
	CASE("needle == haystack but len one short") check_strnstr("hello", "hello", 4);
	CASE("huge len (SIZE_MAX) must not read past the \\0") check_strnstr("abc", "c", (size_t)-1);
	CASE("huge len, needle not found") check_strnstr("abc", "d", (size_t)-1);
	CASE("stops at the haystack's \\0 (\"abc\\0def\" / \"def\")") check_strnstr("abc\0def", "def", 7);
	CASE("single char needle") check_strnstr("abcdef", "e", 6);
	CASE("len larger than the haystack") check_strnstr("abc", "bc", 100);
}

/* ------------------------------------------------------------------------- */
/*  ft_atoi                                                                  */
/* ------------------------------------------------------------------------- */

static void	check_atoi(const char *s)
{
	int	g;
	int	e;

	g = ft_atoi(s);
	e = atoi(s);
	if (g != e)
		FAIL("ft_atoi(%s) returned %d, libc atoi returned %d", show(s), g, e);
}

static void	test_atoi(void)
{
	static const char	*ok[] = {"0", "42", "-42", "+42", "   42", "\t\n\v\f\r 42",
		"42abc", "abc", "", "   ", "-", "+", "--1", "+-1", "-+1", "- 1", "+ 1",
		"0042", "-0", "+0", "  -0042xyz", "2147483647", "-2147483648", "12 34",
		"1.5", "\v-5", "0000000000000000000000000042", "-0000000000000000000000000042",
		"123456789", "-123456789", "7\n", "\x01" "5", "5\0" "6", NULL};
	static const char	*ovf[] = {"2147483648", "-2147483649", "4294967296",
		"4294967297", "9223372036854775807", "9223372036854775808",
		"-9223372036854775808", "-9223372036854775809", "99999999999999999999",
		"-99999999999999999999", "18446744073709551616", NULL};
	int					i;

	FUNC("ft_atoi");
	for (i = 0; ok[i]; i++)
		CASE(show(ok[i])) check_atoi(ok[i]);
	for (i = 0; ovf[i]; i++)
		CASE_OPT(show(ovf[i])) check_atoi(ovf[i]);
}

/* ------------------------------------------------------------------------- */
/*  ft_calloc / ft_strdup                                                    */
/* ------------------------------------------------------------------------- */

static void	check_calloc(size_t count, size_t size)
{
	unsigned char	*p;
	size_t			i;
	size_t			tot;

	tot = count * size;
	p = ft_calloc(count, size);
	if (!p && tot)
		FAIL("ft_calloc(%zu, %zu) returned NULL", count, size);
	for (i = 0; p && i < tot; i++)
		if (p[i])
			FAIL("ft_calloc(%zu, %zu): byte %zu is 0x%02x, must be 0", count, size, i, p[i]);
	if (p && tot)
		memset(p, 0xFF, tot);
	free(p);
}

static void	check_calloc_overflow(size_t count, size_t size)
{
	void	*p;

	p = ft_calloc(count, size);
	if (p)
		FAIL("ft_calloc(%zu, %zu): count * size overflows size_t, must return NULL", count, size);
}

static void	test_calloc(void)
{
	FUNC("ft_calloc");
	CASE("5 x 4 bytes, all zero and writable") check_calloc(5, 4);
	CASE("1 x 1") check_calloc(1, 1);
	CASE("1 x 10000") check_calloc(1, 10000);
	CASE("1000 x 8") check_calloc(1000, 8);
	CASE("count = 0 (must not crash; NULL or a freeable pointer)") check_calloc(0, 5);
	CASE("size = 0 (must not crash; NULL or a freeable pointer)") check_calloc(5, 0);
	CASE("count = 0 and size = 0") check_calloc(0, 0);
	CASE("overflow: SIZE_MAX * 2 -> NULL") check_calloc_overflow((size_t)-1, 2);
	CASE("overflow: 2 * SIZE_MAX -> NULL") check_calloc_overflow(2, (size_t)-1);
	CASE("overflow that wraps to 0: (SIZE_MAX/2+1) * 2 -> NULL") check_calloc_overflow((size_t)-1 / 2 + 1, 2);
	CASE("overflow that wraps to 0: 2^32 * 2^32 -> NULL")
		check_calloc_overflow((size_t)1 << 32, (size_t)1 << 32);
}

static void	check_strdup(const char *s)
{
	char	*p;

	p = ft_strdup(s);
	if (!p)
		FAIL("ft_strdup(%s) returned NULL", show(s));
	if (p == s)
		FAIL("ft_strdup(%s) returned the same pointer (must allocate a copy)", show(s));
	if (strcmp(p, s))
		FAIL("ft_strdup(%s) returned %s", show(s), show(p));
	free(p);
}

static void	test_strdup(void)
{
	char	buf[] = "hello";
	char	*p;
	char	*big;

	FUNC("ft_strdup");
	CASE("normal string") check_strdup("hello world");
	CASE("empty string") check_strdup("");
	CASE("one char") check_strdup("a");
	CASE("non-ASCII bytes") check_strdup("\xff\xfe\x80");
	CASE("5000 chars")
	{
		big = mk(5000, 'x');
		check_strdup(big);
	}
	CASE("copy is independent from the original")
	{
		p = ft_strdup(buf);
		CHECK(p != NULL, "ft_strdup returned NULL");
		p[0] = 'X';
		CHECK(buf[0] == 'h', "modifying the copy changed the original");
		CHECK(strcmp(p, "Xello") == 0, "copy content wrong: %s", show(p));
	}
}

/* ------------------------------------------------------------------------- */
/*  ft_substr / ft_strjoin / ft_strtrim                                      */
/* ------------------------------------------------------------------------- */

static void	check_substr(const char *s, unsigned int start, size_t len)
{
	char	*g;
	char	*e;

	g = ft_substr(s, start, len);
	e = ref_substr(s, start, len);
	if (!g)
		FAIL("ft_substr(%s, %u, %zu) returned NULL, expected %s", show(s), start, len, show(e));
	if (strcmp(g, e))
		FAIL("ft_substr(%s, %u, %zu) returned %s, expected %s", show(s), start, len, show(g), show(e));
	free(g);
}

static void	test_substr(void)
{
	FUNC("ft_substr");
	CASE("(\"hello world\", 6, 5) -> \"world\"") check_substr("hello world", 6, 5);
	CASE("start 0, len = whole string") check_substr("hello", 0, 5);
	CASE("start 0, len 3") check_substr("hello", 0, 3);
	CASE("len bigger than what remains -> rest of the string") check_substr("hello world", 6, 100);
	CASE("len = 0 -> empty string (not NULL)") check_substr("hello", 2, 0);
	CASE("start == strlen -> empty string") check_substr("hello", 5, 3);
	CASE("start > strlen -> empty string (not NULL)") check_substr("hello", 50, 3);
	CASE("empty source string") check_substr("", 0, 5);
	CASE("last char only") check_substr("hello", 4, 1);
	CASE("len = SIZE_MAX (len + 1 must not overflow)") check_substr("hello", 1, (size_t)-1);
	CASE("start = UINT_MAX") check_substr("hello", UINT_MAX, 3);
	CASE("start 1, len 1") check_substr("hello", 1, 1);
	CASE_OPT("NULL string (returns NULL, must not crash)")
		CHECK(ft_substr(NULL, 0, 3) == NULL, "ft_substr(NULL, 0, 3) did not return NULL");
}

static void	check_strjoin(const char *s1, const char *s2)
{
	char	*a = strdup(s1);
	char	*b = strdup(s2);
	char	*e = malloc(strlen(s1) + strlen(s2) + 1);
	char	*r;

	strcpy(e, s1);
	strcat(e, s2);
	r = ft_strjoin(a, b);
	if (!r)
		FAIL("ft_strjoin(%s, %s) returned NULL", show(s1), show(s2));
	if (strcmp(r, e))
		FAIL("ft_strjoin(%s, %s) returned %s, expected %s", show(s1), show(s2), show(r), show(e));
	if (strcmp(a, s1) || strcmp(b, s2))
		FAIL("ft_strjoin(%s, %s) modified one of its arguments", show(s1), show(s2));
	if (r == a || r == b)
		FAIL("ft_strjoin must return a new allocation");
	free(r);
}

static void	test_strjoin(void)
{
	char	*a;
	char	*b;

	FUNC("ft_strjoin");
	CASE("(\"hello \", \"world\")") check_strjoin("hello ", "world");
	CASE("s1 empty") check_strjoin("", "world");
	CASE("s2 empty") check_strjoin("hello", "");
	CASE("both empty -> empty string") check_strjoin("", "");
	CASE("one char each") check_strjoin("a", "b");
	CASE("with spaces / newline") check_strjoin("a b\n", " c d");
	CASE("5000 + 5000 chars")
	{
		a = mk(5000, 'a');
		b = mk(5000, 'b');
		check_strjoin(a, b);
	}
	CASE_OPT("NULL s1 (returns NULL, must not crash)")
		CHECK(ft_strjoin(NULL, "a") == NULL, "ft_strjoin(NULL, \"a\") did not return NULL");
	CASE_OPT("NULL s2 (returns NULL, must not crash)")
		CHECK(ft_strjoin("a", NULL) == NULL, "ft_strjoin(\"a\", NULL) did not return NULL");
}

static void	check_strtrim(const char *s1, const char *set)
{
	char	*a = strdup(s1);
	char	*e = ref_strtrim(s1, set);
	char	*r;

	r = ft_strtrim(a, set);
	if (!r)
		FAIL("ft_strtrim(%s, %s) returned NULL, expected %s", show(s1), show(set), show(e));
	if (strcmp(r, e))
		FAIL("ft_strtrim(%s, %s) returned %s, expected %s", show(s1), show(set), show(r), show(e));
	if (strcmp(a, s1))
		FAIL("ft_strtrim modified its first argument");
	free(r);
}

static void	test_strtrim(void)
{
	FUNC("ft_strtrim");
	CASE("(\"  hello  \", \" \")") check_strtrim("  hello  ", " ");
	CASE("nothing to trim") check_strtrim("hello", "xyz");
	CASE("trim only at the start") check_strtrim("xxhello", "x");
	CASE("trim only at the end") check_strtrim("helloxx", "x");
	CASE("several chars in set, mixed order") check_strtrim("xyxhelloyx", "xy");
	CASE("everything is trimmed -> empty string (not NULL)") check_strtrim("aaaa", "a");
	CASE("string made only of set chars, different chars") check_strtrim("abcabc", "abc");
	CASE("empty s1") check_strtrim("", "abc");
	CASE("empty set -> copy of s1") check_strtrim("hello", "");
	CASE("empty s1 and empty set") check_strtrim("", "");
	CASE("set chars inside the string are kept") check_strtrim("  a b  ", " ");
	CASE("set longer than s1") check_strtrim("ab", "abcdefghij");
	CASE("single char string, trimmed") check_strtrim("x", "x");
	CASE("single char string, kept") check_strtrim("x", "y");
	CASE("\\n and \\t in set") check_strtrim("\n\t hello \t\n", "\n\t ");
	CASE_OPT("NULL s1 (returns NULL, must not crash)")
		CHECK(ft_strtrim(NULL, "a") == NULL, "ft_strtrim(NULL, \"a\") did not return NULL");
	CASE_OPT("NULL set (must not crash)")
	{
		char	*r = ft_strtrim("abc", NULL);

		(void)r;
	}
}

/* ------------------------------------------------------------------------- */
/*  ft_split                                                                 */
/* ------------------------------------------------------------------------- */

static void	check_split(const char *s, char c, ...)
{
	va_list		ap;
	const char	*exp[256];
	int			n;
	int			i;
	char		*in;
	char		**r;
	char		call[300];

	n = 0;
	va_start(ap, c);
	while (n < 255 && (exp[n] = va_arg(ap, const char *)) != NULL)
		n++;
	va_end(ap);
	in = strdup(s);
	snprintf(call, sizeof(call), "ft_split(%s, '%s')", show(s), chr((unsigned char)c));
	r = ft_split(in, c);
	if (!r)
		FAIL("%s returned NULL", call);
	for (i = 0; i < n; i++)
	{
		if (!r[i])
			FAIL("%s: only %d word(s) in the array, expected %d (word %d should be %s)",
				call, i, n, i, show(exp[i]));
		if (strcmp(r[i], exp[i]))
			FAIL("%s: word %d is %s, expected %s", call, i, show(r[i]), show(exp[i]));
	}
	if (r[n])
		FAIL("%s: too many words: word %d is %s, expected the NULL terminator",
			call, n, show(r[n]));
	if (strcmp(in, s))
		FAIL("%s modified the input string", call);
	for (i = 0; i < n; i++)
		free(r[i]);
	free(r);
}

static void	check_split_many(int count)
{
	char	*s = malloc((size_t)count * 12 + 1);
	char	w[24];
	char	**r;
	int		i;

	s[0] = '\0';
	for (i = 0; i < count; i++)
	{
		snprintf(w, sizeof(w), "%sw%d", i ? " " : "", i);
		strcat(s, w);
	}
	r = ft_split(s, ' ');
	if (!r)
		FAIL("ft_split on %d words returned NULL", count);
	for (i = 0; i < count; i++)
	{
		snprintf(w, sizeof(w), "w%d", i);
		if (!r[i])
			FAIL("ft_split on %d words: array ends after %d word(s)", count, i);
		if (strcmp(r[i], w))
			FAIL("ft_split on %d words: word %d is %s, expected \"%s\"", count, i, show(r[i]), w);
	}
	if (r[count])
		FAIL("ft_split on %d words: array is not NULL-terminated after the last word", count);
	for (i = 0; i < count; i++)
		free(r[i]);
	free(r);
}

static void	test_split(void)
{
	FUNC("ft_split");
	CASE("(\"hello world\", ' ')") check_split("hello world", ' ', "hello", "world", (char *)NULL);
	CASE("5 words") check_split("this is a test string", ' ', "this", "is", "a", "test", "string", (char *)NULL);
	CASE("leading, trailing and repeated delimiters")
		check_split("   a   b  c  ", ' ', "a", "b", "c", (char *)NULL);
	CASE("no delimiter in the string -> 1 word") check_split("abc", ' ', "abc", (char *)NULL);
	CASE("only delimiters -> empty array (just NULL)") check_split("     ", ' ', (char *)NULL);
	CASE("empty string -> empty array (just NULL)") check_split("", ' ', (char *)NULL);
	CASE("single char word") check_split("a", ' ', "a", (char *)NULL);
	CASE("single delimiter") check_split(" ", ' ', (char *)NULL);
	CASE("word followed by a delimiter") check_split("a ", ' ', "a", (char *)NULL);
	CASE("delimiter followed by a word") check_split(" a", ' ', "a", (char *)NULL);
	CASE("delimiter is ',' (\"a,b,c\")") check_split("a,b,c", ',', "a", "b", "c", (char *)NULL);
	CASE("delimiter is not a space, but the string has spaces")
		check_split("hello world,foo bar", ',', "hello world", "foo bar", (char *)NULL);
	CASE("delimiters at both ends (\",a,\")") check_split(",a,", ',', "a", (char *)NULL);
	CASE("only ',' delimiters") check_split(",,,,", ',', (char *)NULL);
	CASE("long words")
		check_split("abcdefghijklmnopqrstuvwxyz 0123456789abcdefghijklmnopqrstuvwxyz", ' ',
			"abcdefghijklmnopqrstuvwxyz", "0123456789abcdefghijklmnopqrstuvwxyz", (char *)NULL);
	CASE("tab / newline are NOT delimiters when c = ' '")
		check_split("a\tb c\nd", ' ', "a\tb", "c\nd", (char *)NULL);
	CASE("1000 words") check_split_many(1000);
	CASE_OPT("c = '\\0' (whole string is one word; ambiguous in the subject)")
		check_split("lorem ipsum", '\0', "lorem ipsum", (char *)NULL);
	CASE_OPT("NULL string (returns NULL, must not crash)")
		CHECK(ft_split(NULL, ' ') == NULL, "ft_split(NULL, ' ') did not return NULL");
}

/* ------------------------------------------------------------------------- */
/*  ft_itoa                                                                  */
/* ------------------------------------------------------------------------- */

static void	check_itoa(int n)
{
	char	exp[32];
	char	*r;

	snprintf(exp, sizeof(exp), "%d", n);
	r = ft_itoa(n);
	if (!r)
		FAIL("ft_itoa(%d) returned NULL", n);
	if (strcmp(r, exp))
		FAIL("ft_itoa(%d) returned %s, expected \"%s\"", n, show(r), exp);
	free(r);
}

static void	test_itoa(void)
{
	static const int	v[] = {0, 1, -1, 5, 9, 10, -10, 11, 42, -42, 99, 100, -100,
		101, 999, 1000, 12345, -12345, 123456789, -123456789, 1000000000,
		-1000000000, 2147483646, 2147483647, -2147483647, INT_MIN, INT_MIN + 1};
	char				label[32];
	size_t				i;
	int					n;

	FUNC("ft_itoa");
	for (i = 0; i < sizeof(v) / sizeof(v[0]); i++)
	{
		snprintf(label, sizeof(label), "%d", v[i]);
		CASE(label) check_itoa(v[i]);
	}
	CASE("every value from -2000 to 2000")
	{
		for (n = -2000; n <= 2000; n++)
			check_itoa(n);
	}
	CASE("100000 pseudo-random values")
	{
		unsigned int	seed = 12345;

		for (i = 0; i < 100000; i++)
		{
			seed = seed * 1103515245u + 12345u;
			check_itoa((int)seed);
		}
	}
}

/* ------------------------------------------------------------------------- */
/*  ft_strmapi / ft_striteri                                                 */
/* ------------------------------------------------------------------------- */

static unsigned int	g_calls;
static int			g_bad;
static unsigned int	g_badi;
static char			*g_base;

static char	f_upper(unsigned int i, char c)
{
	(void)i;
	return ((char)toupper((unsigned char)c));
}

static char	f_idx(unsigned int i, char c)
{
	(void)c;
	return ((char)('0' + i % 10));
}

static char	f_track(unsigned int i, char c)
{
	if (i != g_calls && !g_bad)
	{
		g_bad = 1;
		g_badi = i;
	}
	g_calls++;
	return (c);
}

static void	test_strmapi(void)
{
	char	buf[] = "hello";
	char	*r;
	char	*long25;
	char	exp[32];
	int		i;

	FUNC("ft_strmapi");
	CASE("toupper on \"hello World\"")
	{
		r = ft_strmapi("hello World", f_upper);
		CHECK(r != NULL, "returned NULL");
		CHECK(!strcmp(r, "HELLO WORLD"), "returned %s, expected \"HELLO WORLD\"", show(r));
	}
	CASE("f uses the index (\"aaaaa\" -> \"01234\")")
	{
		r = ft_strmapi("aaaaa", f_idx);
		CHECK(r != NULL, "returned NULL");
		CHECK(!strcmp(r, "01234"), "returned %s, expected \"01234\"", show(r));
	}
	CASE("index goes past 9 (25 chars)")
	{
		long25 = mk(25, 'a');
		for (i = 0; i < 25; i++)
			exp[i] = (char)('0' + i % 10);
		exp[25] = '\0';
		r = ft_strmapi(long25, f_idx);
		CHECK(r != NULL, "returned NULL");
		CHECK(!strcmp(r, exp), "returned %s, expected %s", show(r), show(exp));
	}
	CASE("empty string -> empty string (not NULL)")
	{
		r = ft_strmapi("", f_upper);
		CHECK(r != NULL, "returned NULL");
		CHECK(r[0] == '\0', "returned %s, expected \"\"", show(r));
	}
	CASE("f called once per char, with indices 0,1,2,... in order")
	{
		g_calls = 0;
		g_bad = 0;
		r = ft_strmapi(buf, f_track);
		CHECK(g_calls == 5, "f was called %u time(s), expected 5", g_calls);
		CHECK(!g_bad, "f was called with index %u when %u was expected", g_badi, g_badi);
		CHECK(r != NULL && !strcmp(r, "hello"), "returned %s", show(r));
	}
	CASE("original string is not modified, result is a new allocation")
	{
		r = ft_strmapi(buf, f_upper);
		CHECK(r != buf, "returned the same pointer as the input");
		CHECK(!strcmp(buf, "hello"), "the original string was modified: %s", show(buf));
	}
	CASE_OPT("NULL string (returns NULL, must not crash)")
		CHECK(ft_strmapi(NULL, f_upper) == NULL, "ft_strmapi(NULL, f) did not return NULL");
	CASE_OPT("NULL function (must not crash)")
	{
		r = ft_strmapi("abc", NULL);
		(void)r;
	}
}

static void	fi_upper(unsigned int i, char *c)
{
	(void)i;
	*c = (char)toupper((unsigned char)*c);
}

static void	fi_idx(unsigned int i, char *c)
{
	*c = (char)('0' + i % 10);
}

static void	fi_track(unsigned int i, char *c)
{
	if (!g_bad && (i != g_calls || c != g_base + i))
	{
		g_bad = 1;
		g_badi = i;
	}
	g_calls++;
}

static void	fi_count(unsigned int i, char *c)
{
	(void)i;
	(void)c;
	g_calls++;
}

static void	test_striteri(void)
{
	char	b1[] = "hello World";
	char	b2[16] = "aaaaaaaaaaaa";
	char	b3[] = "hello";
	char	b4[8] = "abc\0XYZ";
	char	b5[] = "";

	FUNC("ft_striteri");
	CASE("toupper in place on \"hello World\"")
	{
		ft_striteri(b1, fi_upper);
		CHECK(!strcmp(b1, "HELLO WORLD"), "string is now %s, expected \"HELLO WORLD\"", show(b1));
	}
	CASE("f uses the index (12 x 'a' -> \"012345678901\")")
	{
		ft_striteri(b2, fi_idx);
		CHECK(!strcmp(b2, "012345678901"), "string is now %s", show(b2));
	}
	CASE("empty string: f must never be called")
	{
		g_calls = 0;
		ft_striteri(b5, fi_count);
		CHECK(g_calls == 0, "f was called %u time(s) on an empty string", g_calls);
	}
	CASE("f called once per char, index i, with the ADDRESS &s[i]")
	{
		g_calls = 0;
		g_bad = 0;
		g_base = b3;
		ft_striteri(b3, fi_track);
		CHECK(g_calls == 5, "f was called %u time(s), expected 5", g_calls);
		CHECK(!g_bad, "call with index/pointer wrong at index %u (pointer must be &s[i])", g_badi);
	}
	CASE("does not touch bytes after the terminator")
	{
		ft_striteri(b4, fi_upper);
		CHECK(!memcmp(b4, "ABC\0XYZ", 8), "bytes after the \\0 were modified or the string is wrong");
	}
	CASE_OPT("NULL string (must not crash)") ft_striteri(NULL, fi_upper);
	CASE_OPT("NULL function (must not crash)") ft_striteri(b3, NULL);
}

/* ------------------------------------------------------------------------- */
/*  ft_put*_fd (output captured through a pipe)                              */
/* ------------------------------------------------------------------------- */

static int	g_pfd[2];

static void	cap_begin(void)
{
	if (pipe(g_pfd) < 0)
		FAIL("tester: pipe() failed");
}

static size_t	cap_end(char *buf, size_t max)
{
	ssize_t	n;
	size_t	tot;

	close(g_pfd[1]);
	tot = 0;
	while (tot < max && (n = read(g_pfd[0], buf + tot, max - tot)) > 0)
		tot += (size_t)n;
	close(g_pfd[0]);
	return (tot);
}

static void	expect_out(const char *call, const char *exp, size_t explen)
{
	char	buf[20000];
	size_t	n;

	n = cap_end(buf, sizeof(buf));
	if (n != explen || memcmp(buf, exp, explen))
		FAIL("%s wrote %zu byte(s) %s, expected %zu byte(s) %s",
			call, n, shown(buf, n), explen, shown(exp, explen));
}

static void	check_putchar(char c)
{
	char	call[64];

	cap_begin();
	ft_putchar_fd(c, g_pfd[1]);
	snprintf(call, sizeof(call), "ft_putchar_fd('%s')", chr((unsigned char)c));
	expect_out(call, &c, 1);
}

static void	test_putchar_fd(void)
{
	FUNC("ft_putchar_fd");
	CASE("'a'") check_putchar('a');
	CASE("'\\n'") check_putchar('\n');
	CASE("' '") check_putchar(' ');
	CASE("'\\0' (one \\0 byte must be written)") check_putchar('\0');
	CASE("non-ASCII char (0xFF)") check_putchar((char)0xFF);
	CASE("two calls, order is kept ('4' then '2')")
	{
		cap_begin();
		ft_putchar_fd('4', g_pfd[1]);
		ft_putchar_fd('2', g_pfd[1]);
		expect_out("ft_putchar_fd('4') + ft_putchar_fd('2')", "42", 2);
	}
	CASE("invalid fd (-1): must not crash") ft_putchar_fd('a', -1);
}

static void	check_putstr(const char *s, size_t explen)
{
	char	call[300];

	cap_begin();
	ft_putstr_fd((char *)s, g_pfd[1]);
	snprintf(call, sizeof(call), "ft_putstr_fd(%s)", show(s));
	expect_out(call, s, explen);
}

static void	test_putstr_fd(void)
{
	char	*big;

	FUNC("ft_putstr_fd");
	CASE("\"hello\"") check_putstr("hello", 5);
	CASE("empty string (writes nothing)") check_putstr("", 0);
	CASE("string with newlines") check_putstr("a\nb\n", 4);
	CASE("stops at the first \\0 (\"a\\0b\" -> \"a\")") check_putstr("a\0b", 1);
	CASE("non-ASCII bytes") check_putstr("\xc3\xa9\xff", 3);
	CASE("10000 chars")
	{
		big = mk(10000, 'z');
		check_putstr(big, 10000);
	}
	CASE("invalid fd (-1): must not crash") ft_putstr_fd("hello", -1);
	CASE_OPT("NULL string (must not crash)")
	{
		cap_begin();
		ft_putstr_fd(NULL, g_pfd[1]);
	}
}

static void	check_putendl(const char *s)
{
	char	call[300];
	char	*exp;
	size_t	l;

	l = strlen(s);
	exp = malloc(l + 2);
	memcpy(exp, s, l);
	exp[l] = '\n';
	exp[l + 1] = '\0';
	cap_begin();
	ft_putendl_fd((char *)s, g_pfd[1]);
	snprintf(call, sizeof(call), "ft_putendl_fd(%s)", show(s));
	expect_out(call, exp, l + 1);
}

static void	test_putendl_fd(void)
{
	char	*big;

	FUNC("ft_putendl_fd");
	CASE("\"hello\" -> \"hello\\n\"") check_putendl("hello");
	CASE("empty string -> just \"\\n\"") check_putendl("");
	CASE("string that already has a newline") check_putendl("a\nb");
	CASE("10000 chars")
	{
		big = mk(10000, 'z');
		check_putendl(big);
	}
	CASE("invalid fd (-1): must not crash") ft_putendl_fd("hello", -1);
	CASE_OPT("NULL string (must not crash)")
	{
		cap_begin();
		ft_putendl_fd(NULL, g_pfd[1]);
	}
}

static void	check_putnbr(int n)
{
	char	exp[32];
	char	call[64];

	snprintf(exp, sizeof(exp), "%d", n);
	snprintf(call, sizeof(call), "ft_putnbr_fd(%d)", n);
	cap_begin();
	ft_putnbr_fd(n, g_pfd[1]);
	expect_out(call, exp, strlen(exp));
}

static void	test_putnbr_fd(void)
{
	static const int	v[] = {0, 1, -1, 5, 9, 10, -10, 11, 42, -42, 99, 100, -100,
		101, 1000, 12345, -12345, 123456789, 1000000000, 2147483646,
		2147483647, -2147483647, INT_MIN, INT_MIN + 1};
	char				label[32];
	size_t				i;

	FUNC("ft_putnbr_fd");
	for (i = 0; i < sizeof(v) / sizeof(v[0]); i++)
	{
		snprintf(label, sizeof(label), "%d", v[i]);
		CASE(label) check_putnbr(v[i]);
	}
	CASE("invalid fd (-1): must not crash") ft_putnbr_fd(42, -1);
}

/* ------------------------------------------------------------------------- */
/*  main                                                                     */
/* ------------------------------------------------------------------------- */

int	main(int argc, char **argv)
{
	int	i;
	int	ok;
	int	ko;
	int	warn;

	g_color = isatty(1);
	g_msg = mmap(NULL, MSGSZ, PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (g_msg == MAP_FAILED)
		return (perror("mmap"), 1);
	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-ko"))
			g_only_ko = 1;
		else if (!strcmp(argv[i], "-nc"))
			g_color = 0;
		else if (g_nfilter < 64)
			g_filter[g_nfilter++] = argv[i];
	}
	printf("%slibft tester (mandatory part)%s\n", C_BLD, C_RST);
	test_prototypes();
	test_isalpha();
	test_isdigit();
	test_isalnum();
	test_isascii();
	test_isprint();
	test_strlen();
	test_memset();
	test_bzero();
	test_memcpy();
	test_memmove();
	test_strlcpy();
	test_strlcat();
	test_toupper();
	test_tolower();
	test_strchr();
	test_strrchr();
	test_strncmp();
	test_memchr();
	test_memcmp();
	test_strnstr();
	test_atoi();
	test_calloc();
	test_strdup();
	test_substr();
	test_strjoin();
	test_strtrim();
	test_split();
	test_itoa();
	test_strmapi();
	test_striteri();
	test_putchar_fd();
	test_putstr_fd();
	test_putendl_fd();
	test_putnbr_fd();
	printf("\n%s================ SUMMARY ================%s\n", C_BLD, C_RST);
	ok = 0;
	ko = 0;
	warn = 0;
	for (i = 0; i < g_nf; i++)
	{
		printf("%-48s %3d/%-3d", g_f[i].name, g_f[i].ok, g_f[i].ok + g_f[i].ko);
		if (g_f[i].ko)
			printf("  %sKO (%d failed)%s", C_RED, g_f[i].ko, C_RST);
		else
			printf("  %sOK%s", C_GRN, C_RST);
		if (g_f[i].warn)
			printf("  %s(%d warning%s)%s", C_YEL, g_f[i].warn, g_f[i].warn > 1 ? "s" : "", C_RST);
		printf("\n");
		ok += g_f[i].ok;
		ko += g_f[i].ko;
		warn += g_f[i].warn;
	}
	printf("\nTOTAL: %s%d OK%s, %s%d KO%s, %s%d WARN%s\n", C_GRN, ok, C_RST,
		ko ? C_RED : C_GRN, ko, C_RST, C_YEL, warn, C_RST);
	return (ko != 0);
}
