/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hamrachi <hamrachi@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/26 18:32:13 by hamrachi          #+#    #+#             */
/*   Updated: 2025/09/09 20:00:00 by hamrachi         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <cctype>
#include <sstream> // for stringstream

Server::Server(const std::string &name, int port, const std::string &pass)
: _listenFd(-1), _name(name), _port(port), _passWord(pass) {}

/* nickname validation */
bool Server::isValidNick(const std::string& s) const {
    if (s.empty()) return false;

    char c0 = s[0];
    if (c0 == '#' || c0 == ':' || c0 == ' ') return false;

    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char ch = static_cast<unsigned char>(s[i]);
        if (std::isalnum(ch)) continue;
        if (ch == '[' || ch == ']' || ch == '{' || ch == '}' ||
            ch == '\\' || ch == '|' || ch == '-') continue;
        if (ch == ' ') return false;
        return false;
    }
    return true;
}

bool Server::isNickTaken(const std::string& s, int exceptFd) const {
    for (std::map<int, Client>::const_iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->first == exceptFd) continue;
        const std::string& other = it->second.getNick();
        if (!other.empty() && other == s) return true;
    }
    return false;
}

void Server::sendWelcome(Client& cli) {
    const std::string nick = cli.getNick();

    sendLine(cli, cli.buildNumeric(_name, 001, nick, "Welcome to the ft_irc server, " + nick));
    sendLine(cli, cli.buildNumeric(_name, 002, nick, "Your host is " + _name));
    sendLine(cli, cli.buildNumeric(_name, 003, nick, "This server was created just now"));
    sendLine(cli, cli.buildNumeric(_name, 004, nick, _name + " ft_irc o o"));

    sendLine(cli, ":" + _name + " 375 " + nick + " :- Message of the Day -");
    sendLine(cli, ":" + _name + " 372 " + nick + " :- Welcome to ft_irc!");
    sendLine(cli, ":" + _name + " 376 " + nick + " :End of /MOTD command.");
}

void Server::sendLine(Client& cli, const std::string& line) {
    cli.appendSend(line);

    int fd = cli.getFd();
    for (size_t i = 0; i < pfds.size(); ++i) {
        if (pfds[i].fd == fd) {
            pfds[i].events |= POLLOUT;
            break;
        }
    }
}

