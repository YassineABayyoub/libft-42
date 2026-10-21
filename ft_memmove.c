/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_memmove.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: yabouayy <yabouayy@student.1337.ma>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/22 00:27:49 by yabouayy          #+#    #+#             */
/*   Updated: 2026/10/22 00:35:51 by yabouayy         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

void	*ft_memmove(void *dest, const void *src, size_t n)
{
	unsigned char	*dst;
	unsigned char	*source;

	dst = (unsigned char *)dest;
	source = (unsigned char *)src;
	if (dst == source || n == 0)
		return (dest);
	if (dst < source)
	{
		while (n--)
			*dst++ = *source++;
	}
	else
	{
		dst += n;
		source += n;
		while (n--)
			*--dst = *--source;
	}
	return (dest);
}
