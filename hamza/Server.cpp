/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hamrachi <hamrachi@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/26 18:32:13 by hamrachi          #+#    #+#             */
/*   Updated: 2025/09/06 22:01:45 by hamrachi         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <arpa/inet.h> //inet_ntoa 

Server::Server(const std::string &name, int port, const std::string &pass) : _listenFd(-1), _name(name),
 _port(port), _passWord(pass){
             // _listenFd;
             // _port;
                            };



void Server::handleMessage(Client& cli, const IRCMessage& m) {
    const std::string cmd = m.command;

    // ===== PASS =====
    if (cmd == "PASS") 
    {
        if (cli.getRegistered()) 
        {
            sendLine(cli, cli.buildNumeric(_name, 462, cli.getNick(), "You may not reregister"));
            return;
        }
        if (m.params.empty()) 
        {
            sendLine(cli, cli.buildNumeric(_name, 461, "*", "PASS :Not enough parameters"));
            return;
        }
        if (m.params[0] == _passWord) 
        {
            cli.setPassOk(true);
        } 
        else 
        {
            sendLine(cli, cli.buildNumeric(_name, 464, "*", "Password incorrect"));
            removeClient(cli.getFd());
            return;
        }
        if (cli.tryFinishRegistration()) 
        {
            sendWelcome(cli);
        }
        return;
    }

    // ===== NICK =====
    if (cmd == "NICK") 
    {
        if (m.params.empty()) 
        {
            sendLine(cli, cli.buildNumeric(_name, 461, "*", "NICK :Not enough parameters"));
            return;
        }
        // TODO: check validity/uniqueness
        cli.setNick(m.params[0]);
        if (cli.tryFinishRegistration()) 
        {
            sendWelcome(cli);
        }
        return;
    }

    // ===== USER =====
    if (cmd == "USER") {
        if (m.params.size() < 4) {
            sendLine(cli, cli.buildNumeric(_name, 461, "*", "USER :Not enough parameters"));
            return;
        }
        // USER <username> 0 * :<realname>
        cli.setUser(m.params[0], m.params[3]);
        if (cli.tryFinishRegistration()) {
            sendWelcome(cli);
        }
        return;
    }

    // ===== block non-auth commands until registered =====
    if (!cli.getRegistered()) {
        sendLine(cli, cli.buildNumeric(_name, 451, "*", "You have not registered"));
        return;
    }

    // ===== JOIN =====
    if (cmd == "JOIN") {
        // TODO: joinChannel(cli, m.params);
        return;
    }

    // ===== PRIVMSG =====
    if (cmd == "PRIVMSG") {
        // TODO: privmsg(cli, m.params);
        return;
    }

    // ===== KICK / INVITE / TOPIC / MODE =====
    if (cmd == "KICK") {
        // TODO
        return;
    }
    if (cmd == "INVITE") {
        // TODO
        return;
    }
    if (cmd == "TOPIC") {
        // TODO
        return;
    }
    if (cmd == "MODE") {
        // TODO
        return;
    }

    // ===== Unknown =====
    sendLine(cli, cli.buildNumeric(_name, 421, cli.getNick(), cmd + " :Unknown command"));
}


void Server::removeClient(int fd) {
    ::close(fd);
    _clients.erase(fd); // remove from map
    
    for (size_t i = 0; i < pfds.size(); ++i) { //remove frome pfds
        if (pfds[i].fd == fd) {
            pfds.erase(pfds.begin() + i);
            break;
        }
    }
    std::cout << "Client fd=" << fd << " removed\n";
}


// void Server::receiveFromClient(int clientFd)
// {
//     char buffer[4000];
//     ssize_t bytesRead = ::recv(clientFd, buffer, sizeof(buffer) - 1, 0);

//     if (bytesRead < 0)
//     {
//         std::cerr << "Error reading from client fd=" << clientFd << std::endl;
//         ::close(clientFd);
//     }
//     else if (bytesRead == 0)
//     {
//         std::cout << "Client fd=" << clientFd << " disconnected" << std::endl;
//         ::close(clientFd);
//     }
//     else
//     {
//         buffer[bytesRead] = '\0';
//         std::cout << "Message from fd=" << clientFd << ": " << buffer << std::endl;
//     }
// }

// Server.cpp

void Server::receiveFromClient(int clientFd)
{
    char buffer[4096];
    memset(&buffer, 0 , sizeof(buffer)); // أكبر شوية باش نقلّلو عدد الـrecv
    ssize_t bytesRead = ::recv(clientFd, buffer, sizeof(buffer), 0);

    if (bytesRead > 0)
    {
        Client &cli = _clients[clientFd];
        std::vector<std::string> lines;
        printf("this id buffer  %s\n ", buffer);
        cli.feed(buffer, static_cast<size_t>(bytesRead), lines);
    
        for (size_t i = 0; i < lines.size(); ++i) 
        {
            if (lines[i].empty()) 
                continue; // طنّش الفارغ
            //std::cout << "[RAW fd=" << clientFd << "] " << lines[i] << std::endl;
            IRCMessage msg = cli.parseLine(lines[i]);
            // Print command
                //std::cout << "[PARSE] command = " << msg.command << std::endl;

                // Print params
            size_t p = 0;
            while (p < msg.params.size())  
            {
                //std::cout << "[PARSE] param[" << p << "] = " << msg.params[p] << std::endl;
                ++p;
            }
            handleMessage(cli,msg);

            
            // ↓ لاحقاً: غادي نديرو parse + handleMessage(...)
            // IRCMessage msg = Client::parseLine(lines[i]);
            // handleMessage(cli, msg);
        }
    }
    else if (bytesRead == 0)
    {
        std::cout << "Client fd=" << clientFd << " disconnected" << std::endl;
        removeClient(clientFd);
    }
    else // bytesRead < 0
    {
        std::cerr << "Error reading from client fd=" << clientFd << std::endl;
        removeClient(clientFd);
    }
}



void Server::acceptNewClient(std::vector<struct pollfd> &pfds) {
    
    socklen_t cl = sizeof(_claddr);

    int cfd = ::accept(_listenFd, (sockaddr *)&_claddr, &cl);
    if (cfd < 0)
        throw std::runtime_error("accept() failed");

    ::fcntl(cfd, F_SETFL, O_NONBLOCK);

    std::string ip = inet_ntoa(_claddr.sin_addr);   // "192.168.1.5"  ip of client 

    _clients[cfd] = Client(cfd, ip);

    std::cout << "new client: fd=" << cfd << " ip=" << ip << std::endl;

    struct pollfd c;
    c.fd = cfd;
    c.events = POLLIN;
    c.revents = 0;
    pfds.push_back(c);
}


void Server::run()
{

    try
    {
        struct pollfd lp;
        lp.fd = _listenFd;
        lp.events = POLLIN;
        lp.revents = 0;
        pfds.push_back(lp);
        std::cout << _listenFd << std::endl;
        while (true)
        {
            int ret = ::poll(&pfds[0], pfds.size(), -1);
            // hnaya kaytfrizza kaytsna server ikon pollin lihiya jayah chi datat mn socket okher
            //std::cout <<"DAZ LFREEZ"<<std::endl;
            if (ret < 0)
            {
                throw std::runtime_error("poll() failed");
            }
            size_t i = 0;
            while (i < pfds.size())
            {
                if (pfds[i].revents & POLLIN)
                {
                    if (pfds[i].fd == _listenFd)
                    {
                       // printf("lawla\n");
                        acceptNewClient(pfds);
                    }
                    else
                    {
                       // printf("tanya\n");
                        receiveFromClient(pfds[i].fd);
                    }
                }
                ++i;
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error in run: " << e.what() << std::endl;
    }
};

void Server::initSocket()
{
    try
    {
        _listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (_listenFd < 0)
            throw std::runtime_error("socket() failed");

        int yes = 1;
        if (::setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
            throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");

        if (::fcntl(_listenFd, F_SETFL, O_NONBLOCK) < 0)
            throw std::runtime_error("fcntl(O_NONBLOCK) failed");

        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY); // Bind to any address
        addr.sin_port = htons(_port);

        if (::bind(_listenFd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0)
            throw std::runtime_error("bind() failed");

        if (::listen(_listenFd, SOMAXCONN) < 0)
            throw std::runtime_error("listen() failed");

        std::cout << "Server listening on port " << _port << " (non-blocking)\n";
    }
    catch (const std::exception &e)
    {
        if (_listenFd >= 0)
        {
            ::close(_listenFd);
            _listenFd = -1;
        }
        throw;
    }
};