void Server::handleMessage(Client& cli, const IRCMessage& m) {
    const std::string cmd = m.command;
    const bool passRequired = !_passWord.empty();

    /* ===== PASS ===== */
    if (cmd == "PASS") {
        if (cli.getRegistered()) {
            sendLine(cli, cli.buildNumeric(_name, 462, cli.getNick(), "You may not reregister"));
            return;
        }
        if (m.params.empty()) {
            sendLine(cli, cli.buildNumeric(_name, 461, "*", "PASS :Not enough parameters"));
            return;
        }
        if (m.params[0] == _passWord) {
            cli.setPassOk(true);
        } else {
            cli.setPassOk(false);
            sendLine(cli, cli.buildNumeric(_name, 464, "*", "Password incorrect"));
            return;
        }
        if (cli.tryFinishRegistration()) sendWelcome(cli);
        return;
    }

    /* ===== NICK ===== */
    if (cmd == "NICK") {
        if (m.params.empty()) {
            sendLine(cli, cli.buildNumeric(_name, 431, "*", "No nickname given"));
            return;
        }
        if (m.params.size() != 1) {
            sendLine(cli, cli.buildNumeric(_name, 432, "*", "Erroneous nickname"));
            return;
        }

        const std::string newNick = m.params[0];

        if (!isValidNick(newNick)) {
            sendLine(cli, ":" + _name + " 432 * " + newNick + " :Erroneous nickname");
            return;
        }
        if (isNickTaken(newNick, cli.getFd())) {
            sendLine(cli, ":" + _name + " 433 * " + newNick + " :Nickname is already in use");
            return;
        }

        if (passRequired && !cli.getPassOk()) {
            cli.setNick(newNick);
            sendLine(cli, cli.buildNumeric(_name, 464, "*", "Password required before registration"));
            return;
        }

        cli.setNick(newNick);
        if (cli.tryFinishRegistration()) sendWelcome(cli);
        return;
    }

    /* ===== USER ===== */
    if (cmd == "USER") {
        if (cli.getRegistered()) {
            sendLine(cli, cli.buildNumeric(_name, 462, cli.getNick(), "You may not reregister"));
            return;
        }
        if (m.params.size() < 4) {
            sendLine(cli, cli.buildNumeric(_name, 461, "*", "USER :Not enough parameters"));
            return;
        }

        const std::string username = m.params[0];
        const std::string realname = m.params[m.params.size() - 1];

        cli.setUser(username, realname);

        if (passRequired && !cli.getPassOk()) {
            sendLine(cli, cli.buildNumeric(_name, 464, "*", "Password required before registration"));
            return;
        }

        if (cli.tryFinishRegistration()) sendWelcome(cli);
        return;
    }

    /* block non-auth commands until registered */
    if (!cli.getRegistered()) {
        sendLine(cli, cli.buildNumeric(_name, 451, "*", "You have not registered"));
        return;
    }

    /* ===== JOIN ===== */
    if (cmd == "JOIN") {
        if (m.params.empty()) {
            sendLine(cli, cli.buildNumeric(_name, 461, cli.getNick(), "JOIN :Not enough parameters"));
            return;
        }

        std::string chanName = m.params[0];
        if (chanName.empty()) {
            sendLine(cli, cli.buildNumeric(_name, 461, cli.getNick(), "JOIN :Not enough parameters"));
            return;
        }
        if (chanName[0] != '#') chanName = "#" + chanName;

        Channel &chan = _channels[chanName];
        if (chan.getName().empty()) {
            // default-constructed; initialize it
            chan = Channel(chanName);
        }

        chan.addClient(cli.getFd());

        // announce to the joiner
        sendLine(cli, ":" + cli.getNick() + "!" + cli.getNick() + "@" + cli.getIp() +
                      " JOIN " + chanName);

        if (!chan.getTopic().empty()) {
            sendLine(cli, cli.buildNumeric(_name, 332, cli.getNick(), chanName + " :" + chan.getTopic()));
        } else {
            sendLine(cli, cli.buildNumeric(_name, 331, cli.getNick(), chanName + " :No topic is set"));
        }

        std::string names;
        const std::set<int>& members = chan.getClients();
        for (std::set<int>::const_iterator it = members.begin(); it != members.end(); ++it) {
            names += _clients[*it].getNick() + " ";
        }
        sendLine(cli, cli.buildNumeric(_name, 353, cli.getNick(), "= " + chanName + " :" + names));
        sendLine(cli, cli.buildNumeric(_name, 366, cli.getNick(), chanName + " :End of NAMES list"));
        return;
    }

    /* ===== PRIVMSG ===== */
    if (cmd == "PRIVMSG") {
        if (m.params.size() < 2) {
            sendLine(cli, cli.buildNumeric(_name, 461, cli.getNick(), "PRIVMSG :Not enough parameters"));
            return;
        }

        const std::string target = m.params[0];
        const std::string text   = m.params[1];

        // channel target
        if (!target.empty() && target[0] == '#') {
            std::map<std::string, Channel>::iterator it = _channels.find(target);
            if (it == _channels.end()) {
                sendLine(cli, cli.buildNumeric(_name, 403, cli.getNick(), target + " :No such channel"));
                return;
            }

            Channel &chan = it->second;
            if (!chan.hasClient(cli.getFd())) {
                sendLine(cli, cli.buildNumeric(_name, 442, cli.getNick(), target + " :You're not on that channel"));
                return;
            }

            const std::set<int>& members = chan.getClients();
            for (std::set<int>::const_iterator mit = members.begin(); mit != members.end(); ++mit) {
                if (*mit == cli.getFd()) continue;
                Client &other = _clients[*mit];
                sendLine(other, ":" + cli.getNick() + "!" + cli.getNick() + "@" + cli.getIp() +
                                  " PRIVMSG " + target + " :" + text);
            }
            return;
        }

        // user target
        for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
            if (it->second.getNick() == target) {
                sendLine(it->second, ":" + cli.getNick() + "!" + cli.getNick() + "@" + cli.getIp() +
                                      " PRIVMSG " + target + " :" + text);
                return;
            }
        }

        sendLine(cli, cli.buildNumeric(_name, 401, cli.getNick(), target + " :No such nick"));
        return;
    }

    /* not implemented */
    sendLine(cli, cli.buildNumeric(_name, 421, cli.getNick(), cmd + " :Unknown command"));
}

/* INVITE */
void Server::handleInvite(Client& requester, const std::string& chanName, const std::string& nickTarget) {
    if (_channels.find(chanName) == _channels.end()) {
        sendLine(requester, requester.buildNumeric(_name, 403, requester.getNick(), chanName + " :No such channel"));
        return;
    }
    Channel& chan = _channels[chanName];

    if (!chan.isOperator(requester.getFd())) {
        sendLine(requester, requester.buildNumeric(_name, 482, requester.getNick(), chanName + " :You're not channel operator"));
        return;
    }

    int targetFd = -1;
    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->second.getNick() == nickTarget) {
            targetFd = it->first;
            break;
        }
    }
    if (targetFd < 0) {
        sendLine(requester, requester.buildNumeric(_name, 401, requester.getNick(), nickTarget + " :No such nick"));
        return;
    }

    chan.addClient(targetFd);
    sendLine(_clients[targetFd], ":" + requester.getNick() + "!" + requester.getNick() + "@" + requester.getIp() +
                              " INVITE " + nickTarget + " :" + chanName);
}

