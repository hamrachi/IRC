/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cmd_privmsg.cpp                                    :+:      :+:    :+:   */
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

void cmd_privmsg(Server* server, Client& cli, const IRCMessage& msg) {
    if (msg.params.empty()) {
        server->sendLine(cli, cli.buildNumeric(server->_name, 411, cli.getNick(), "PRIVMSG :No recipient specified"));
        return;
    }
    if (msg.params.size() < 2) {
        server->sendLine(cli, cli.buildNumeric(server->_name, 412, cli.getNick(), "PRIVMSG :No text to send"));
        return;
    }

    const std::string targetList = msg.params[0];
    const std::string text   = msg.params[1];

    std::vector<std::string> targets;
    std::string cur;
    for (size_t i = 0; i < targetList.size(); ++i) {
        if (targetList[i] == ',') {
            if (!cur.empty()) { targets.push_back(cur); cur.clear(); }
        } else {
            cur.push_back(targetList[i]);
        }
    }
    if (!cur.empty()) targets.push_back(cur);

    for (size_t ti = 0; ti < targets.size(); ++ti) {
        std::string target = targets[ti];
        if (target.empty()) continue;

        while (!target.empty() && (target[0] == '@' || target[0] == '%' || target[0] == '+' || target[0] == '~')) {
            target.erase(0,1);
        }

        if (!target.empty() && (target[0] == '#' || target[0] == '&')) {
            std::map<std::string, Channel>::iterator it = server->_channels.find(target);
            if (it == server->_channels.end()) {
                server->sendLine(cli, cli.buildNumeric(server->_name, 403, cli.getNick(), target + " :No such channel"));
                continue;
            }

            Channel &chan = it->second;

            if (!chan.hasClient(cli.getFd())) {
                server->sendLine(cli, cli.buildNumeric(server->_name, 404, cli.getNick(), target + " :Cannot send to channel"));
                continue;
            }

            const std::set<int>& members = chan.getClients();
            for (std::set<int>::const_iterator mit = members.begin(); mit != members.end(); ++mit) {
                if (*mit == cli.getFd()) continue;
                Client &other = server->_clients[*mit];
                server->sendLine(other, ":" + cli.getNick() + "!" + cli.getUser() + "@" + cli.getIp() +
                                     " PRIVMSG " + target + " :" + text);
            }
            continue;
        }

        int foundFd = -1;
        for (std::map<int, Client>::iterator it = server->_clients.begin(); it != server->_clients.end(); ++it) {
            if (it->second.getNick() == target) {
                foundFd = it->first;
                break;
            }
        }
        if (foundFd < 0) {
            server->sendLine(cli, cli.buildNumeric(server->_name, 401, cli.getNick(), target + " :No such nick"));
            continue;
        }

        server->sendLine(server->_clients[foundFd], ":" + cli.getNick() + "!" + cli.getUser() + "@" + cli.getIp() +
                                  " PRIVMSG " + target + " :" + text);
    }
}
