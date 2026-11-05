/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_split.c                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: yabouayy <yabouayy@student.1337.ma>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/25 12:25:17 by yabouayy          #+#    #+#             */
/*   Updated: 2026/11/01 23:14:21 by yabouayy         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

static int	count_words(const char *str, char c)
{
	int	count;
	int	i;

	count = 0;
	i = 0;
	while (str[i])
	{
		if ((i == 0 || str[i - 1] == c) && str[i] != c)
			count++;
		i++;
	}
	return (count);
}

static char	*set_word(const char *str, int c)
{
	char	*dest;
	int		count;

	count = 0;
	while (str[count] && str[count] != c)
		count++;
	dest = malloc(count + 1);
	if (!dest)
		return (NULL);
	ft_strlcpy(dest, str, count + 1);
	return (dest);
}

static void	free_all(char **dest)
{
	int	i;

	i = 0;
	while (dest[i])
	{
		free(dest[i]);
		i++;
	}
	free(dest);
}

char	**ft_split(const char *str, char c)
{
	char	**dest;
	int		i;
	int		j;

	if (!str)
		return (NULL);
	i = 0;
	j = 0;
	dest = malloc((count_words(str, c) + 1) * sizeof(char *));
	if (!dest)
		return (NULL);
	while (str[i])
	{
		if ((i == 0 || str[i - 1] == c) && str[i] != c)
		{
			dest[j] = set_word(str + i, c);
			if (dest[j] == NULL)
				return (free_all(dest), NULL);
			j++;
		}
		i++;
	}
	dest[j] = NULL;
	return (dest);
}
