/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_memcpy.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: yabouayy <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/17 05:59:50 by yabouayy          #+#    #+#             */
/*   Updated: 2026/10/17 06:33:31 by yabouayy         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

void	*ft_memcpy(void *s1, const void *s2, size_t n)
{
	unsigned char		*dest;
	const unsigned char	*src;
	size_t				i;

	dest = (unsigned char *)s1;
	src = (const unsigned char *)s2;
	i = 0;
	if (!dest && !src)
		return (NULL);
	while (i < n)
	{
		dest[i] = src[i];
		i++;
	}
	return (s1);
}
