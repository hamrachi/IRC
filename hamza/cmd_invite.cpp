/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cmd_invite.cpp                                     :+:      :+:    :+:   */
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

void cmd_invite(Server* server, Client& cli, const IRCMessage& msg) {
    if (msg.params.size() < 2) {
        server->sendLine(cli, cli.buildNumeric(server->_name, 461, cli.getNick(), "INVITE :Not enough parameters"));
        return;
    }

    std::string nick = msg.params[0];
    std::string channel = msg.params[1];

    int targetFd = -1;
    for (std::map<int, Client>::iterator it = server->_clients.begin(); it != server->_clients.end(); ++it) {
        if (it->second.getNick() == nick) {
            targetFd = it->first;
            break;
        }
    }
    if (targetFd < 0) {
        server->sendLine(cli, cli.buildNumeric(server->_name, 401, cli.getNick(), nick + " :No such nick"));
        return;
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

    if (chan.hasClient(targetFd)) {
        server->sendLine(cli, cli.buildNumeric(server->_name, 443, cli.getNick(), nick + " " + channel + " :is already on channel"));
        return;
    }

    if (chan.getInviteOnly() && !chan.isOperator(cli.getFd())) {
        server->sendLine(cli, cli.buildNumeric(server->_name, 482, cli.getNick(), channel + " :You're not channel operator"));
        return;
    }

    chan.addInvite(targetFd);

    server->sendLine(cli, cli.buildNumeric(server->_name, 341, cli.getNick(), nick + " " + channel));

    server->sendLine(server->_clients[targetFd], ":" + cli.getNick() + "!" + cli.getUser() + "@" + cli.getIp() +
                         " INVITE " + nick + " " + channel);
}
