/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: eel-alao <eel-alao@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/26 18:36:19 by hamrachi          #+#    #+#             */
/*   Updated: 2025/11/17 16:14:24 by eel-alao         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <stdexcept>
#include <csignal>

// bool sig_atomic_t g_terminate = false;

// static void handle_sig(int) 
// {
//     g_terminate = 1;
// }

static int toInt(char *s)
{
	int i = 0;
	int val = 0;

	// Check if port string is empty
	if (s[0] == '\0')
	{
		throw std::runtime_error("Port is empty");
	}

	// Convert string to integer digit by digit
	while (s[i] != '\0')
	{
		// Check if character is a digit
		if (s[i] < '0' || s[i] > '9')
		{
			throw std::runtime_error("Port must be numeric");
		}

		// Build the integer value
		val = (val * 10) + (s[i] - '0');
		i = i + 1;
	}

	return val;
}

int main(int ac, char **av)
{
	try
	{
		// Check argument count
		if (ac != 3)
		{
			std::cerr << "Usage: ./ircserv <port> <password>\n";
			return 1;
		}

		// Parse and validate port
		int port = toInt(av[1]);
		if (port < 1024)
		{
			throw std::runtime_error("Port must be >= 1024");
		}
		if (port > 65535)
		{
			throw std::runtime_error("Port must be <= 65535");
		}

		// Get and validate password
		std::string password = av[2];
		if (password.size() == 0)
		{
			throw std::runtime_error("Password cannot be empty");
		}

		// Create and start server
		Server srv("ft_irc", port, password);

		// Install signal handlers for graceful shutdown
		// std::signal(SIGINT, handle_sig);
		// std::signal(SIGTERM, handle_sig);

		// Initialize socket and run server
		srv.initSocket();
		srv.run();

		// Cleanup on termination
		// if (g_terminate)
		// {
		// 	srv.stop();
		// }

		return 0;
	}
	catch (const std::exception& e)
	{
		std::cerr << "Fatal: " << e.what() << "\n";
		return 2;
	}
	catch (...)
	{
		std::cerr << "Fatal: unknown error\n";
		return 2;
	}
}
