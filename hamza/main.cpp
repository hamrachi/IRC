/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hamrachi <hamrachi@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/26 18:36:19 by hamrachi          #+#    #+#             */
/*   Updated: 2025/08/31 17:38:13 by hamrachi         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <stdexcept>

static int toInt(char *s)
{
    int i = 0;
    int val = 0;
    if (s[0] == '\0') {
        throw std::runtime_error("Port is empty");
    }
    while (s[i] != '\0') {
        if (s[i] < '0' || s[i] > '9') {
            throw std::runtime_error("Port must be numeric");
        }
        val = (val * 10) + (s[i] - '0');
        i = i + 1;
    }
    return val;
}

int main (int ac , char **av)
{
    try{
        if (ac != 3){
             std::cerr << "Usage: ./ircserv <port> <password>\n";
            return 1;
        }
        int port = toInt(av[1]);
        if (port < 1024){
            throw std::runtime_error("Port must be >= 1024");
        }
        if (port > 65535) {
                throw std::runtime_error("Port must be <= 65535");
            }
         std::string password = av[2];
        if (password.size() == 0) {
            throw std::runtime_error("Password cannot be empty");
        }
        Server srv("ft_irc", port, password);
        srv.initSocket();
        srv.run();
        return 0;
        
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 2;
    } catch (...) {
        std::cerr << "Fatal: unknown error\n";
        return 2;
    }
}
