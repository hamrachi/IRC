/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cmd_kick.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hamrachi <hamrachi@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/17 by hamrachi                   #+#    #+#             */
/*   Updated: 2025/11/17 by hamrachi                ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Commands.hpp"
#include "Server.hpp"
#include "Channel.hpp"

void cmd_kick(Server* server, Client& cli, const IRCMessage& msg) {
    if (msg.params.size() < 2) {
        server->sendLine(cli, cli.buildNumeric(server->_name, 461, cli.getNick(), "KICK :Not enough parameters"));
        return;
    }

    std::string channel = msg.params[0];
    std::string userList = msg.params[1];

    std::string reason = "No reason given";
    if (msg.params.size() > 2) {
        reason = msg.params[2];
    }

    std::map<std::string, Channel>::iterator it = server->_channels.find(channel);
    if (it == server->_channels.end()) {
        server->sendLine(cli, cli.buildNumeric(server->_name, 403, cli.getNick(), channel + " :No such channel"));
        return;
    }

    Channel &chan = it->second;

    if (!chan.hasClient(cli.getFd())) {
        server->sendLine(cli, cli.buildNumeric(server->_name, 442, cli.getNick(), channel + " :You're not on that channel"));
        return;
    }

    if (!chan.isOperator(cli.getFd())) {
        server->sendLine(cli, cli.buildNumeric(server->_name, 482, cli.getNick(), channel + " :You're not channel operator"));
        return;
    }

    std::vector<std::string> users;
    std::string cur;
    for (size_t i = 0; i < userList.size(); ++i) {
        if (userList[i] == ',') {
            if (!cur.empty()) { users.push_back(cur); cur.clear(); }
        } else {
            cur.push_back(userList[i]);
        }
    }
    if (!cur.empty()) users.push_back(cur);

    for (size_t ui = 0; ui < users.size(); ++ui) {
        std::string nick = users[ui];
        if (nick.empty()) continue;

        int targetFd = -1;
        for (std::map<int, Client>::iterator cit = server->_clients.begin(); cit != server->_clients.end(); ++cit) {
            if (cit->second.getNick() == nick) {
                targetFd = cit->first;
                break;
            }
        }
        if (targetFd < 0) {
            server->sendLine(cli, cli.buildNumeric(server->_name, 401, cli.getNick(), nick + " :No such nick"));
            continue;
        }

        if (!chan.hasClient(targetFd)) {
            server->sendLine(cli, cli.buildNumeric(server->_name, 441, cli.getNick(), nick + " " + channel + " :They aren't on that channel"));
            continue;
        }

        const std::set<int>& members = chan.getClients();
        for (std::set<int>::const_iterator mit = members.begin(); mit != members.end(); ++mit) {
            Client &member = server->_clients[*mit];
            server->sendLine(member, ":" + cli.getNick() + "!" + cli.getUser() + "@" + cli.getIp() +
                                 " KICK " + channel + " " + nick + " :" + reason);
        }

        chan.removeClient(targetFd);
    }

    if (chan.getClients().empty()) {
        server->_channels.erase(channel);
    }
}
