/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hamrachi <hamrachi@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/26 18:32:07 by hamrachi          #+#    #+#             */
/*   Updated: 2025/09/09 00:02:16 by hamrachi         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <iostream>
#include <map>
#include "Client.hpp"
#include "Channel.hpp"
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <poll.h>
#include <sys/types.h>

class Client;
class Channel;

class Server{
private:
    int _listenFd;
    std::string _name;
    int _port;
    std::string _passWord;
    std::map <int , Client> _clients;
    std::map<std::string, Channel> _channels;
    std::vector<struct pollfd> pfds;
    struct sockaddr_in _claddr;

public:
    Server(const std::string& name, int port, const std::string& pass);
    void run();
    void initSocket();
    void acceptNewClient(std::vector<struct pollfd>& pfds);
    void receiveFromClient(int clientFd);
    void removeClient(int fd);
    void handleMessage(Client& cli, const IRCMessage& m);
    void sendLine(Client& cli, const std::string& line);
    void sendWelcome(Client& cli);
    bool isValidNick(const std::string& s) const;
    bool isNickTaken(const std::string& s, int exceptFd) const;

    // channel commands
    void handleInvite(Client& requester, const std::string& chanName, const std::string& nickTarget);
    void handleKick(Client& requester, const std::string& chanName, const std::string& nickTarget, const std::string& reason = "");
    void handleMode(Client& requester, const std::string& chanName, const std::string& modes, const std::string& param = "");
    void handleTopic(Client& requester, const std::string& chanName, const std::string& newTopic = "");
};

#endif