/* KICK */
void Server::handleKick(Client& requester, const std::string& chanName, const std::string& nickTarget, const std::string& reason) {
    if (_channels.find(chanName) == _channels.end()) {
        sendLine(requester, requester.buildNumeric(_name, 403, requester.getNick(), chanName + " :No such channel"));
        return;
    }
    Channel& chan = _channels[chanName];

    if (!chan.isOperator(requester.getFd())) {
        sendLine(requester, requester.buildNumeric(_name, 482, requester.getNick(), chanName + " :You're not channel operator"));
        return;
    }

    int targetFd = -1;
    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->second.getNick() == nickTarget) {
            targetFd = it->first;
            break;
        }
    }
    if (targetFd < 0 || !chan.hasClient(targetFd)) {
        sendLine(requester, requester.buildNumeric(_name, 441, requester.getNick(), nickTarget + " " + chanName + " :They aren't on that channel"));
        return;
    }

    chan.removeClient(targetFd);
    sendLine(_clients[targetFd], ":" + requester.getNick() + "!" + requester.getNick() + "@" + requester.getIp() +
                              " KICK " + chanName + " " + nickTarget + " :" + reason);
}

/* MODE */
void Server::handleMode(Client& requester, const std::string& chanName, const std::string& modes, const std::string& param) {
    if (_channels.find(chanName) == _channels.end()) {
        sendLine(requester, requester.buildNumeric(_name, 403, requester.getNick(), chanName + " :No such channel"));
        return;
    }
    Channel& chan = _channels[chanName];

    if (!chan.isOperator(requester.getFd())) {
        sendLine(requester, requester.buildNumeric(_name, 482, requester.getNick(), chanName + " :You're not channel operator"));
        return;
    }

    for (size_t i = 0; i < modes.size(); ++i) {
        char c = modes[i];
        switch (c) {
            case 'i': chan.setInviteOnly(true); break;
            case 't': chan.setTopicOnlyOps(true); break;
            case 'k': if (!param.empty()) chan.setKey(param); break;
            case 'l': 
                if (!param.empty()) {
                    std::stringstream ss(param);
                    int limit = 0;
                    ss >> limit;
                    chan.setLimit(limit);
                }
                break;
            case 'o': {
                int targetFd = -1;
                for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
                    if (it->second.getNick() == param) { targetFd = it->first; break; }
                }
                if (targetFd >= 0) chan.addOperator(targetFd);
                break;
            }
        }
    }

    sendLine(requester, ":" + requester.getNick() + "!" + requester.getNick() + "@" + requester.getIp() +
                     " MODE " + chanName + " " + modes + " " + param);
}

/* TOPIC */
void Server::handleTopic(Client& requester, const std::string& chanName, const std::string& newTopic) {
    if (_channels.find(chanName) == _channels.end()) {
        sendLine(requester, requester.buildNumeric(_name, 403, requester.getNick(), chanName + " :No such channel"));
        return;
    }
    Channel& chan = _channels[chanName];

    if (!chan.hasClient(requester.getFd())) {
        sendLine(requester, requester.buildNumeric(_name, 442, requester.getNick(), chanName + " :You're not on that channel"));
        return;
    }

    if (!newTopic.empty()) {
        if (chan.getTopicOnlyOps() && !chan.isOperator(requester.getFd())) {
            sendLine(requester, requester.buildNumeric(_name, 482, requester.getNick(), chanName + " :You're not channel operator"));
            return;
        }
        chan.setTopic(newTopic);

        const std::set<int>& members = chan.getClients();
        for (std::set<int>::const_iterator it = members.begin(); it != members.end(); ++it) {
            int fd = *it;
            sendLine(_clients[fd], ":" + requester.getNick() + "!" + requester.getNick() + "@" + requester.getIp() +
                              " TOPIC " + chanName + " :" + newTopic);
        }
    } else {
        if (chan.getTopic().empty())
            sendLine(requester, requester.buildNumeric(_name, 331, requester.getNick(), chanName + " :No topic is set"));
        else
            sendLine(requester, requester.buildNumeric(_name, 332, requester.getNick(), chanName + " :" + chan.getTopic()));
    }
}

