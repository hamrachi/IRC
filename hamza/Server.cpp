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

        bool chanExists = (_channels.find(chanName) != _channels.end());
        Channel &chan = _channels[chanName];
        if (chan.getName().empty()) {
            // default-constructed; initialize it
            chan = Channel(chanName);
        }

        // Check if user is already on the channel
        if (chan.hasClient(cli.getFd())) {
            return; // Already on channel, silently ignore
        }

        // Check invite-only mode (+i)
        if (chan.getInviteOnly() && !chan.isInvited(cli.getFd())) {
            sendLine(cli, cli.buildNumeric(_name, 473, cli.getNick(), chanName + " :Cannot join channel (+i)"));
            return;
        }

        // Check channel key (+k)
        if (!chan.getKey().empty()) {
            std::string providedKey;
            if (m.params.size() > 1) {
                providedKey = m.params[1];
            }
            if (providedKey != chan.getKey()) {
                sendLine(cli, cli.buildNumeric(_name, 475, cli.getNick(), chanName + " :Cannot join channel (+k)"));
                return;
            }
        }

        // Check user limit (+l)
        if (chan.getLimit() > 0 && chan.getClients().size() >= chan.getLimit()) {
            sendLine(cli, cli.buildNumeric(_name, 471, cli.getNick(), chanName + " :Cannot join channel (+l)"));
            return;
        }

        // If channel is new or empty, make the joiner an operator
        if (!chanExists || chan.getClients().empty()) {
            chan.addOperator(cli.getFd());
        }

        chan.addClient(cli.getFd());
        // Remove from invited list if they were invited
        chan.removeInvite(cli.getFd());

        // announce to the joiner
        sendLine(cli, ":" + cli.getNick() + "!" + cli.getUser() + "@" + cli.getIp() +
                      " JOIN " + chanName);

        // Send topic or no topic
        if (!chan.getTopic().empty()) {
            sendLine(cli, cli.buildNumeric(_name, 332, cli.getNick(), chanName + " :" + chan.getTopic()));
        } else {
            sendLine(cli, cli.buildNumeric(_name, 331, cli.getNick(), chanName + " :No topic is set"));
        }

        // Send NAMES list
        std::string names;
        const std::set<int>& members = chan.getClients();
        for (std::set<int>::const_iterator it = members.begin(); it != members.end(); ++it) {
            if (chan.isOperator(*it)) {
                names += "@";
            }
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
                sendLine(other, ":" + cli.getNick() + "!" + cli.getUser() + "@" + cli.getIp() +
                                  " PRIVMSG " + target + " :" + text);
            }
            return;
        }

        // user target
        for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
            if (it->second.getNick() == target) {
                sendLine(it->second, ":" + cli.getNick() + "!" + cli.getUser() + "@" + cli.getIp() +
                                      " PRIVMSG " + target + " :" + text);
                return;
            }
        }

        sendLine(cli, cli.buildNumeric(_name, 401, cli.getNick(), target + " :No such nick"));
        return;
    }

    /* ===== PART ===== */
    if (cmd == "PART") {
        if (m.params.empty()) {
            sendLine(cli, cli.buildNumeric(_name, 461, cli.getNick(), "PART :Not enough parameters"));
            return;
        }

        std::string chanName = m.params[0];
        std::string reason;
        if (m.params.size() > 1) {
            reason = m.params[1];
        }

        std::map<std::string, Channel>::iterator it = _channels.find(chanName);
        if (it == _channels.end()) {
            sendLine(cli, cli.buildNumeric(_name, 403, cli.getNick(), chanName + " :No such channel"));
            return;
        }

        Channel &chan = it->second;
        if (!chan.hasClient(cli.getFd())) {
            sendLine(cli, cli.buildNumeric(_name, 442, cli.getNick(), chanName + " :You're not on that channel"));
            return;
        }

        // Broadcast PART to all channel members including the leaving user
        const std::set<int>& members = chan.getClients();
        for (std::set<int>::const_iterator mit = members.begin(); mit != members.end(); ++mit) {
            Client &other = _clients[*mit];
            if (reason.empty()) {
                sendLine(other, ":" + cli.getNick() + "!" + cli.getUser() + "@" + cli.getIp() +
                                  " PART " + chanName);
            } else {
                sendLine(other, ":" + cli.getNick() + "!" + cli.getUser() + "@" + cli.getIp() +
                                  " PART " + chanName + " :" + reason);
            }
        }

        chan.removeClient(cli.getFd());
        // Remove empty channels
        if (chan.getClients().empty()) {
            _channels.erase(it);
        }
        return;
    }

    /* ===== TOPIC ===== */
    if (cmd == "TOPIC") {
        if (m.params.empty()) {
            sendLine(cli, cli.buildNumeric(_name, 461, cli.getNick(), "TOPIC :Not enough parameters"));
            return;
        }

        std::string chanName = m.params[0];
        std::string newTopic;
        if (m.params.size() > 1) {
            newTopic = m.params[1];
        }

        handleTopic(cli, chanName, newTopic);
        return;
    }

    /* ===== INVITE ===== */
    if (cmd == "INVITE") {
        if (m.params.size() < 2) {
            sendLine(cli, cli.buildNumeric(_name, 461, cli.getNick(), "INVITE :Not enough parameters"));
            return;
        }

        std::string nickTarget = m.params[0];
        std::string chanName = m.params[1];

        handleInvite(cli, chanName, nickTarget);
        return;
    }

    /* ===== KICK ===== */
    if (cmd == "KICK") {
        if (m.params.size() < 2) {
            sendLine(cli, cli.buildNumeric(_name, 461, cli.getNick(), "KICK :Not enough parameters"));
            return;
        }

        std::string chanName = m.params[0];
        std::string nickTarget = m.params[1];
        std::string reason;
        if (m.params.size() > 2) {
            reason = m.params[2];
        }

        handleKick(cli, chanName, nickTarget, reason);
        return;
    }

    /* ===== MODE ===== */
    if (cmd == "MODE") {
        if (m.params.empty()) {
            sendLine(cli, cli.buildNumeric(_name, 461, cli.getNick(), "MODE :Not enough parameters"));
            return;
        }

        std::string target = m.params[0];
        
        // User mode (target is a nickname)
        if (target[0] != '#') {
            // Simple user mode response
            if (m.params.size() == 1) {
                sendLine(cli, cli.buildNumeric(_name, 221, cli.getNick(), "+"));
            }
            return;
        }

        // Channel mode
        if (m.params.size() < 2) {
            // Query mode - RPL_CHANNELMODEIS
            std::map<std::string, Channel>::iterator it = _channels.find(target);
            if (it == _channels.end()) {
                sendLine(cli, cli.buildNumeric(_name, 403, cli.getNick(), target + " :No such channel"));
                return;
            }
            // Return current modes (simplified)
            sendLine(cli, ":" + _name + " 324 " + cli.getNick() + " " + target + " +nt");
            return;
        }

        std::string modes = m.params[1];
        std::string param;
        if (m.params.size() > 2) {
            param = m.params[2];
        }

        handleMode(cli, target, modes, param);
        return;
    }

    /* ===== PING ===== */
    if (cmd == "PING") {
        if (m.params.empty()) {
            sendLine(cli, cli.buildNumeric(_name, 409, cli.getNick(), "No origin specified"));
            return;
        }
        // Reply with PONG
        sendLine(cli, ":" + _name + " PONG " + _name + " :" + m.params[0]);
        return;
    }

    /* ===== PONG ===== */
    if (cmd == "PONG") {
        // Just acknowledge, no action needed
        return;
    }

    /* ===== QUIT ===== */
    if (cmd == "QUIT") {
        std::string reason = "Client quit";
        if (!m.params.empty()) {
            reason = "Quit: " + m.params[0];
        }

        // Broadcast QUIT to all channels the user is in
        std::set<int> notifiedClients;
        for (std::map<std::string, Channel>::iterator chanIt = _channels.begin(); 
             chanIt != _channels.end(); ++chanIt) {
            Channel &chan = chanIt->second;
            if (chan.hasClient(cli.getFd())) {
                const std::set<int>& members = chan.getClients();
                for (std::set<int>::const_iterator mit = members.begin(); mit != members.end(); ++mit) {
                    if (*mit != cli.getFd() && notifiedClients.find(*mit) == notifiedClients.end()) {
                        Client &other = _clients[*mit];
                        sendLine(other, ":" + cli.getNick() + "!" + cli.getUser() + "@" + cli.getIp() +
                                          " QUIT :" + reason);
                        notifiedClients.insert(*mit);
                    }
                }
                chan.removeClient(cli.getFd());
            }
        }

        // Send ERROR to the quitting client
        sendLine(cli, "ERROR :Closing Link: " + cli.getIp() + " (" + reason + ")");
        
        // Mark for removal
        removeClient(cli.getFd());
        return;
    }

    /* not implemented */
    sendLine(cli, cli.buildNumeric(_name, 421, cli.getNick(), cmd + " :Unknown command"));
}

