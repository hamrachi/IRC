/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cmd_topic.cpp                                      :+:      :+:    :+:   */
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
#include <sstream>

void cmd_topic(Server* server, Client& cli, const IRCMessage& msg) {
    if (msg.params.empty()) {
        server->sendLine(cli, cli.buildNumeric(server->_name, 461, cli.getNick(), "TOPIC :Not enough parameters"));
        return;
    }

    std::string channel = msg.params[0];
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

    // If only channel param -> query current topic
    if (msg.params.size() == 1) {
        std::string topic = chan.getTopic();
        if (topic.empty()) {
            server->sendLine(cli, cli.buildNumeric(server->_name, 331, cli.getNick(), channel + " :No topic is set"));
        } else {
            server->sendLine(cli, cli.buildNumeric(server->_name, 332, cli.getNick(), channel + " :" + topic));
            std::ostringstream oss;
            oss << chan.getTopicTime();
            std::string timeStr = oss.str();
            server->sendLine(cli, cli.buildNumeric(server->_name, 333, cli.getNick(),
                                                   channel + " " + chan.getTopicWho() + " " + timeStr));
        }
        return;
    }

    // Otherwise, client is attempting to set (or clear) the topic.
    // Accept both forms: "TOPIC #chan newtopic" and "TOPIC #chan :newtopic".
    if (chan.getTopicOnlyOps() && !chan.isOperator(cli.getFd())) {
        server->sendLine(cli, cli.buildNumeric(server->_name, 482, cli.getNick(), channel + " :You're not channel operator"));
        return;
    }

    std::string newTopic = "";
    if (msg.params.size() > 1) {
        newTopic = msg.params[1];
    }

    // Set topic (empty string clears it)
    chan.setTopic(newTopic, cli.getNick());

    const std::set<int>& members = chan.getClients();
    for (std::set<int>::const_iterator mit = members.begin(); mit != members.end(); ++mit) {
        Client &member = server->_clients[*mit];
        if (newTopic.empty()) {
            server->sendLine(member, ":" + cli.getNick() + "!" + cli.getUser() + "@" + cli.getIp() +
                                 " TOPIC " + channel);
        } else {
            server->sendLine(member, ":" + cli.getNick() + "!" + cli.getUser() + "@" + cli.getIp() +
                                 " TOPIC " + channel + " :" + newTopic);
        }
    }
}