/* REMOVE CLIENT */
void Server::removeClient(int fd) {
    ::close(fd);
    _clients.erase(fd);

    for (size_t i = 0; i < pfds.size(); ++i) {
        if (pfds[i].fd == fd) {
            pfds.erase(pfds.begin() + i);
            break;
        }
    }
    std::cout << "Client fd=" << fd << " removed\n";
}

/* RECEIVE DATA */
void Server::receiveFromClient(int clientFd) {
    char buffer[4096];
    std::memset(&buffer, 0, sizeof(buffer));
    ssize_t bytesRead = ::recv(clientFd, buffer, sizeof(buffer), 0);

    if (bytesRead > 0) {
        Client &cli = _clients[clientFd];
        std::vector<std::string> lines;
        cli.feed(buffer, static_cast<size_t>(bytesRead), lines);

        for (size_t i = 0; i < lines.size(); ++i) {
            if (lines[i].empty()) continue;
            IRCMessage msg = cli.parseLine(lines[i]);
            handleMessage(cli, msg);
        }
    } else if (bytesRead == 0) {
        std::cout << "Client fd=" << clientFd << " disconnected" << std::endl;
        removeClient(clientFd);
    } else {
        std::cerr << "Error reading from client fd=" << clientFd << std::endl;
        removeClient(clientFd);
    }
}

/* ACCEPT NEW CLIENT */
void Server::acceptNewClient(std::vector<struct pollfd> &pfds) {
    socklen_t cl = sizeof(_claddr);
    int cfd = ::accept(_listenFd, (sockaddr *)&_claddr, &cl);
    if (cfd < 0) throw std::runtime_error("accept() failed");

    ::fcntl(cfd, F_SETFL, O_NONBLOCK);

    std::string ip = inet_ntoa(_claddr.sin_addr);
    _clients[cfd] = Client(cfd, ip);

    struct pollfd c;
    c.fd = cfd;
    c.events = POLLIN;
    c.revents = 0;
    pfds.push_back(c);
}

/* RUN SERVER */
void Server::run() {
    try {
        struct pollfd lp;
        lp.fd = _listenFd;
        lp.events = POLLIN;
        lp.revents = 0;
        pfds.push_back(lp);

        std::cout << "Server listening on fd=" << _listenFd << std::endl;

        while (true) {
            int ret = ::poll(&pfds[0], pfds.size(), -1);
            if (ret < 0) {
                throw std::runtime_error("poll() failed");
            }

            for (size_t i = 0; i < pfds.size();) {
                int fd  = pfds[i].fd;
                int rev = pfds[i].revents;
                bool removed = false;

                if ((rev & POLLERR) || (rev & POLLHUP) || (rev & POLLNVAL)) {
                    if (fd != _listenFd) {
                        removeClient(fd);
                        removed = true;
                    }
                }

                if (!removed && (rev & POLLIN)) {
                    if (fd == _listenFd) {
                        acceptNewClient(pfds);
                    } else {
                        receiveFromClient(fd);
                        if (_clients.find(fd) == _clients.end()) {
                            removed = true;
                        }
                    }
                }

                if (!removed && (rev & POLLOUT) && fd != _listenFd) {
                    Client &cli = _clients[fd];

                    while (cli.hasPending()) {
                        const char* data = cli.pendingData();
                        size_t      left = cli.pendingSize();
                        if (!data || left == 0) break;

                        ssize_t w = ::send(fd, data, left, 0);
                        if (w > 0) {
                            cli.advanceSent(static_cast<size_t>(w));
                        } else {
                            removeClient(fd);
                            removed = true;
                            break;
                        }
                    }

                    if (!removed && !cli.hasPending()) {
                        pfds[i].events &= ~POLLOUT;
                    }
                }

                if (!removed) ++i;
            }
        }
    } catch (const std::exception &e) {
        std::cerr << "Error in run(): " << e.what() << std::endl;
    }
}

/* INIT SOCKET */
void Server::initSocket() {
    try {
        _listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (_listenFd < 0) throw std::runtime_error("socket() failed");

        int yes = 1;
        if (::setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
            throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");

        if (::fcntl(_listenFd, F_SETFL, O_NONBLOCK) < 0)
            throw std::runtime_error("fcntl(O_NONBLOCK) failed");

        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(_port);

        if (::bind(_listenFd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0)
            throw std::runtime_error("bind() failed");

        if (::listen(_listenFd, SOMAXCONN) < 0)
            throw std::runtime_error("listen() failed");

        std::cout << "Server listening on port " << _port << " (non-blocking)\n";
    } catch (const std::exception &e) {
        if (_listenFd >= 0) {
            ::close(_listenFd);
            _listenFd = -1;
        }
        throw;
    }
}