/* INVITE */
void Server::handleInvite(Client& requester, const std::string& chanName, const std::string& nickTarget) {
    // Find the channel
    if (_channels.find(chanName) == _channels.end()) {
        sendLine(requester, requester.buildNumeric(_name, 403, requester.getNick(), chanName + " :No such channel"));
        return;
    }
    Channel& chan = _channels[chanName];

    // Requester must be on the channel
    if (!chan.hasClient(requester.getFd())) {
        sendLine(requester, requester.buildNumeric(_name, 442, requester.getNick(), chanName + " :You're not on that channel"));
        return;
    }

    // For invite-only channels, must be operator
    if (chan.getInviteOnly() && !chan.isOperator(requester.getFd())) {
        sendLine(requester, requester.buildNumeric(_name, 482, requester.getNick(), chanName + " :You're not channel operator"));
        return;
    }

    // Find target user
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

    // Check if target is already on channel
    if (chan.hasClient(targetFd)) {
        sendLine(requester, requester.buildNumeric(_name, 443, requester.getNick(), nickTarget + " " + chanName + " :is already on channel"));
        return;
    }

    // Add to invited list (does NOT auto-join)
    chan.addInvite(targetFd);

    // Send RPL_INVITING to requester
    sendLine(requester, requester.buildNumeric(_name, 341, requester.getNick(), nickTarget + " " + chanName));

    // Send INVITE message to target
    sendLine(_clients[targetFd], ":" + requester.getNick() + "!" + requester.getUser() + "@" + requester.getIp() +
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

    // Broadcast KICK to all channel members
    const std::set<int>& members = chan.getClients();
    std::string kickMsg;
    if (reason.empty()) {
        kickMsg = ":" + requester.getNick() + "!" + requester.getUser() + "@" + requester.getIp() +
                  " KICK " + chanName + " " + nickTarget;
    } else {
        kickMsg = ":" + requester.getNick() + "!" + requester.getUser() + "@" + requester.getIp() +
                  " KICK " + chanName + " " + nickTarget + " :" + reason;
    }

    for (std::set<int>::const_iterator it = members.begin(); it != members.end(); ++it) {
        sendLine(_clients[*it], kickMsg);
    }

    chan.removeClient(targetFd);
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

    bool adding = true;
    std::string appliedModes;
    std::string appliedParams;

    for (size_t i = 0; i < modes.size(); ++i) {
        char c = modes[i];
        if (c == '+') {
            adding = true;
            continue;
        } else if (c == '-') {
            adding = false;
            continue;
        }

        switch (c) {
            case 'i': 
                chan.setInviteOnly(adding);
                appliedModes += c;
                break;
            case 't': 
                chan.setTopicOnlyOps(adding);
                appliedModes += c;
                break;
            case 'k': 
                if (adding && !param.empty()) {
                    chan.setKey(param);
                    appliedModes += c;
                    appliedParams = param;
                } else if (!adding) {
                    chan.setKey("");
                    appliedModes += c;
                }
                break;
            case 'l': 
                if (adding && !param.empty()) {
                    std::stringstream ss(param);
                    int limit = 0;
                    ss >> limit;
                    if (limit > 0) {
                        chan.setLimit(limit);
                        appliedModes += c;
                        appliedParams = param;
                    }
                } else if (!adding) {
                    chan.setLimit(0);
                    appliedModes += c;
                }
                break;
            case 'o': {
                if (!param.empty()) {
                    int targetFd = -1;
                    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
                        if (it->second.getNick() == param) { 
                            targetFd = it->first; 
                            break; 
                        }
                    }
                    if (targetFd >= 0 && chan.hasClient(targetFd)) {
                        if (adding) {
                            chan.addOperator(targetFd);
                        } else {
                            chan.removeOperator(targetFd);
                        }
                        appliedModes += c;
                        appliedParams = param;
                    }
                }
                break;
            }
            default:
                // Unknown mode, ignore
                break;
        }
    }

    // Broadcast MODE change to all channel members
    if (!appliedModes.empty()) {
        std::string modeStr = (adding ? "+" : "-") + appliedModes;
        if (!appliedParams.empty()) {
            modeStr += " " + appliedParams;
        }

        const std::set<int>& members = chan.getClients();
        for (std::set<int>::const_iterator it = members.begin(); it != members.end(); ++it) {
            sendLine(_clients[*it], ":" + requester.getNick() + "!" + requester.getUser() + "@" + requester.getIp() +
                              " MODE " + chanName + " " + modeStr);
        }
    }
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

    // If newTopic is provided, set it
    if (!newTopic.empty()) {
        if (chan.getTopicOnlyOps() && !chan.isOperator(requester.getFd())) {
            sendLine(requester, requester.buildNumeric(_name, 482, requester.getNick(), chanName + " :You're not channel operator"));
            return;
        }
        chan.setTopic(newTopic);

        // Broadcast TOPIC change to all channel members
        const std::set<int>& members = chan.getClients();
        for (std::set<int>::const_iterator it = members.begin(); it != members.end(); ++it) {
            int fd = *it;
            sendLine(_clients[fd], ":" + requester.getNick() + "!" + requester.getUser() + "@" + requester.getIp() +
                              " TOPIC " + chanName + " :" + newTopic);
        }
    } else {
        // Query topic
        if (chan.getTopic().empty())
            sendLine(requester, requester.buildNumeric(_name, 331, requester.getNick(), chanName + " :No topic is set"));
        else
            sendLine(requester, requester.buildNumeric(_name, 332, requester.getNick(), chanName + " :" + chan.getTopic()));
    }
}

/* REMOVE CLIENT */
void Server::removeClient(int fd) {
    // Remove client from all channels
    for (std::map<std::string, Channel>::iterator it = _channels.begin(); it != _channels.end(); ) {
        it->second.removeClient(fd);
        // Remove empty channels
        if (it->second.getClients().empty()) {
            std::map<std::string, Channel>::iterator toErase = it;
            ++it;
            _channels.erase(toErase);
        } else {
            ++it;
        }
    }

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
